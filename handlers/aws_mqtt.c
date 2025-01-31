#include <zephyr.h>
#include <net/mqtt.h>
#include <net/socket.h>
#include <sys/byteorder.h>
#include <string.h>
#include <stdio.h>

#define AWS_ENDPOINT "your_mqtt_broker_hostname"

struct mqtt_client_ctx {
    struct mqtt_client client;
    struct sockaddr_in broker;
};

static struct mqtt_client_ctx client_ctx;

int aws_mqtt_init(void)
{
    int err;

    struct sockaddr_in broker_addr;
    broker_addr.sin_family = AF_INET;
    broker_addr.sin_port = htons(CONFIG_MQTT_BROKER_PORT);

    err = inet_pton(AF_INET, AWS_ENDPOINT, &broker_addr.sin_addr);
    if (err <= 0) {
        printk("Invalid broker address\n");
        return -EINVAL;
    }

    client_ctx.broker = broker_addr;
    client_ctx.client.broker = (struct sockaddr *)&client_ctx.broker;
    client_ctx.client.broker_len = sizeof(struct sockaddr_in);

    // Initialize MQTT client here
    // ...

    return 0;
}

int aws_mqtt_publish(const char *topic, const uint8_t *payload, size_t len)
{
    struct mqtt_publish_param param;

    param.message.topic.qos = MQTT_QOS_1_AT_LEAST_ONCE;
    param.message.topic.topic.utf8 = (uint8_t *)topic;
    param.message.topic.topic.size = strlen(topic);
    param.message.payload.data = (uint8_t *)payload;
    param.message.payload.len = len;
    param.message.payload.offset = 0;
    param.topic.topic = NULL;
    param.message_id = 0;
    param.message.topic.type = MQTT_TOPIC_UTF8;
    param.dupflag = 0;
    param.retain = 0;

    return mqtt_publish(&client_ctx.client, &param);
}