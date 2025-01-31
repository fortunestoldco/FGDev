#ifndef CONFIG_H
#define CONFIG_H

#define I2C_DEV_NAME        "I2C_0"
#define BUTTON_GPIO_PORT    "GPIO_0"
#define BUTTON_GPIO_PIN     0
#define LED_GPIO_PORT       "GPIO_0"
#define LED_GPIO_PIN        1
#define POLLING_INTERVAL    60000  // Default polling interval in milliseconds

// AWS IoT Core Configuration
#define AWS_ENDPOINT         "your-aws-endpoint.iot.your-region.amazonaws.com"
#define AWS_PORT             8883
#define AWS_CLIENT_ID        "plant_monitor_client"

// MQTT Topics
#define MQTT_PUBLISH_TOPIC  "/devices/plants/"

// Storage Keys
#define STORAGE_NAMESPACE    "plant_monitor"
#define KEY_UUID             "uuid"
#define KEY_WIFI_SSID        "wifi_ssid"
#define KEY_WIFI_PASS        "wifi_pass"
#define KEY_PLANT_NAME       "plant_name"
#define KEY_PLANT_VARIETY    "plant_variety"
#define KEY_PLANT_LOCATION   "plant_location"
#define KEY_POLLING_INTERVAL "polling_interval"

// Button Definitions
#define BUTTON_DEBOUNCE_TIME 200   // in milliseconds
#define BUTTON_DOUBLE_PRESS_INTERVAL 500 // in milliseconds
#define BUTTON_LONG_PRESS_TIME 2000    // in milliseconds

#endif // CONFIG_H
