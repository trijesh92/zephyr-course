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

static int channel_get_my_impl(
  const struct device *dev, 
  enum sensor_channel chan, 
  struct sensor_value *val)
{
    /* Implement your channel_get logic here */
    LOG_INF("Getting channel value for channel: %d", chan);
    return 0; // Return appropriate status
}

static int init(const struct device *dev)
{
    /* Implement your initialization logic here */
    LOG_INF("Initializing RT driver");
    init_led();
    return 0; // Return appropriate status
}

static int rt_driver_sample_fetch(const struct device *dev, enum sensor_channel chan)
{
    /* Implement your sample_fetch logic here */
    LOG_INF("Fetching sample for channel: %d", chan);
    led_on(); // Turn off LED once to indicate sample fetch
    return 0; // Return appropriate status
}

static int rt_driver_channel_get(const struct device *dev, enum sensor_channel chan, struct sensor_value *val)
{
    /* Implement your channel_get logic here */
    LOG_INF("Getting channel value for channel: %d", chan);
    led_off(); // Turn on LED once to indicate channel get
    return channel_get_my_impl(dev, chan, val);
}

static DEVICE_API(sensor, api_rt_driver) = {
    .sample_fetch = rt_driver_sample_fetch,
    .channel_get = rt_driver_channel_get,
};

#define RT_DRIVER_INIT(inst)\
    DEVICE_DT_INST_DEFINE(inst,\
                          init,\
                          NULL,\
                          NULL,\
                          NULL,\
                          POST_KERNEL,\
                          80,\
                          &api_rt_driver);

DT_INST_FOREACH_STATUS_OKAY(RT_DRIVER_INIT)