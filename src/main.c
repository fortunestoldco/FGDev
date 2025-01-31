/**
 * @file main.c
 * @brief Plant Monitor Firmware
 * @version 1.0
 * @date 2025-01-31
 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/adc.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/logging/log.h>
#include <zephyr/settings/settings.h>
#include <zephyr/fs/fs.h>
#include <zephyr/net/mqtt.h>
#include <zephyr/random/random.h>
#include <zephyr/net/socket.h>
#include <string.h>
#include "config.h"
#include "../drivers/soil_moisture_sensor.h"
#include "../drivers/max17043_driver.h"

LOG_MODULE_REGISTER(main, CONFIG_APP_LOG_LEVEL);

#define SLEEP_TIME_MS   300000  // 5 minutes

// Device pointers
static const struct device *adc_dev;
static const struct device *i2c_dev;
static const struct device *button_dev;

// MQTT client instance
static struct mqtt_client client;

// UUID storage
static char uuid_str[37] = {0};  // 36 chars + null terminator

// Work queue for publishing data
static void publish_work_handler(struct k_work *work);
static K_WORK_DELAYABLE_DEFINE(publish_work, publish_work_handler);

static int init_adc(void)
{
    int err;
    
    adc_dev = DEVICE_DT_GET(DT_NODELABEL(adc0));
    if (!device_is_ready(adc_dev)) {
        LOG_ERR("ADC device not ready");
        return -ENODEV;
    }

    struct adc_channel_cfg channel_cfg = {
        .gain = ADC_GAIN_1,
        .reference = ADC_REF_INTERNAL,
        .acquisition_time = ADC_ACQ_TIME_DEFAULT,
        .channel_id = 0,
    };

    err = adc_channel_setup(adc_dev, &channel_cfg);
    if (err < 0) {
        LOG_ERR("Failed to setup ADC channel: %d", err);
        return err;
    }

    return 0;
}

static int init_i2c(void)
{
    i2c_dev = DEVICE_DT_GET(DT_NODELABEL(i2c0));
    if (!device_is_ready(i2c_dev)) {
        LOG_ERR("I2C device not ready");
        return -ENODEV;
    }
    return 0;
}

static void button_pressed_cb(const struct device *dev, struct gpio_callback *cb,
                            uint32_t pins)
{
    LOG_INF("Button pressed, triggering immediate sensor reading");
    k_work_schedule(&publish_work, K_NO_WAIT);
}

static int init_button(void)
{
    int err;

    button_dev = DEVICE_DT_GET(DT_NODELABEL(button0));
    if (!device_is_ready(button_dev)) {
        LOG_ERR("Button device not ready");
        return -ENODEV;
    }

    err = gpio_pin_configure(button_dev, CONFIG_BUTTON_PIN,
                           GPIO_INPUT | GPIO_PULL_UP);
    if (err < 0) {
        return err;
    }

    static struct gpio_callback button_cb;
    gpio_init_callback(&button_cb, button_pressed_cb, BIT(CONFIG_BUTTON_PIN));
    err = gpio_add_callback(button_dev, &button_cb);

    return err;
}

// Settings handler
static int settings_set(const char *name, size_t len,
                       settings_read_cb read_cb, void *cb_arg)
{
    int ret;
    const char *next;

    if (settings_name_steq(name, "uuid", &next) && !next) {
        if (len >= sizeof(uuid_str)) {
            return -EINVAL;
        }
        ret = read_cb(cb_arg, uuid_str, len);
        if (ret < 0) {
            return ret;
        }
        uuid_str[len] = '\0';
    }

    return 0;
}

static struct settings_handler settings_conf = {
    .name = STORAGE_NAMESPACE,
    .h_set = settings_set
};

static int read_sensors(float *soil_moisture, float *battery_level)
{
    int err;

    // Read soil moisture
    err = soil_moisture_read(i2c_dev, soil_moisture);
    if (err) {
        LOG_ERR("Failed to read soil moisture: %d", err);
        return err;
    }

    // Read battery level
    err = max17043_read(i2c_dev, battery_level);
    if (err) {
        LOG_ERR("Failed to read battery level: %d", err);
        return err;
    }

    return 0;
}

static void mqtt_evt_handler(struct mqtt_client *client,
                           const struct mqtt_evt *evt)
{
    switch (evt->type) {
    case MQTT_EVT_CONNECTED:
        LOG_INF("MQTT client connected");
        break;

    case MQTT_EVT_DISCONNECTED:
        LOG_INF("MQTT client disconnected");
        break;

    case MQTT_EVT_PUBLISH:
        LOG_INF("MQTT publish successful");
        break;

    default:
        LOG_DBG("MQTT event: %d", evt->type);
        break;
    }
}

static int init_mqtt(void)
{
    mqtt_client_init(&client);

    client.broker = (struct sockaddr_in){
        .sin_family = AF_INET,
        .sin_port = htons(CONFIG_MQTT_BROKER_PORT),
    };
    inet_pton(AF_INET, CONFIG_MQTT_BROKER_HOSTNAME, &client.broker.sin_addr);

    client.client_id.utf8 = (uint8_t *)CONFIG_MQTT_CLIENT_ID;
    client.client_id.size = strlen(CONFIG_MQTT_CLIENT_ID);
    client.protocol_version = MQTT_VERSION_3_1_1;
    client.evt_cb = mqtt_evt_handler;

    return mqtt_connect(&client);
}

static int publish_data(const char *payload)
{
    struct mqtt_publish_param param = {
        .message = {
            .topic = {
                .qos = MQTT_QOS_1_AT_LEAST_ONCE,
                .topic = {
                    .utf8 = (uint8_t *)CONFIG_MQTT_PUB_TOPIC,
                    .size = strlen(CONFIG_MQTT_PUB_TOPIC),
                },
            },
            .payload = {
                .data = payload,
                .len = strlen(payload),
            },
        },
        .message_id = k_cycle_get_32(),
    };

    return mqtt_publish(&client, &param);
}

static int cache_data(const char *payload)
{
    struct fs_file_t file;
    int ret;

    fs_file_t_init(&file);

    ret = fs_open(&file, CONFIG_CACHE_FILE_PATH,
                  FS_O_CREATE | FS_O_WRITE | FS_O_APPEND);
    if (ret < 0) {
        LOG_ERR("Failed to open cache file: %d", ret);
        return ret;
    }

    ret = fs_write(&file, payload, strlen(payload));
    if (ret < 0) {
        LOG_ERR("Failed to write to cache file: %d", ret);
        fs_close(&file);
        return ret;
    }

    fs_close(&file);
    return 0;
}

static void generate_and_store_uuid(void)
{
    if (settings_load_subtree(STORAGE_NAMESPACE) < 0) {
        LOG_WRN("Failed to load settings, generating new UUID");
    }

    if (strlen(uuid_str) == 0) {
        uint32_t r1 = sys_rand32_get();
        uint32_t r2 = sys_rand32_get();
        uint32_t r3 = sys_rand32_get();
        uint32_t r4 = sys_rand32_get();

        snprintf(uuid_str, sizeof(uuid_str),
                "%08x-%04x-%04x-%04x-%012x",
                r1, (r2 >> 16) & 0xFFFF, r2 & 0xFFFF,
                (r3 >> 16) & 0xFFFF, ((r3 & 0xFFFF) << 24) | r4);

        int ret = settings_save_one(KEY_UUID, uuid_str, strlen(uuid_str));
        if (ret) {
            LOG_ERR("Failed to store UUID: %d", ret);
        }
    }
}

static void publish_work_handler(struct k_work *work)
{
    float soil_moisture, battery_level;
    char payload[128];
    int err;

    err = read_sensors(&soil_moisture, &battery_level);
    if (err) {
        LOG_ERR("Failed to read sensors: %d", err);
        return;
    }

    snprintf(payload, sizeof(payload),
             "{\"uuid\":\"%s\",\"soil_moisture\":%.2f,\"battery\":%.2f}",
             uuid_str, soil_moisture, battery_level);

    err = publish_data(payload);
    if (err) {
        LOG_WRN("Failed to publish data: %d", err);
        err = cache_data(payload);
        if (err) {
            LOG_ERR("Failed to cache data: %d", err);
        }
    }

    k_work_schedule(&publish_work, K_MSEC(SLEEP_TIME_MS));
}

void main(void)
{
    int ret;

    LOG_INF("Starting Plant Monitor Firmware v%s", CONFIG_APP_VERSION);

    // Initialize settings subsystem
    ret = settings_subsys_init();
    if (ret) {
        LOG_ERR("Failed to initialize settings subsystem: %d", ret);
        return;
    }

    ret = settings_register(&settings_conf);
    if (ret) {
        LOG_ERR("Failed to register settings handler: %d", ret);
        return;
    }

    ret = settings_load();
    if (ret) {
        LOG_ERR("Failed to load settings: %d", ret);
        return;
    }

    generate_and_store_uuid();

    ret = init_adc();
    if (ret) {
        LOG_ERR("ADC init failed: %d", ret);
        return;
    }

    ret = init_i2c();
    if (ret) {
        LOG_ERR("I2C init failed: %d", ret);
        return;
    }

    ret = init_button();
    if (ret) {
        LOG_ERR("Button init failed: %d", ret);
        return;
    }

    ret = soil_moisture_init(i2c_dev);
    if (ret) {
        LOG_ERR("Soil moisture sensor init failed: %d", ret);
        return;
    }

    ret = max17043_init(i2c_dev);
    if (ret) {
        LOG_ERR("Battery monitor init failed: %d", ret);
        return;
    }

    ret = init_mqtt();
    if (ret) {
        LOG_ERR("MQTT init failed: %d", ret);
        return;
    }

    // Start periodic sensor reading and publishing
    k_work_schedule(&publish_work, K_NO_WAIT);

    // Main loop - can be used for other tasks
    while (1) {
        k_sleep(K_FOREVER);
    }
}