if(CONFIG_RT_DRIVER)
  target_sources(app PRIVATE rt_driver.c)

  #zephyr_library()
  #zephyr_library_sources(rt_driver.c)
endif()