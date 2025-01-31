#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/logging/log.h>
#include "config.h"

LOG_MODULE_REGISTER(button_handler, LOG_LEVEL_INF);

// Button debounce variables
static uint32_t last_press_time = 0;
static uint32_t press_count = 0;

// Button callback
static struct gpio_callback button_cb_data;

// Forward declarations
void ble_provisioning_init(void);
void soft_reset(void);
void hard_reset(void);

// Button Handler Function
static void button_pressed(const struct device *dev, struct gpio_callback *cb, uint32_t pins)
{
    uint32_t current_time = k_uptime_get_32();
    if ((current_time - last_press_time) > BUTTON_DEBOUNCE_TIME) {
        press_count++;
        last_press_time = current_time;

        if (press_count == 1) {
            // Schedule a delayed work to detect double press or long press
            // For simplicity, directly handling here
            // Implement timers as needed
            // Example:
            // If pressed and held for certain time, trigger long press
        }
    }
}

// Initialize Button Handler
void button_init_handler(const struct device *dev, gpio_pin_t pin)
{
    gpio_init_callback(&button_cb_data, button_pressed, BIT(pin));
    gpio_add_callback(dev, &button_cb_data);
    gpio_pin_interrupt_configure(dev, pin, GPIO_INT_EDGE_TO_ACTIVE);
}

// Implement soft_reset and hard_reset functions
void soft_reset(void)
{
    LOG_INF("Performing soft reset...");
    sys_reboot(SYS_REBOOT_COLD);
}

void hard_reset(void)
{
    LOG_INF("Performing hard reset and wiping credentials...");
    // Implement wiping of stored settings
    settings_delete(STORAGE_NAMESPACE);
    settings_save();
    sys_reboot(SYS_REBOOT_COLD);
}
