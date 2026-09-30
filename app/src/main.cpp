#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include "rt_driver.h"

LOG_MODULE_REGISTER(main, LOG_LEVEL_INF);

namespace {
    void test() {
        const struct device *dev =DEVICE_DT_GET(DT_NODELABEL(rt_driver0));
        struct sensor_value val;
        int ret = sensor_sample_fetch(dev);
        LOG_INF("Sample fetch ret %d", ret);
        k_msleep(1000); // Sleep for 1 second to allow time for sample fetch
        ret = sensor_channel_get(dev, SENSOR_CHAN_AMBIENT_TEMP, &val);
        LOG_INF("Channel ret %d", ret);
    }
}

int main(void)
{
    LOG_INF("Hello World! %s", CONFIG_BOARD);
    test();
    return 0;
}
