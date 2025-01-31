/* 
 * Plant Monitor Firmware for Seeed Xiao ESP32C6 using ZephyrOS
 * 
 * Features:
 * - Temperature and Humidity Monitoring (AHT10)
 * - Soil Moisture Sensing (Capacitive Sensor)
 * - Light Level Measurement (Photoresistor)
 * - Battery Level Monitoring (MAX17043 Fuel Gauge)
 * - UUID Generation and Persistent Storage
 * - Wi-Fi Provisioning over BLE
 * - AWS IoT Core Integration via MQTT
 * - Data Caching and Reconnection Logic
 * - Button Interactions for Reset and Re-Provisioning
 * - Over-The-Air (OTA) Updates
 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/adc.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/net/mqtt.h>
#include <zephyr/sys/util.h>
#include <zephyr/settings/settings.h>
#include <zephyr/fs/fs.h>
#include <zephyr/logging/log.h>

#include "config.h"


LOG_MODULE_REGISTER(main, CONFIG_APP_LOG_LEVEL);

// Forward declarations
static void button_init_handler(const struct device *dev, gpio_pin_t pin);
void ble_provisioning_init(void);
void aws_mqtt_init(void);
void button_init(void);

// Global Variables
static const struct device *i2c_dev;
static const struct device *button_dev;
static const struct device *led_dev;
static const struct device *adc_dev;
static struct adc_sequence adc_seq = {
    .channels = BIT(0),
    .buffer = NULL,
    .buffer_size = 0,
    .resolution = ADC_RESOLUTION,
    .oversampling = 0,
    .calibrate = false
};

// MQTT Client
static struct mqtt_client client;

// Work for publishing data
static struct k_work_delayable publish_work;

// Connectivity Status
bool wifi_connected = false;
static int reconnect_attempts = 0;
static const int MAX_RECONNECT_ATTEMPTS = 3;

// Plant Data Structure
struct plant_data {
    char plant_id[37];        // UUID v4 string
    char plant_name[50];
    char plant_variety[50];
    char plant_location[100];
    int polling_interval;     // in minutes
    float temperature;
    float humidity;
    float soil_moisture;
    float light_level;
    float battery_level;
    int64_t timestamp;
};

// Function Prototypes
static void read_sensors(struct plant_data *data);
static void publish_data(struct plant_data *data);
static void cache_data(struct plant_data *data);
static void generate_and_store_uuid(void);

// Settings Load Callback
static int settings_set(const char *name, size_t len, settings_read_cb read_cb, void *cb_arg)
{
    return 0;
}

static struct settings_handler settings_handler_data = {
    .name = STORAGE_NAMESPACE,
    .h_set = settings_set,
};

int main(void)
{
    int ret;
    
    LOG_INF("Starting Plant Monitor Firmware v%s", CONFIG_APP_VERSION);

    // Initialize Settings
    ret = settings_subsys_init();
    if (ret) {
        LOG_ERR("Failed to initialize settings subsystem: %d", ret);
        return ret;
    }

    ret = settings_register(&settings_handler_data);
    if (ret) {
        LOG_ERR("Failed to register settings handler: %d", ret);
        return ret;
    }

    ret = settings_load();
    if (ret) {
        LOG_ERR("Failed to load settings: %d", ret);
        return ret;
    }

    // Initialize UUID
    generate_and_store_uuid();

    // Initialize I2C
    i2c_dev = device_get_binding(I2C_DEV_NAME);
    if (!device_is_ready(i2c_dev)) {
        LOG_ERR("I2C device not ready");
        return -ENODEV;
    }

    // Initialize GPIO for Button
    button_init();

    // Initialize GPIO for LED
    led_dev = device_get_binding(LED_GPIO_PORT);
    if (!device_is_ready(led_dev)) {
        LOG_ERR("LED GPIO device not ready");
        return -ENODEV;
    }
    
    ret = gpio_pin_configure(led_dev, LED_GPIO_PIN, GPIO_OUTPUT_ACTIVE | GPIO_ACTIVE_LOW);
    if (ret) {
        LOG_ERR("Failed to configure LED GPIO: %d", ret);
        return ret;
    }

    // Initialize ADC for Photoresistor
    adc_dev = DEVICE_DT_GET(DT_NODELABEL(adc0));
    if (!device_is_ready(adc_dev)) {
        LOG_ERR("ADC device not ready");
        return -ENODEV;
    }

    struct adc_channel_cfg channel_cfg = {
        .gain = ADC_GAIN,
        .reference = ADC_REFERENCE,
        .acquisition_time = ADC_ACQUISITION_TIME,
        .channel_id = ADC_CHANNEL,
        .differential = false
    };

    ret = adc_channel_setup(adc_dev, &channel_cfg);
    if (ret) {
        LOG_ERR("Failed to setup ADC channel: %d", ret);
        return ret;
    }

    // Initialize BLE Provisioning
    ble_provisioning_init();

    // Connect to Wi-Fi
    wifi_connected = false;
    reconnect_attempts = 0;

    // Initialize AWS MQTT
    aws_mqtt_init();

    // Schedule Data Publishing
    k_work_init_delayable(&publish_work, publish_work_handler);
    k_work_schedule(&publish_work, K_MSEC(POLLING_INTERVAL));

    return 0;
}

static void publish_work_handler(struct k_work *work)
{
    struct plant_data data = {0};
    read_sensors(&data);

    if (wifi_connected) {
        publish_data(&data);
        reconnect_attempts = 0;
    } else {
        cache_data(&data);
        reconnect_attempts++;
        if (reconnect_attempts < MAX_RECONNECT_ATTEMPTS) {
            // Attempt to reconnect in the next cycle
            wifi_connected = false;
        }
    }

    // Reschedule the publish work
    k_work_schedule(&publish_work, K_MSEC(POLLING_INTERVAL));
}

static void read_sensors(struct plant_data *data)
{
    uint8_t raw_data[4];
    int16_t adc_value;
    int ret;

    // Read temperature and humidity from AHT10
    ret = aht10_read(i2c_dev, &data->temperature, &data->humidity);
    if (ret) {
        LOG_ERR("Failed to read AHT10 sensor: %d", ret);
        data->temperature = 0.0f;
        data->humidity = 0.0f;
    }

    // Read soil moisture
    ret = soil_moisture_read(i2c_dev, &data->soil_moisture);
    if (ret) {
        LOG_ERR("Failed to read soil moisture sensor: %d", ret);
        data->soil_moisture = 0.0f;
    }

    // Read light level using ADC
    adc_seq.buffer = &adc_value;
    adc_seq.buffer_size = sizeof(adc_value);
    
    ret = adc_read(adc_dev, &adc_seq);
    if (ret == 0) {
        data->light_level = (float)adc_value * (100.0f / ((1 << ADC_RESOLUTION) - 1));
    } else {
        LOG_ERR("Failed to read ADC: %d", ret);
        data->light_level = 0.0f;
    }

    // Read battery level from MAX17043
    ret = max17043_read(i2c_dev, &data->battery_level);
    if (ret) {
        LOG_ERR("Failed to read battery level: %d", ret);
        data->battery_level = 0.0f;
    }

    data->timestamp = k_uptime_get();
}

static void generate_and_store_uuid(void)
{
    uint8_t uuid[16];
    char uuid_str[37];
    
    if (settings_load_subtree(STORAGE_NAMESPACE) < 0) {
        // Generate random bytes for UUID
        for (int i = 0; i < sizeof(uuid); i++) {
            uuid[i] = k_cycle_get_32() & 0xFF;  // Use cycle count as entropy source
        }
        
        // Set version 4 and variant bits according to RFC 4122
        uuid[6] = (uuid[6] & 0x0F) | 0x40;  // Version 4
        uuid[8] = (uuid[8] & 0x3F) | 0x80;  // Variant 1
        
        // Format UUID string
        snprintf(uuid_str, sizeof(uuid_str),
                "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
                uuid[0], uuid[1], uuid[2], uuid[3],
                uuid[4], uuid[5], uuid[6], uuid[7],
                uuid[8], uuid[9], uuid[10], uuid[11],
                uuid[12], uuid[13], uuid[14], uuid[15]);
                
        ret = settings_save_one(KEY_UUID, uuid_str, strlen(uuid_str));
        if (ret) {
            LOG_ERR("Failed to save UUID: %d", ret);
        } else {
            LOG_INF("Generated and stored UUID: %s", uuid_str);
        }
    } else {
        LOG_INF("UUID already exists");
    }
}

static void publish_data(struct plant_data *data)
{
    char topic[128];
    char payload[512];
    int ret;

    // Construct MQTT topic
    snprintf(topic, sizeof(topic), "%s%s", MQTT_PUBLISH_TOPIC, data->plant_id);

    // Construct JSON payload using integer arithmetic to avoid float-to-double promotion
    snprintf(payload, sizeof(payload),
             "{"
             "\"plantId\":\"%s\","
             "\"timestamp\":%lld,"
             "\"plantName\":\"%s\","
             "\"plantVariety\":\"%s\","
             "\"plantLocation\":\"%s\","
             "\"temperature\":%d.%02d,"
             "\"humidity\":%d.%02d,"
             "\"soilMoisture\":%d.%02d,"
             "\"lightLevel\":%d.%02d,"
             "\"batteryLevel\":%d.%02d"
             "}",
             data->plant_id,
             data->timestamp,
             data->plant_name,
             data->plant_variety,
             data->plant_location,
             (int)data->temperature, (int)((data->temperature - (int)data->temperature) * 100),
             (int)data->humidity, (int)((data->humidity - (int)data->humidity) * 100),
             (int)data->soil_moisture, (int)((data->soil_moisture - (int)data->soil_moisture) * 100),
             (int)data->light_level, (int)((data->light_level - (int)data->light_level) * 100),
             (int)data->battery_level, (int)((data->battery_level - (int)data->battery_level) * 100));

    struct mqtt_publish_param param = {
        .message.topic.qos = MQTT_QOS_1_AT_LEAST_ONCE,
        .message.topic.topic.utf8 = (uint8_t *)topic,
        .message.topic.topic.size = strlen(topic),
        .message.payload.data = payload,
        .message.payload.len = strlen(payload),
        .message_id = k_cycle_get_32(),
    };

    ret = mqtt_publish(&client, &param);
    if (ret) {
        LOG_ERR("Failed to publish MQTT message: %d", ret);
        cache_data(data);
    } else {
        LOG_INF("Published data to AWS IoT: %s", topic);
    }
}

static void cache_data(struct plant_data *data)
{
    struct fs_file_t file;
    char payload[512];
    int ret;

    fs_file_t_init(&file);
    
    ret = fs_open(&file, CACHE_FILE_PATH, FS_O_CREATE | FS_O_WRITE | FS_O_APPEND);
    if (ret) {
        LOG_ERR("Failed to open cache file: %d", ret);
        return;
    }

    // Construct JSON payload using integer arithmetic
    snprintf(payload, sizeof(payload),
             "{"
             "\"plantId\":\"%s\","
             "\"timestamp\":%lld,"
             "\"plantName\":\"%s\","
             "\"plantVariety\":\"%s\","
             "\"plantLocation\":\"%s\","
             "\"temperature\":%d.%02d,"
             "\"humidity\":%d.%02d,"
             "\"soilMoisture\":%d.%02d,"
             "\"lightLevel\":%d.%02d,"
             "\"batteryLevel\":%d.%02d"
             "}\n",
             data->plant_id,
             data->timestamp,
             data->plant_name,
             data->plant_variety,
             data->plant_location,
             (int)data->temperature, (int)((data->temperature - (int)data->temperature) * 100),
             (int)data->humidity, (int)((data->humidity - (int)data->humidity) * 100),
             (int)data->soil_moisture, (int)((data->soil_moisture - (int)data->soil_moisture) * 100),
             (int)data->light_level, (int)((data->light_level - (int)data->light_level) * 100),
             (int)data->battery_level, (int)((data->battery_level - (int)data->battery_level) * 100));

    ret = fs_write(&file, payload, strlen(payload));
    if (ret < 0) {
        LOG_ERR("Failed to write to cache file: %d", ret);
    } else {
        LOG_INF("Cached data locally");
    }

    fs_close(&file);
}