#ifndef MAX17043_DRIVER_H
#define MAX17043_DRIVER_H

#include <zephyr/device.h>

// Register definitions
#define MAX17043_REG_VCELL    0x02  // Voltage register
#define MAX17043_REG_SOC      0x04  // State of charge register
#define MAX17043_REG_MODE     0x06  // Mode register
#define MAX17043_REG_VERSION  0x08  // Version register
#define MAX17043_REG_CONFIG   0x0C  // Configuration register
#define MAX17043_REG_COMMAND  0xFE  // Command register

// Command definitions
#define MAX17043_CMD_POR      0x5400  // Power-on reset
#define MAX17043_CMD_QUICK_START  0x4000  // Quick-start command

// Function declarations
/**
 * @brief Initialize the MAX17043 fuel gauge
 *
 * @param i2c_dev Pointer to I2C device structure
 * @return 0 on success, negative errno on failure
 */
int max17043_init(const struct device *i2c_dev);

/**
 * @brief Read battery voltage
 *
 * @param i2c_dev Pointer to I2C device structure
 * @param voltage Pointer to store voltage value (in volts)
 * @return 0 on success, negative errno on failure
 */
int max17043_read_voltage(const struct device *i2c_dev, float *voltage);

/**
 * @brief Read battery state of charge (percentage)
 *
 * @param i2c_dev Pointer to I2C device structure
 * @param battery_level Pointer to store battery level (0-100%)
 * @return 0 on success, negative errno on failure
 */
int max17043_read(const struct device *i2c_dev, float *battery_level);

/**
 * @brief Perform quick start (reset voltage and SOC calculation)
 *
 * @param i2c_dev Pointer to I2C device structure
 * @return 0 on success, negative errno on failure
 */
int max17043_quick_start(const struct device *i2c_dev);

/**
 * @brief Configure alert threshold
 *
 * @param i2c_dev Pointer to I2C device structure
 * @param threshold Alert threshold percentage (1-32%)
 * @return 0 on success, negative errno on failure
 */
int max17043_set_alert_threshold(const struct device *i2c_dev, uint8_t threshold);

/**
 * @brief Get alert status
 *
 * @param i2c_dev Pointer to I2C device structure
 * @param alert_triggered Pointer to store alert status
 * @return 0 on success, negative errno on failure
 */
int max17043_get_alert_status(const struct device *i2c_dev, bool *alert_triggered);

#endif /* MAX17043_DRIVER_H */