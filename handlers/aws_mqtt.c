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
#include <zephyr/net/mqtt.h>

static struct mqtt_client client;

int aws_mqtt_init(void)
{
    int ret;

    // Initialize the MQTT client structure
    mqtt_client_init(&client);

    // Configure MQTT client
    client.broker = (struct sockaddr_in){
        .sin_family = AF_INET,
        .sin_port = htons(8883),  // Default MQTT over TLS port
    };
    
    client.client_id.utf8 = (uint8_t *)CONFIG_MQTT_CLIENT_ID;
    client.client_id.size = strlen(CONFIG_MQTT_CLIENT_ID);
    
    client.evt_cb = mqtt_evt_handler;  // Set callback handler

    return 0;
}