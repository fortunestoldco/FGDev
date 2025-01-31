#include <zephyr/kernel.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/logging/log.h>
#include "config.h"

LOG_MODULE_REGISTER(soil_moisture_sensor, LOG_LEVEL_INF);

#define SOIL_MOISTURE_ADDR 0x36

int soil_moisture_read(struct device *i2c_dev, float *soil_moisture)
{
    uint8_t data[2];
    int ret;

    ret = i2c_write_read(i2c_dev, SOIL_MOISTURE_ADDR << 1, NULL, 0, data, 2);
    if (ret != 0) {
        LOG_ERR("Soil Moisture read failed: %d", ret);
        return ret;
    }

    uint16_t raw = (data[0] << 8) | data[1];
    *soil_moisture = (raw / 65535.0f) * 100.0f; // Use 'f' suffix for float literals

    LOG_INF("Soil Moisture: %d.%02d%%",
            (int)*soil_moisture,
            (int)((*soil_moisture - (int)*soil_moisture) * 100));
    return 0;
}