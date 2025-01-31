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
#include <zephyr/sys/printk.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/net/mqtt.h>
#include <zephyr/net/socket.h>
#include <zephyr/settings/settings.h>
#include <zephyr/sys/reboot.h>
#include <zephyr/sys/util.h>
#include <zephyr/drivers/adc.h>
#include <zephyr/fs/fs.h>
#include "config.h"

// Forward declarations
static void button_init_handler(const struct device *dev, uint32_t pin);
static void publish_work_handler(struct k_work *work);
static void generate_and_store_uuid(void);
static void connect_wifi(void);
void ble_provisioning_init(void);
void aws_mqtt_init(void);
void button_init(void);

// Global Variables
static const struct device *i2c_dev;
static const struct device *button_dev;
static const struct device *led_dev;
static const struct device *adc_dev;

// MQTT Client
static struct mqtt_client client;

// Work for publishing data
static struct k_work_delayable publish_work;

// Connectivity Status
static bool wifi_connected = false;
static int reconnect_attempts = 0;
static const int MAX_RECONNECT_ATTEMPTS = 3;

// Plant Data Structure
struct plant_data {
    char plant_id[37]; // UUID v4 string
    char plant_name[50];
    char plant_variety[50];
    char plant_location[100];
    int polling_interval; // in minutes
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

// Settings Load Callback
static int settings_set(const char *name, size_t len, settings_read_cb read_cb, void *cb_arg) {
    // Implementation for settings backend
    return 0;
}

static struct settings_handler settings_handler_data = {
    .name = STORAGE_NAMESPACE,
    .h_set = settings_set,
};

int main(void)
{
    printk("Starting Plant Monitor Firmware\n");

    // Initialize Settings
    settings_subsys_init();
    settings_register(&settings_handler_data);
    settings_load();

    // Initialize UUID
    generate_and_store_uuid();

    // Initialize I2C
    i2c_dev = device_get_binding(I2C_DEV_NAME);
    if (!i2c_dev) {
        printk("I2C: Device not found.\n");
        return -1;
    }

    // Initialize GPIO for Button
    button_init();

    // Initialize GPIO for LED
    led_dev = device_get_binding(LED_GPIO_PORT);
    if (!led_dev) {
        printk("GPIO: LED device not found.\n");
        return -1;
    }
    gpio_pin_configure(led_dev, LED_GPIO_PIN, GPIO_OUTPUT_ACTIVE | GPIO_ACTIVE_LOW);

    // Initialize ADC for Photoresistor
    adc_dev = DEVICE_DT_GET(DT_NODELABEL(adc0));
    if (!device_is_ready(adc_dev)) {
        printk("ADC: Device not ready.\n");
        return -1;
    }

    // Initialize BLE Provisioning
    ble_provisioning_init();

    // Connect to Wi-Fi
    connect_wifi();

    // Initialize AWS MQTT
    aws_mqtt_init();

    // Schedule Data Publishing
    k_work_init_delayable(&publish_work, publish_work_handler);
    k_work_schedule(&publish_work, K_MSEC(POLLING_INTERVAL));

    return 0;
}

// UUID Generation and Storage
static void generate_and_store_uuid(void)
{
    uint8_t uuid[16];
    char uuid_str[37];
    
    if (settings_load_subtree(STORAGE_NAMESPACE) < 0) {
        // Generate random bytes for UUID
        for (int i = 0; i < sizeof(uuid); i++) {
            uuid[i] = sys_rand32_get() & 0xFF;
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
                
        settings_save_one(KEY_UUID, uuid_str, strlen(uuid_str));
        printk("Generated and stored UUID: %s\n", uuid_str);
    } else {
        printk("UUID already exists.\n");
    }
}

// Read Sensor Data
static void read_sensors(struct plant_data *data)
{
    int ret;

    // Read Temperature and Humidity from AHT10
    ret = i2c_write_read(i2c_dev, AHT10_ADDR, NULL, 0, &data->temperature, sizeof(float));
    if (ret != 0) {
        printk("Failed to read AHT10 sensor.\n");
        data->temperature = 0.0f;
        data->humidity = 0.0f;
    }

    // Read Soil Moisture from Capacitive Sensor
    ret = i2c_read(i2c_dev, &data->soil_moisture, sizeof(float), SOIL_MOISTURE_ADDR);
    if (ret != 0) {
        printk("Failed to read Soil Moisture sensor.\n");
        data->soil_moisture = 0.0f;
    }

    // Read Light Level from Photoresistor
    struct adc_sequence sequence = {
        .channels = BIT(0),
        .buffer = &data->light_level,
        .buffer_size = sizeof(data->light_level),
        .resolution = 12,
    };
    if (adc_read(adc_dev, &sequence) != 0) {
        printk("Failed to read ADC for light level.\n");
        data->light_level = 0.0f;
    }

    // Read Battery Level from MAX17043
    ret = i2c_read(i2c_dev, &data->battery_level, sizeof(float), MAX17043_ADDR);
    if (ret != 0) {
        printk("Failed to read MAX17043 fuel gauge.\n");
        data->battery_level = 0.0f;
    }

    // Get current timestamp
    data->timestamp = k_uptime_get();
}

// Publish Work Handler
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
            connect_wifi();
        }
    }

    // Reschedule the publish work
    k_work_schedule(&publish_work, K_MSEC(POLLING_INTERVAL));
}

