#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include "rt_driver.h"

LOG_MODULE_REGISTER(main, LOG_LEVEL_INF);

namespace {
    void test() {
        uint32_t threshold = 75;
        uint32_t set_value = 100;
        const struct device *dev =DEVICE_DT_GET(DT_NODELABEL(rt_driver0));

        for(int i = 0; i < 5; ++i) {
            int ret = rt_driver_set_threshold(dev, set_value);
            if (ret != 0) {
                LOG_ERR("Failed to set threshold: %d", ret);
                return;
            }

            ret = rt_driver_get_threshold(dev, &threshold);
            if (ret != 0) {
                LOG_ERR("Failed to get threshold: %d", ret);
                return;
            }

            LOG_INF("Retrieved threshold: %d (set=%d)", threshold, set_value);

            // Change the value for the next iteration
            set_value += 10;
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
