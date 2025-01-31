#include <zephyr/kernel.h>
#include <zephyr/net/mqtt.h>
#include <zephyr/logging/log.h>
#include "config.h"

LOG_MODULE_REGISTER(aws_mqtt, LOG_LEVEL_INF);

extern bool wifi_connected;  // Declare as extern since it's defined in main.c
static struct mqtt_client client;

// MQTT Event Callback
static void mqtt_evt_handler(struct mqtt_client *const c, const struct mqtt_evt *evt)
{
    switch (evt->type) {
        case MQTT_EVT_CONNACK:
            if (evt->result == 0) {
                LOG_INF("MQTT Connected");
                wifi_connected = true;
            } else {
                LOG_ERR("MQTT Connack error: %d", evt->result);
                wifi_connected = false;
            }
            break;
        case MQTT_EVT_DISCONNECT:
            LOG_INF("MQTT Disconnected");
            wifi_connected = false;
            break;
        default:
            break;
    }
}

// Initialize MQTT Client
void aws_mqtt_init(void)
{
    struct mqtt_client_config config = {
        .broker = {
            .hostname = AWS_ENDPOINT,
            .port = AWS_PORT
        },
        .client_id = {
            .utf8 = (uint8_t *)AWS_CLIENT_ID,
            .size = strlen(AWS_CLIENT_ID)
        },
        .cb = mqtt_evt_handler  // Set the callback handler
    };

    mqtt_client_init(&client, &config);
    
    int ret = mqtt_connect(&client);
    if (ret != 0) {
        LOG_ERR("Failed to connect to MQTT broker: %d", ret);
        wifi_connected = false;
    } else {
        LOG_INF("Connected to MQTT broker");
    }
}