#include <zephyr/drivers/sensor.h>
#include <zephyr/logging/log.h>
#include <zephyr/drivers/gpio.h>
#include "rt_driver.h"

#define DT_DRV_COMPAT rt_driver

LOG_MODULE_REGISTER(rt_driver, LOG_LEVEL_INF); 

#define SLEEP_TIME_MS 300
#define LED_NODE DT_NODELABEL(green_led)
static const struct gpio_dt_spec led = GPIO_DT_SPEC_GET(LED_NODE, gpios);

void init_led(void)
{
    if (!device_is_ready(led.port)) {
        LOG_ERR("GPIO device not ready");
        return;
    }
    if (gpio_pin_configure_dt(&led, GPIO_OUTPUT_ACTIVE) < 0) {
        LOG_ERR("Failed to configure GPIO pin");
        return;
    }
}

void led_on(void)
{
    if (gpio_pin_set_dt(&led, 1) < 0) {
        LOG_ERR("Failed to set GPIO pin");
    }
}

void led_off(void)
{
    if (gpio_pin_set_dt(&led, 0) < 0) {
        LOG_ERR("Failed to clear GPIO pin");
    }
}

void led_blink(void)
{
    led_on();
    k_msleep(SLEEP_TIME_MS);
    led_off();
    k_msleep(SLEEP_TIME_MS);
}

// -----------------------------------------------------------------------------------

struct rt_driver_data {
    uint32_t current_value;
    uint32_t threshold;       /* Value we will change at runtime */
};

static int impl_rt_driver_set_threshold(const struct device *dev, uint32_t threshold)
{
    struct rt_driver_data *data = dev->data;
    
    /* Modify driver data at runtime */
    data->threshold = threshold;
    LOG_INF("Runtime data updated: threshold set to %u", data->threshold);
    led_blink(); // Blink LED to indicate threshold change
    return 0;
}

static int impl_rt_driver_get_threshold(const struct device *dev, uint32_t *value)
{
    struct rt_driver_data *data = dev->data;

    if (!value) {
        return -EINVAL;
    }
    led_blink(); // Blink LED to indicate threshold read
    *value = data->threshold;

    return 0;
}

// --------------------------------------------------------------------------------


static int init(const struct device *dev)
{
    /* Implement your initialization logic here */
    struct rt_driver_data *data = dev->data;
    data->current_value = 100;
    data->threshold = 50;
    LOG_INF("Initializing RT driver");
    init_led();
    return 0; // Return appropriate status
}

static const struct rt_driver_api rt_driver_driver_api = {
    .set_threshold = impl_rt_driver_set_threshold,
    .get_threshold     = impl_rt_driver_get_threshold,
};

#define RT_DRIVER_INIT(inst)\
    static struct rt_driver_data rt_driver_data_##inst;\
    \
    DEVICE_DT_INST_DEFINE(inst,\
                          init,\
                          NULL,\
                          &rt_driver_data_##inst,\
                          NULL,\
                          POST_KERNEL,\
                          80,\
                          &rt_driver_driver_api);

DT_INST_FOREACH_STATUS_OKAY(RT_DRIVER_INIT)