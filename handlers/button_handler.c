#include <zephyr.h>
#include <drivers/gpio.h>
#include <sys/printk.h>

// Define BUTTON_DEBOUNCE_TIME as a uint32_t representing milliseconds
#define BUTTON_DEBOUNCE_TIME 200

static uint32_t last_press_time = 0;

void button_pressed_cb(const struct device *dev, struct gpio_callback *cb, uint32_t pins)
{
    uint32_t current_time = k_uptime_get_32();

    if ((current_time - last_press_time) > BUTTON_DEBOUNCE_TIME) {
        printk("Button pressed at %u ms\n", current_time);
        last_press_time = current_time;
        // Handle button press event
    }
}
