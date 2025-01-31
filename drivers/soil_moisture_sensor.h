#ifndef SOIL_MOISTURE_SENSOR_H
#define SOIL_MOISTURE_SENSOR_H

#include <zephyr/device.h>

// Register definitions
#define SOIL_MOISTURE_REG_DATA        0x00  // Moisture data register
#define SOIL_MOISTURE_REG_CONFIG      0x01  // Configuration register
#define SOIL_MOISTURE_REG_CALIB       0x02  // Calibration register
#define SOIL_MOISTURE_REG_THRESHOLD   0x03  // Threshold register

// Configuration bits
#define SOIL_MOISTURE_CONFIG_CONT     0x01  // Continuous measurement mode
#define SOIL_MOISTURE_CONFIG_SLEEP    0x00  // Sleep mode
#define SOIL_MOISTURE_CONFIG_INT_EN   0x02  // Enable interrupt
#define SOIL_MOISTURE_CONFIG_INT_DIS  0x00  // Disable interrupt

/**
 * @brief Initialize the soil moisture sensor
 *
 * @param i2c_dev Pointer to I2C device structure
 * @return 0 on success, negative errno on failure
 */
int soil_moisture_init(const struct device *i2c_dev);

/**
 * @brief Read soil moisture level
 *
 * @param i2c_dev Pointer to I2C device structure
 * @param moisture Pointer to store moisture value (0-100%)
 * @return 0 on success, negative errno on failure
 */
int soil_moisture_read(const struct device *i2c_dev, float *moisture);

/**
 * @brief Calibrate the sensor
 *
 * @param i2c_dev Pointer to I2C device structure
 * @param dry_value Calibration value for dry soil
 * @param wet_value Calibration value for wet soil
 * @return 0 on success, negative errno on failure
 */
int soil_moisture_calibrate(const struct device *i2c_dev, 
                          uint16_t dry_value, 
                          uint16_t wet_value);

/**
 * @brief Set moisture threshold for interrupt
 *
 * @param i2c_dev Pointer to I2C device structure
 * @param threshold Moisture threshold (0-100%)
 * @return 0 on success, negative errno on failure
 */
int soil_moisture_set_threshold(const struct device *i2c_dev, float threshold);

/**
 * @brief Enable/disable continuous measurement mode
 *
 * @param i2c_dev Pointer to I2C device structure
 * @param enable True to enable continuous mode, false for sleep mode
 * @return 0 on success, negative errno on failure
 */
int soil_moisture_set_continuous_mode(const struct device *i2c_dev, bool enable);

/**
 * @brief Get raw sensor value
 *
 * @param i2c_dev Pointer to I2C device structure
 * @param raw_value Pointer to store raw sensor value
 * @return 0 on success, negative errno on failure
 */
int soil_moisture_read_raw(const struct device *i2c_dev, uint16_t *raw_value);

#endif /* SOIL_MOISTURE_SENSOR_H */