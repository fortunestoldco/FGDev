#include <zephyr/kernel.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/logging/log.h>
#include "config.h"

LOG_MODULE_REGISTER(max17043_driver, LOG_LEVEL_INF);

#define MAX17043_ADDR 0x36
#define MAX17043_REG_SOC 0x04

int max17043_read(struct device *i2c_dev, float *battery_level)
{
    uint8_t reg = MAX17043_REG_SOC;
    uint8_t data[2];
    int ret;

    ret = i2c_write_read(i2c_dev, MAX17043_ADDR << 1, &reg, 1, data, 2);
    if (ret != 0) {
        LOG_ERR("MAX17043 read failed: %d", ret);
        return ret;
    }

    uint16_t soc = (data[0] << 8) | data[1];
    *battery_level = soc / 256.0; // Convert to percentage

    LOG_INF("MAX17043 Battery Level: %.2f%%", *battery_level);
    return 0;
}
