#ifndef RT_DRIVER_H_
#define RT_DRIVER_H_

#include <zephyr/device.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Custom data type */
struct rt_driver_config_data {
    uint32_t blink_speed_ms; // Blink speed in milliseconds
};

/* extension api */
typedef int (*rt_driver_set_data_t)(const struct device *dev, const struct rt_driver_config_data *data);
typedef int (*rt_driver_get_data_t)(const struct device *dev, struct rt_driver_config_data *data);

/* API struct with extension API*/
__subsystem struct rt_driver_api {
    struct sensor_driver_api sensor_api; 
    rt_driver_set_data_t set_data;
    rt_driver_get_data_t get_data;
};

/* Extension API Wrappers */
static inline int rt_driver_set_data(const struct device *dev, const struct rt_driver_config_data *data)
{
    const struct rt_driver_api *api = (const struct rt_driver_api *)dev->api;

    if (api == NULL || api->set_data == NULL) {
        return -ENOSYS;
    }

    return api->set_data(dev, data);
}

static inline int rt_driver_get_data(const struct device *dev, struct rt_driver_config_data *data)
{
    const struct rt_driver_api *api = (const struct rt_driver_api *)dev->api;

    if (api == NULL || api->get_data == NULL) {
        return -ENOSYS;
    }

    return api->get_data(dev, data);
}

#ifdef __cplusplus
}
#endif

#endif /* RT_DRIVER_H_ */