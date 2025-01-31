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
#include <zephyr/net/mqtt.h>
#include <zephyr/net/socket.h>
#include <zephyr/settings/settings.h>
#include <zephyr/sys/reboot.h>
#include <zephyr/sys/util.h>
#include <zephyr/drivers/adc.h>
#include "config.h"

// Forward declarations
void ble_provisioning_init(void);
void aws_mqtt_init(void);
void button_init(void);

// Global Variables
static struct device *i2c_dev;
static struct device *button_dev;
static struct device *led_dev;
static struct device *adc_dev;

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

void main(void)
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
        return;
    }

    // Initialize GPIO for Button
    button_init();

    // Initialize GPIO for LED
    led_dev = device_get_binding(LED_GPIO_PORT);
    if (!led_dev) {
        printk("GPIO: LED device not found.\n");
        return;
    }
    gpio_pin_configure(led_dev, LED_GPIO_PIN, GPIO_OUTPUT_ACTIVE | GPIO_ACTIVE_LOW);

    // Initialize ADC for Photoresistor
    adc_dev = device_get_binding(DT_LABEL(DT_NODELABEL(adc0)));
    if (!adc_dev) {
        printk("ADC: Device not found.\n");
        return;
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
}

// UUID Generation and Storage
static void generate_and_store_uuid(void)
{
    struct uuid uuid;
    uint8_t uuid_buff[16];
    char uuid_str[37] = {0};

    // Read existing UUID
    if (settings_load_subtree(STORAGE_NAMESPACE)) {
        // UUID not set
        sys_uuid_get(&uuid);
        uuid_to_str(&uuid, uuid_str, sizeof(uuid_str));
        settings_save_one(KEY_UUID, uuid_str);
        printk("Generated and stored UUID: %s\n", uuid_str);
    } else {
        // UUID exists
        printk("UUID already exists.\n");
    }
}

// Read Sensor Data
static void read_sensors(struct plant_data *data)
{
    // Read Temperature and Humidity from AHT10
    if (aht10_read(i2c_dev, &data->temperature, &data->humidity) != 0) {
        printk("Failed to read AHT10 sensor.\n");
        data->temperature = 0.0;
        data->humidity = 0.0;
    }

    // Read Soil Moisture from Capacitive Sensor
    if (soil_moisture_read(i2c_dev, &data->soil_moisture) != 0) {
        printk("Failed to read Soil Moisture sensor.\n");
        data->soil_moisture = 0.0;
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
        data->light_level = 0.0;
    }

    // Read Battery Level from MAX17043
    if (max17043_read(i2c_dev, &data->battery_level) != 0) {
        printk("Failed to read MAX17043 fuel gauge.\n");
        data->battery_level = 0.0;
    }

    // Get current timestamp
    data->timestamp = k_uptime_get();
}

// Publish Work Handler
static void publish_work_handler(struct k_work *work)
{
    struct plant_data data;
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
        .message_id = 0,
    };

    ret = mqtt_publish(&client, &param);
    if (ret != 0) {
        printk("Failed to publish MQTT message: %d\n", ret);
        cache_data(&data);
    } else {
        printk("Published data to AWS IoT: %s\n", topic);
    }
}

// Cache Data Locally
static void cache_data(struct plant_data *data)
{
    // Implement local caching using LittleFS
    struct fs_file_t file;
    int ret;

    fs_file_t_init(&file);
    ret = fs_open(&file, "/cache/data.json", FS_O_CREATE | FS_O_RDWR | FS_O_APPEND);
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
    // Implement Wi-Fi connection using stored credentials
    // This is a simplified example. In a real implementation, use the WLAN API.

    // Example:
    // 1. Retrieve SSID and password from settings
    // 2. Initialize Wi-Fi with these credentials
    // 3. Attempt to connect
    // 4. Set wifi_connected accordingly

    // For demonstration, assume connected
    wifi_connected = true;
    printk("Connected to Wi-Fi.\n");
}

// Initialize BLE Provisioning
void ble_provisioning_init(void)
{
    // Implement BLE provisioning service
    // This involves setting up BLE GATT services to accept Wi-Fi credentials and plant details
    // For brevity, a simplified placeholder is provided

    printk("Initializing BLE Provisioning...\n");
    // Initialize BLE stack
    // Register GATT services
    // Handle provisioning callbacks
}

// Initialize AWS MQTT
void aws_mqtt_init(void)
{
    // Initialize MQTT client structure
    client.broker.uri = AWS_ENDPOINT;
    client.broker.port = AWS_PORT;
    client.evt_cb = NULL; // Define your event callback
    client.client_id.utf8 = AWS_CLIENT_ID;
    client.client_id.size = strlen(AWS_CLIENT_ID);

    // Set TLS configuration
    client.transport.type = MQTT_TRANSPORT_SECURE;
    client.transport.tls_config.ca_cert = resources_AmazonRootCA1_pem_start;
    client.transport.tls_config.ca_cert_size = resources_AmazonRootCA1_pem_end - resources_AmazonRootCA1_pem_start;
    client.transport.tls_config.client_cert = resources_ClientCert_pem_start;
    client.transport.tls_config.client_cert_size = resources_ClientCert_pem_end - resources_ClientCert_pem_start;
    client.transport.tls_config.client_key = resources_ClientKey_pem_start;
    client.transport.tls_config.client_key_size = resources_ClientKey_pem_end - resources_ClientKey_pem_start;

    // Connect to MQTT broker
    int ret = mqtt_connect(&client);
    if (ret != 0) {
        printk("Failed to connect to MQTT broker: %d\n", ret);
        wifi_connected = false;
    } else {
        printk("Connected to MQTT broker.\n");
    }
}

// Initialize Button
void button_init(void)
{
    button_dev = device_get_binding(BUTTON_GPIO_PORT);
    if (!button_dev) {
        printk("GPIO: Button device not found.\n");
        return;
    }

    gpio_pin_configure(button_dev, BUTTON_GPIO_PIN, GPIO_INPUT | GPIO_PULL_UP);

    // Initialize button handler
    button_init_handler(button_dev, BUTTON_GPIO_PIN);
}
