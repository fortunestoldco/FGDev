#ifndef CONFIG_H
#define CONFIG_H

// Device configurations
#define I2C_DEV_NAME        "I2C_0"
#define LED_GPIO_PORT       "GPIO_0"
#define LED_GPIO_PIN        13
#define BUTTON_GPIO_PORT    "GPIO_0"
#define BUTTON_GPIO_PIN     12
#define BUTTON_DEBOUNCE_TIME 300  // 300ms debounce time

// Sensor I2C addresses
#define AHT10_ADDR         0x38
#define SOIL_MOISTURE_ADDR 0x36
#define MAX17043_ADDR      0x36

// Storage configurations
#define STORAGE_NAMESPACE  "settings"
#define KEY_UUID          "uuid"

// Timing configurations
#define POLLING_INTERVAL   (60 * 1000) // 1 minute in milliseconds

// AWS IoT configurations
#define AWS_ENDPOINT      "your-iot-endpoint.amazonaws.com"
#define AWS_PORT         8883
#define AWS_CLIENT_ID    "plant_monitor"
#define MQTT_PUBLISH_TOPIC "plants/"

#endif /* CONFIG_H */