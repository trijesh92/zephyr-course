#include <zephyr/drivers/sensor.h>
#include <zephyr/logging/log.h>
#include <zephyr/drivers/gpio.h>
#include "rt_driver.h"

#define DT_DRV_COMPAT rt_driver

LOG_MODULE_REGISTER(rt_driver, LOG_LEVEL_INF); 

#define SLEEP_TIME_MS 300
#define LED_NODE DT_NODELABEL(green_led)
static const struct gpio_dt_spec led = GPIO_DT_SPEC_GET(LED_NODE, gpios);
volatile static uint32_t s_led_blink_speed_ms = SLEEP_TIME_MS; // Default blink speed

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
    k_msleep(s_led_blink_speed_ms);
    led_off();
    k_msleep(s_led_blink_speed_ms);
}

// -----------------------------------------------------------------------------------

struct rt_driver_data {
    struct rt_driver_config_data config;
};

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

/* Extension API Implementation */
static int rt_driver_impl_set_data(const struct device *dev, const struct rt_driver_config_data *data)
{
    if (data == NULL) {
        return -EINVAL;
    }

    s_led_blink_speed_ms = data->blink_speed_ms;
    led_blink();
    LOG_INF("Data updated: blink speed = %u", s_led_blink_speed_ms);

    return 0;
}

static int rt_driver_impl_get_data(const struct device *dev, struct rt_driver_config_data *data)
{

    if (data == NULL) {
        return -EINVAL;
    }

    *data = (struct rt_driver_config_data){ .blink_speed_ms = s_led_blink_speed_ms };
    return 0;
}

static const struct rt_driver_api api_rt_driver = {
    .sensor_api = {
        .sample_fetch = rt_driver_sample_fetch,
        .channel_get = rt_driver_channel_get,
    },
    .set_data = rt_driver_impl_set_data,
    .get_data = rt_driver_impl_get_data,
};

#define RT_DRIVER_INIT(inst)                                              \
    static struct rt_driver_data rt_driver_data_##inst;                  \
                                                                          \
    DEVICE_DT_INST_DEFINE(inst,                                           \
                          init,                                           \
                          NULL,                                           \
                          &rt_driver_data_##inst,                         \
                          NULL,                                           \
                          POST_KERNEL,                                    \
                          80,                                             \
                          &api_rt_driver);

DT_INST_FOREACH_STATUS_OKAY(RT_DRIVER_INIT)