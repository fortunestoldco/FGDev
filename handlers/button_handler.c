// handlers/button_handler.c
#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/logging/log.h>
#include "config.h"

LOG_MODULE_REGISTER(button_handler, LOG_LEVEL_INF);

static struct gpio_callback button_cb_data;
static uint32_t last_press_time;

static void button_pressed_cb(const struct device *dev, struct gpio_callback *cb, uint32_t pins)
{
    uint32_t current_time = k_uptime_get_32();
    
    if ((current_time - last_press_time) > BUTTON_DEBOUNCE_TIME) {
        LOG_INF("Button pressed");
        // Add your button press handling logic here
        last_press_time = current_time;
    }
}

void button_init(void)
{
    const struct device *button_dev;
    int ret;

    button_dev = device_get_binding(BUTTON_GPIO_PORT);
    if (!device_is_ready(button_dev)) {
        LOG_ERR("Button device not ready");
        return;
    }

    ret = gpio_pin_configure(button_dev, BUTTON_GPIO_PIN, 
                           GPIO_INPUT | GPIO_PULL_UP | GPIO_INT_EDGE_TO_ACTIVE);
    if (ret != 0) {
        LOG_ERR("Failed to configure button pin: %d", ret);
        return;
    }

    gpio_init_callback(&button_cb_data, button_pressed_cb, BIT(BUTTON_GPIO_PIN));
    gpio_add_callback(button_dev, &button_cb_data);

    ret = gpio_pin_interrupt_configure(button_dev, BUTTON_GPIO_PIN, GPIO_INT_EDGE_TO_ACTIVE);
    if (ret != 0) {
        LOG_ERR("Failed to configure button interrupt: %d", ret);
        return;
    }

    LOG_INF("Button handler initialized");
}