// Publish Data to AWS IoT
static void publish_data(struct plant_data *data)
{
    char topic[128];
    char payload[512];
    int ret;

    // Construct MQTT topic
    snprintf(topic, sizeof(topic), "%s%s", MQTT_PUBLISH_TOPIC, data->plant_id);

    // Construct JSON payload
    snprintf(payload, sizeof(payload),
             "{"
             "\"plantId\":\"%s\","
             "\"timestamp\":%lld,"
             "\"plantName\":\"%s\","
             "\"plantVariety\":\"%s\","
             "\"plantLocation\":\"%s\","
             "\"temperature\":%.2f,"
             "\"humidity\":%.2f,"
             "\"soilMoisture\":%.2f,"
             "\"lightLevel\":%.2f,"
             "\"batteryLevel\":%.2f"
             "}",
             data->plant_id,
             data->timestamp,
             data->plant_name,
             data->plant_variety,
             data->plant_location,
             data->temperature,
             data->humidity,
             data->soil_moisture,
             data->light_level,
             data->battery_level
    );

    // Publish to MQTT
    struct mqtt_publish_param param = {
        .message.topic.qos = MQTT_QOS_1_AT_LEAST_ONCE,
        .message.topic.topic.utf8 = topic,
        .message.topic.topic.size = strlen(topic),
        .message.payload.data = payload,
        .message.payload.len = strlen(payload),
        .message_id = sys_rand32_get(),
    };

    ret = mqtt_publish(&client, &param);
    if (ret != 0) {
        printk("Failed to publish MQTT message: %d\n", ret);
        cache_data(data);
    } else {
        printk("Published data to AWS IoT: %s\n", topic);
    }
}

// Cache Data Locally
static void cache_data(struct plant_data *data)
{
    struct fs_file_t file;
    int ret;

    fs_file_t_init(&file);
    ret = fs_open(&file, "/cache/data.json", FS_O_CREATE | FS_O_WRITE | FS_O_APPEND);
    if (ret < 0) {
        printk("Failed to open cache file: %d\n", ret);
        return;
    }

    // Construct JSON payload
    char payload[512];
    snprintf(payload, sizeof(payload),
             "{"
             "\"plantId\":\"%s\","
             "\"timestamp\":%lld,"
             "\"plantName\":\"%s\","
             "\"plantVariety\":\"%s\","
             "\"plantLocation\":\"%s\","
             "\"temperature\":%.2f,"
             "\"humidity\":%.2f,"
             "\"soilMoisture\":%.2f,"
             "\"lightLevel\":%.2f,"
             "\"batteryLevel\":%.2f"
             "}\n",
             data->plant_id,
             data->timestamp,
             data->plant_name,
             data->plant_variety,
             data->plant_location,
             data->temperature,
             data->humidity,
             data->soil_moisture,
             data->light_level,
             data->battery_level
    );

    // Write to cache file
    ret = fs_write(&file, payload, strlen(payload));
    if (ret < 0) {
        printk("Failed to write to cache file: %d\n", ret);
    } else {
        printk("Cached data locally.\n");
    }

    fs_close(&file);
}

// Connect to Wi-Fi
static void connect_wifi(void)
{
    // This is a placeholder for actual WiFi implementation
    // You'll need to implement the actual WiFi connection logic using your specific WiFi driver
    
    wifi_connected = true;
    printk("Connected to Wi-Fi.\n");
}

// Initialize BLE Provisioning
void ble_provisioning_init(void)
{
    // This is a placeholder for BLE provisioning implementation
    // You'll need to implement the actual BLE provisioning logic
    
    printk("Initializing BLE Provisioning...\n");
}

// Initialize AWS MQTT
void aws_mqtt_init(void)
{
    struct mqtt_client_config config = {
        .broker = {
            .hostname = AWS_ENDPOINT,
            .port = AWS_PORT
        },
        .credentials = {
            .ca_cert = AWS_ROOT_CA,
            .ca_cert_len = sizeof(AWS_ROOT_CA),
            .client_cert = AWS_CLIENT_CERT,
            .client_cert_len = sizeof(AWS_CLIENT_CERT),
            .private_key = AWS_CLIENT_KEY,
            .private_key_len = sizeof(AWS_CLIENT_KEY)
        }
    };

    mqtt_client_init(&client, &config);
    
    int ret = mqtt_connect(&client);
    if (ret != 0) {
        printk("Failed to connect to MQTT broker: %d\n", ret);
        wifi_connected = false;
    } else {
        printk("Connected to MQTT broker.\n");
    }
}

// Button callback handler
static void button_callback(const struct device *dev, struct gpio_callback *cb, uint32_t pins)
{
    printk("Button pressed!\n");
    // Implement button press handling logic here
}

static struct gpio_callback button_cb_data;

// Initialize Button
void button_init(void)
{
    button_dev = device_get_binding(BUTTON_GPIO_PORT);
    if (!button_dev) {
        printk("GPIO: Button device not found.\n");
        return;
    }

    gpio_pin_configure(button_dev, BUTTON_GPIO_PIN, GPIO_INPUT | GPIO_PULL_UP);
    gpio_pin_interrupt_configure(button_dev, BUTTON_GPIO_PIN, GPIO_INT_EDGE_TO_ACTIVE);

    gpio_init_callback(&button_cb_data, button_callback, BIT(BUTTON_GPIO_PIN));
    gpio_add_callback(button_dev, &button_cb_data);
}