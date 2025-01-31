// include/aht10_driver.h
#ifndef AHT10_DRIVER_H
#define AHT10_DRIVER_H

#include <zephyr/device.h>

int aht10_init(const struct device *i2c_dev);
int aht10_read(const struct device *i2c_dev, float *temperature, float *humidity);

#endif /* AHT10_DRIVER_H */

// include/soil_moisture_sensor.h
#ifndef SOIL_MOISTURE_SENSOR_H
#define SOIL_MOISTURE_SENSOR_H

#include <zephyr/device.h>

int soil_moisture_init(const struct device *i2c_dev);
int soil_moisture_read(const struct device *i2c_dev, float *moisture);

#endif /* SOIL_MOISTURE_SENSOR_H */

// include/max17043_driver.h
#ifndef MAX17043_DRIVER_H
#define MAX17043_DRIVER_H

#include <zephyr/device.h>

int max17043_init(const struct device *i2c_dev);
int max17043_read(const struct device *i2c_dev, float *battery_level);

#endif /* MAX17043_DRIVER_H */