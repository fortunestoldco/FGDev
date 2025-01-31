#include <zephyr/kernel.h>
#include <zephyr/net/mqtt.h>
#include <zephyr/logging/log.h>
#include <zephyr/net/socket.h>
#include "config.h"

LOG_MODULE_REGISTER(aws_mqtt, CONFIG_APP_LOG_LEVEL);

#define MQTT_BUFFER_SIZE 256

static uint8_t rx_buffer[MQTT_BUFFER_SIZE];
static uint8_t tx_buffer[MQTT_BUFFER_SIZE];
static struct mqtt_client client_ctx;

static void mqtt_evt_handler(struct mqtt_client *client,
                           const struct mqtt_evt *evt)
{
    switch (evt->type) {
    case MQTT_EVT_CONNACK:
        if (evt->result) {
            LOG_ERR("MQTT connect failed: %d", evt->result);
            break;
        }
        LOG_INF("MQTT client connected");
        break;

    case MQTT_EVT_DISCONNECT:
        LOG_INF("MQTT client disconnected");
        break;

    case MQTT_EVT_PUBLISH:
        LOG_INF("MQTT PUBLISH received");
        break;

    default:
        LOG_DBG("MQTT event: %d", evt->type);
        break;
    }
}

void aws_mqtt_init(void)
{
    int err;
    struct sockaddr_in *broker = &(struct sockaddr_in){
        .sin_family = AF_INET,
        .sin_port = htons(AWS_PORT)
    };
    
    err = inet_pton(AF_INET, AWS_ENDPOINT, &broker->sin_addr);
    if (err != 1) {
        LOG_ERR("Invalid broker address");
        return;
    }

    mqtt_client_init(&client_ctx);

    client_ctx.broker = (struct sockaddr *)broker;
    client_ctx.broker_length = sizeof(struct sockaddr_in);
    
    client_ctx.client_id.utf8 = (uint8_t *)AWS_CLIENT_ID;
    client_ctx.client_id.size = strlen(AWS_CLIENT_ID);
    
    client_ctx.password = NULL;
    client_ctx.user_name = NULL;
    
    client_ctx.protocol_version = MQTT_VERSION_3_1_1;
    client_ctx.transport.type = MQTT_TRANSPORT_NON_SECURE; // Change to MQTT_TRANSPORT_SECURE for TLS
    
    client_ctx.rx_buf = rx_buffer;
    client_ctx.rx_buf_size = sizeof(rx_buffer);
    client_ctx.tx_buf = tx_buffer;
    client_ctx.tx_buf_size = sizeof(tx_buffer);
    
    client_ctx.evt_cb = mqtt_evt_handler;

    err = mqtt_connect(&client_ctx);
    if (err) {
        LOG_ERR("MQTT connect failed: %d", err);
        return;
    }

    LOG_INF("AWS MQTT initialized");
}

int aws_mqtt_publish(const char *topic, const char *payload)
{
    struct mqtt_publish_param param = {
        .message.topic.qos = MQTT_QOS_1_AT_LEAST_ONCE,
        .message.topic.topic.utf8 = (uint8_t *)topic,
        .message.topic.topic.size = strlen(topic),
        .message.payload.data = payload,
        .message.payload.len = strlen(payload),
        .message_id = k_cycle_get_32(),
        .dup_flag = 0,
        .retain_flag = 0
    };

    return mqtt_publish(&client_ctx, &param);
}

struct mqtt_client *aws_mqtt_get_client(void)
{
    return &client_ctx;
}