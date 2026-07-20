idf_component_register(
  SRCS "sw-panel-v4-main.c" "led_strip_encoder.c" "link.c"
  PRIV_REQUIRES
    esp_driver_rmt esp_event esp_partition
    esp_driver_gpio esp_driver_uart esp_driver_usb_serial_jtag esp_wifi esp-tls esp_http_server
    nvs_flash swio_programmer
  INCLUDE_DIRS "."
)
