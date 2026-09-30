#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include "rt_driver.h"

LOG_MODULE_REGISTER(main, LOG_LEVEL_INF);

namespace {
    void test() 
    {
        const struct device *dev =DEVICE_DT_GET(DT_NODELABEL(rt_driver0));
        struct sensor_value val;
        int ret = sensor_sample_fetch(dev);
        LOG_INF("Sample fetch ret %d", ret);
        k_msleep(1000); // Sleep for 1 second to allow time for sample fetch
        ret = sensor_channel_get(dev, SENSOR_CHAN_AMBIENT_TEMP, &val);
        LOG_INF("Channel ret %d", ret);

        struct rt_driver_config_data set_value = { .blink_speed_ms = 100 }; // Initial value to set
        struct rt_driver_config_data read_value;
        for(int i = 0; i < 5; ++i) 
        {
            set_value.blink_speed_ms = 100 * (i);
            int ret = rt_driver_set_data(dev, &set_value);
            if (ret != 0) {
                LOG_ERR("Failed to set blink speed: %d", ret);
                return;
            }

            ret = rt_driver_get_data(dev, &read_value);
            if (ret != 0) {
                LOG_ERR("Failed to get blink speed: %d", ret);
                return;
            }

            LOG_INF("Retrieved blink speed: %d (set=%d)", 
                read_value.blink_speed_ms, 
                set_value.blink_speed_ms);

            k_msleep(1000); // Sleep for 1 second to allow time for sample fetch
        }
    }
}

int main(void)
{
    LOG_INF("Hello World! %s", CONFIG_BOARD);
    test();
    return 0;
}
