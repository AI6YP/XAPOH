idf_component_register(
  SRCS "sw-panel-v3-main.c" "led_strip_encoder.c"
  PRIV_REQUIRES
    esp_driver_rmt esp_event esp_partition
    esp_driver_gpio esp_driver_i2c esp_wifi esp-tls esp_http_server
    nvs_flash 
  INCLUDE_DIRS "."
)
