#include <zephyr/shell/shell.h>
#include <zephyr/drivers/sensor.h>
#include "rt_driver.h"

const char* c_driver = "rt_driver0";

static int sensor_fetch_handler(const struct shell* sh, int argc, char** argv)
{
  const struct device *dev = shell_device_get_binding(c_driver);
  if(!dev)
  {
    shell_error(sh, "Could not find device %s", c_driver);
    return -EFAULT;
  }

  int ret = sensor_sample_fetch(dev);
  if(ret!=0)
  {
    shell_error(sh,"Could not fetch channel, got %d", ret);
    return -EFAULT;
  }

  return 0;
}

static int sensor_read_handler(const struct shell* sh, int argc, char** argv)
{
  const struct device *dev = shell_device_get_binding(c_driver);
  if(!dev)
  {
    shell_error(sh, "Could not find device %s", c_driver);
    return -EFAULT;
  }

  struct sensor_value val;
  int ret = sensor_channel_get(dev, SENSOR_CHAN_ACCEL_X, &val);
  if(ret!=0)
  {
    shell_error(sh,"Could not get sensor data, got %d", ret);
    return -EFAULT;
  }

  return 0;
}

static int sensor_info_handler(const struct shell* sh, int argc, char** argv)
{
  const struct device *dev = shell_device_get_binding(c_driver);
  if(!dev)
  {
    shell_error(sh, "Could not find device %s", c_driver);
    return -EFAULT;
  }
  bool is_ready = device_is_ready(dev);

  shell_info(sh,"Name : %s | State : %s", dev->name, is_ready ? "READY" : "NOT READY");
  return 0;
}

SHELL_STATIC_SUBCMD_SET_CREATE(my_driver_subcmd,
  SHELL_CMD_ARG(fetch, NULL, "fetch sensor data", sensor_fetch_handler,1,0),
  SHELL_CMD_ARG(read, NULL, "get the sensor data", sensor_read_handler,1,0),
  SHELL_CMD_ARG(info, NULL, "info", sensor_info_handler,1,0),
  SHELL_SUBCMD_SET_END
);

SHELL_CMD_REGISTER(sensor, &my_driver_subcmd, "My sensor commads", NULL);