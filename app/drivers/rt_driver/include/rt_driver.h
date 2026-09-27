#ifndef RT_DRIVER_H_
#define RT_DRIVER_H_

#include <zephyr/device.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Custom API function pointer prototypes */
typedef int (*rt_driver_set_threshold_t)(const struct device *dev, uint32_t threshold);
typedef int (*rt_driver_get_threshold_t)(const struct device *dev, uint32_t *);

/* Driver API struct */
__subsystem struct rt_driver_api {
    rt_driver_set_threshold_t set_threshold;
    rt_driver_get_threshold_t get_threshold;
};

/**
 * @brief Change a runtime data struct value (threshold) at runtime.
 */
static inline int rt_driver_set_threshold(const struct device *dev, uint32_t threshold)
{
    const struct rt_driver_api *api = (const struct rt_driver_api *)dev->api;

    if (!api || !api->set_threshold) {
        return -ENOSYS;
    }
    return api->set_threshold(dev, threshold);
}

/**
 * @brief Read current sensor value from driver data.
 */
static inline int rt_driver_get_threshold(const struct device *dev, uint32_t *value)
{
    const struct rt_driver_api *api = (const struct rt_driver_api *)dev->api;

    if (!api || !api->get_threshold) {
        return -ENOSYS;
    }
    return api->get_threshold(dev, value);
}

#ifdef __cplusplus
}
#endif

#endif /* RT_DRIVER_H_ */