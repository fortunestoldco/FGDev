#include <zephyr/kernel.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/logging/log.h>
#include "config.h"

LOG_MODULE_REGISTER(aht10_driver, LOG_LEVEL_INF);

#define AHT10_ADDR 0x38
#define AHT10_CMD_INIT 0xBE
#define AHT10_CMD_MEASURE 0xAC

int aht10_init(struct device *i2c_dev)
{
    uint8_t cmd_init[] = {AHT10_CMD_INIT, 0x08, 0x00};
    int ret = i2c_write(i2c_dev, cmd_init, sizeof(cmd_init), AHT10_ADDR << 1);
    if (ret != 0) {
        LOG_ERR("AHT10 init failed: %d", ret);
        return ret;
    }
    return 0;
}

int aht10_read(struct device *i2c_dev, float *temperature, float *humidity)
{
    uint8_t cmd_measure[] = {AHT10_CMD_MEASURE, 0x00};
    uint8_t data[6];
    int ret;

    ret = i2c_write_read(i2c_dev, AHT10_ADDR << 1, cmd_measure, sizeof(cmd_measure), data, sizeof(data));
    if (ret != 0) {
        LOG_ERR("AHT10 read failed: %d", ret);
        return ret;
    }

    uint32_t raw_temp = (data[3] << 16) | (data[4] << 8) | data[5];
    uint32_t raw_humidity = (data[1] << 16) | (data[2] << 8) | data[3];

    *temperature = (raw_temp / 1048576.0) * 200.0 - 50.0;
    *humidity = (raw_humidity / 1048576.0) * 100.0;

    LOG_INF("AHT10 Temperature: %.2f°C, Humidity: %.2f%%", *temperature, *humidity);
    return 0;
}
