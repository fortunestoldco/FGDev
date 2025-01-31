#ifndef CONFIG_H
#define CONFIG_H

// Device configurations
#define I2C_DEV_NAME        "I2C_0"
#define LED_GPIO_PORT       "GPIO_0"
#define LED_GPIO_PIN        13
#define BUTTON_GPIO_PORT    "GPIO_0"
#define BUTTON_GPIO_PIN     12

// Sensor I2C addresses
#define AHT10_ADDR         0x38
#define SOIL_MOISTURE_ADDR 0x36
#define MAX17043_ADDR      0x36

// Storage configurations
#define STORAGE_NAMESPACE  "settings"
#define KEY_UUID          "uuid"

// Timing configurations
#define POLLING_INTERVAL   (60 * 1000) // 1 minute in milliseconds

#define AWS_ENDPOINT "your-endpoint.iot.region.amazonaws.com"
#define AWS_PORT 8883
#define AWS_CLIENT_ID "your-client-id"
#define MQTT_PUBLISH_TOPIC "your/topic/"

// ADC configurations
#define ADC_RESOLUTION 12
#define ADC_GAIN ADC_GAIN_1
#define ADC_REFERENCE ADC_REF_INTERNAL
#define ADC_ACQUISITION_TIME ADC_ACQ_TIME_DEFAULT
#define ADC_CHANNEL 0
#define BUTTON_DEBOUNCE_TIME K_MSEC(100)
#define CACHE_FILE_PATH "/lfs/cache.json"
#define CONFIG_APP_VERSION "1.0.0"  // Add version number

#endif /* CONFIG_H */