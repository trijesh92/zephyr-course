if(CONFIG_RT_DRIVER)
  target_sources(app PRIVATE rt_driver.c)
  target_include_directories(app PRIVATE include)
endif()