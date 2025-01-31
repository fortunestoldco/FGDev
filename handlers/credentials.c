// handlers/credentials.c
#include <zephyr/kernel.h>
#include <zephyr/settings/settings.h>
#include <zephyr/logging/log.h>
#include "config.h"

LOG_MODULE_REGISTER(credentials, LOG_LEVEL_INF);

static struct credentials {
    char wifi_ssid[32];
    char wifi_pass[64];
    char aws_endpoint[128];
    char aws_client_id[32];
    char device_cert[2048];
    char private_key[2048];
} creds;

static int credentials_set(const char *name, size_t len,
                         settings_read_cb read_cb, void *cb_arg)
{
    int ret;
    const char *next;

    if (settings_name_steq(name, "wifi_ssid", &next) && !next) {
        if (len > sizeof(creds.wifi_ssid) - 1) {
            return -EINVAL;
        }
        ret = read_cb(cb_arg, creds.wifi_ssid, len);
        if (ret < 0) {
            return ret;
        }
        creds.wifi_ssid[len] = '\0';
        return 0;
    }

    // Add similar handlers for other credentials

    return -ENOENT;
}

static struct settings_handler creds_conf = {
    .name = "creds",
    .h_set = credentials_set
};

void credentials_init(void)
{
    int ret = settings_register(&creds_conf);
    if (ret) {
        LOG_ERR("Failed to register settings handler: %d", ret);
        return;
    }

    LOG_INF("Credentials handler initialized");
}