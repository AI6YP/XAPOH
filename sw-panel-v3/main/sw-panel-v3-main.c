#include <stdint.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"

#include "esp_err.h"
#include "esp_log.h"
#include "esp_event.h"
#include "esp_system.h"
#include "esp_partition.h"
#include "nvs_flash.h"

#include "driver/gpio.h"
#include "driver/i2c_master.h"
#include "driver/rmt_tx.h"

#include "led_strip_encoder.h"

// WIFI
#include "esp_wifi.h"
#include "esp_tls_crypto.h"
#include "esp_http_server.h"

#include "lwip/err.h"
#include "lwip/sys.h"

#include "sdkconfig.h"

#include "pages.h"

#define RMT_LED_STRIP_RESOLUTION_HZ 10000000 // 10MHz resolution, 1 tick = 0.1us (led strip needs a high resolution)
#define RMT_LED_STRIP_GPIO_NUM      8

#define NUM_LEDS 11

// pins
#define PWR_SW        0   // pin6   <-
#define FUZE_IN       1   // pin7   -> ADC?
#define FUZE_OUT      2   // pin8   -> ADC?
#define SDA           3   // pin9   <->
#define SCL           4   // pin10  <->
#define ON            5   // pin11  <-
#define DISPLAY_RS    6   // pin12  <-
#define DISPLAY_CS    7   // pin13  <-
#define RGB           8   // pin14  <- LED Strip
#define BOOT          9   // pin15  ->
#define DISPLAY_DATA  14  // pin18  <-
#define DISPLAY_RESET 15  // pin19  <-
#define TX            16  // pin21  <-
#define RX            17  // pin22  ->
#define SW1_B1        18  // pin23  ->
#define SW1_A1        19  // pin24  ->
#define SW1_D         20  // pin25  ->
#define SW1_B         21  // pin26  ->
#define SW1_A         22  // pin27  ->
#define IR            23  // pin28  ->

#define LCD_HOST SPI2_HOST

static const char *TAG = "XAPOH";

// #include "lcd.h"

/* FreeRTOS event group to signal when we are connected*/
static EventGroupHandle_t s_wifi_event_group;

/* The event group allows multiple bits for each event, but we only care about two events:
 * - we are connected to the AP with an IP
 * - we failed to connect after the maximum amount of retries */
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1

struct app_context_s {
  uint8_t pixels[NUM_LEDS * 3];
  char * ssid;
  char * password;
  const void *config_ptr;
  // rmt_channel_handle_t led_chan; // = NULL;
  // rmt_encoder_handle_t led_encoder;
  // rmt_transmit_config_t tx_config;
};
typedef struct app_context_s app_context_t;

static i2c_master_bus_config_t i2c_master;
// static i2c_device_config_t i2c_dev0;
// static i2c_device_config_t i2c_dev1;

static app_context_t cntxt0;
static rmt_channel_handle_t led_chan = NULL;
static rmt_encoder_handle_t led_encoder0 = NULL;

rmt_transmit_config_t tx_config = { .loop_count = 0, };

static int s_retry_num = 0;

// I2C

i2c_master_bus_config_t i2c_mst_config = {
  .clk_source = I2C_CLK_SRC_DEFAULT, // LP_I2C_SCLK_DEFAULT
  .i2c_port   = -1, // auto
  .scl_io_num = SCL,
  .sda_io_num = SDA,
  .glitch_ignore_cnt = 7,
  .flags.enable_internal_pullup = true
};

i2c_master_bus_handle_t bus_handle;

i2c_device_config_t dev_cfg_1 = {
  .dev_addr_length = I2C_ADDR_BIT_LEN_7,
  .device_address = 0x0A,
  .scl_speed_hz = 100000, // 100000
};

i2c_master_dev_handle_t dev_handle_1;

i2c_device_config_t dev_cfg_2 = {
  .dev_addr_length = I2C_ADDR_BIT_LEN_7,
  .device_address = 0x0B,
  .scl_speed_hz = 100000, // 100000
};

i2c_master_dev_handle_t dev_handle_2;

static void event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
  if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
     esp_wifi_connect();
  } else
  if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
    if (s_retry_num < 100 /* EXAMPLE_ESP_MAXIMUM_RETRY */) {
      esp_wifi_connect();
      s_retry_num++;
      ESP_LOGI(TAG, "retry to connect to the AP");
    } else {
      xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
    }
    ESP_LOGI(TAG,"connect to the AP fail");
  } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
    ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
    ESP_LOGI(TAG, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
    s_retry_num = 0;
    xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
  }
}

// I2C master

void rdwr_i2c(i2c_master_bus_config_t i2c_master, uint8_t *tx_data, uint8_t *rx_data, int length) {

  ESP_LOGI(TAG, "To I2C:");
  for (int i = 0; i < length; i++) {
    ESP_LOGI(TAG, "%x", tx_data[i]);
  }

  if (i2c_master_transmit(
    tx_data[0] ? dev_handle_2 : dev_handle_1,
    tx_data + 1,
    length - 1, -1
  ) != ESP_OK) {
    ESP_LOGI(TAG, "I2C Error");
  };

  // spi_device_acquire_bus(spi, portMAX_DELAY);

  // spi_transaction_t t11;
  // memset(&t11, 0, sizeof(t11));
  // t11.length = 8 * length;
  // t11.tx_buffer = tx_data;
  // t11.rx_buffer = rx_data;

  // esp_err_t ret = spi_device_polling_transmit(spi, &t11);
  // assert(ret == ESP_OK);

  // spi_device_release_bus(spi);

  // ESP_LOGI(TAG, "From SPI:");
  // for (int i = 0; i < ws_pkt.len; i++) {
  //   ESP_LOGI(TAG, "%x", buf[i]);
  // }

}

void wifi_init_sta(app_context_t *cntxt) {
  s_wifi_event_group = xEventGroupCreate();
  ESP_ERROR_CHECK(esp_netif_init());

  ESP_ERROR_CHECK(esp_event_loop_create_default());
  esp_netif_create_default_wifi_sta();

  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  ESP_ERROR_CHECK(esp_wifi_init(&cfg));

  esp_event_handler_instance_t instance_any_id;
  esp_event_handler_instance_t instance_got_ip;

  ESP_ERROR_CHECK(esp_event_handler_instance_register(
    WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL, &instance_any_id
  ));

  ESP_ERROR_CHECK(esp_event_handler_instance_register(
    IP_EVENT, IP_EVENT_STA_GOT_IP, &event_handler, NULL, &instance_got_ip
  ));

  wifi_config_t wifi_config = {
    // 0
    .sta = {
      // .ssid = "eu2aa",
      // .password = "mc067181",
       /* Authmode threshold resets to WPA2 as default if password matches WPA2 standards (pasword len => 8).
        * If you want to connect the device to deprecated WEP/WPA networks, Please set the threshold value
        * to WIFI_AUTH_WEP/WIFI_AUTH_WPA_PSK and set the password with length and format matching to
        * WIFI_AUTH_WEP/WIFI_AUTH_WPA_PSK standards.
        */
      // .threshold.authmode = WIFI_AUTH_WPA2_PSK, // ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD,
      // .sae_pwe_h2e = WPA3_SAE_PWE_BOTH,
    },
  };

  ESP_LOGI(TAG, "B1 [%s] : [%s]", wifi_config.sta.ssid, wifi_config.sta.password);

  // ESP_LOGI(TAG, "B2 [%s] : [%s]", cntxt->ssid, cntxt->password);

  memcpy(
    &wifi_config.sta.ssid,
    (char *)cntxt->config_ptr,
    sizeof(wifi_config.sta.ssid)
  );
  memcpy(
    &wifi_config.sta.password,
    ((char *)cntxt->config_ptr) + 32,
    sizeof(wifi_config.sta.password)
  );

  ESP_LOGI(TAG, "B2 [%s] : [%s]", wifi_config.sta.ssid, wifi_config.sta.password);

  ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA) );
  ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config) );
  ESP_ERROR_CHECK(esp_wifi_start());

  ESP_LOGI(TAG, "wifi_init_sta finished.");

  /* Waiting until either the connection is established (WIFI_CONNECTED_BIT) or connection failed for the maximum
   * number of re-tries (WIFI_FAIL_BIT). The bits are set by event_handler() (see above) */
  EventBits_t bits = xEventGroupWaitBits(
    s_wifi_event_group, WIFI_CONNECTED_BIT | WIFI_FAIL_BIT, pdFALSE, pdFALSE, portMAX_DELAY
  );

  /* xEventGroupWaitBits() returns the bits before the call returned, hence we can test which event actually happened. */

  if (bits & WIFI_CONNECTED_BIT) {
    ESP_LOGI(TAG, "connected to ap SSID:%s password:%s", wifi_config.sta.ssid, wifi_config.sta.password);
  } else if (bits & WIFI_FAIL_BIT) {
    ESP_LOGI(TAG, "Failed to connect to SSID:%s, password:%s", wifi_config.sta.ssid, wifi_config.sta.password);
  } else {
    ESP_LOGE(TAG, "UNEXPECTED EVENT");
  }
}

esp_err_t index_get_handler(httpd_req_t *req) {
  /* Send a simple response */
  ESP_LOGI(TAG, "INDEX GET HANDLER");
  httpd_resp_send(req, PAGE_index, PAGE_index_length);
  return ESP_OK;
}

esp_err_t msg_handler(httpd_req_t *req) {
  if (req->method == HTTP_GET) {
    ESP_LOGI(TAG, "Handshake done, the new connection was opened");
    return ESP_OK;
  }
  httpd_ws_frame_t ws_pkt;
  uint8_t *buf = NULL;
  uint8_t *tx_buf = NULL;
  // rmt_encoder_handle_t led_encoder = NULL;
  // led_strip_encoder_config_t encoder_config = {
  //   .resolution = RMT_LED_STRIP_RESOLUTION_HZ,
  // };

  memset(&ws_pkt, 0, sizeof(httpd_ws_frame_t));
  ws_pkt.type = HTTPD_WS_TYPE_BINARY;
  /* Set max_len = 0 to get the frame len */
  esp_err_t ret = httpd_ws_recv_frame(req, &ws_pkt, 0);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "httpd_ws_recv_frame failed to get frame len with %d", ret);
    return ret;
  }
  // ESP_LOGI(TAG, "frame len is %d", ws_pkt.len);
  if (ws_pkt.len) {
    /* ws_pkt.len + 1 is for NULL termination as we are expecting a string */
    buf = calloc(ws_pkt.len, 1);
    tx_buf = calloc(ws_pkt.len, 1);
    // buf = malloc(ws_pkt.len);
    if (buf == NULL) {
      // ESP_LOGE(TAG, "Failed to calloc memory for buf");
      ESP_LOGE(TAG, "Failed to malloc memory for buf");
      return ESP_ERR_NO_MEM;
    }
    ws_pkt.payload = buf;
    /* Set max_len = ws_pkt.len to get the frame payload */
    ret = httpd_ws_recv_frame(req, &ws_pkt, ws_pkt.len);
    if (ret != ESP_OK) {
      ESP_LOGE(TAG, "httpd_ws_recv_frame failed with %d", ret);
      free(buf);
      return ret;
    }
    if (buf[0] == 255) { // RGB LEDS
      ESP_LOGI(TAG, "pkt_len: %d", ws_pkt.len);
      memcpy(&cntxt0.pixels, buf + 1, ws_pkt.len - 1);
      ESP_ERROR_CHECK(rmt_transmit(led_chan, led_encoder0, cntxt0.pixels, sizeof(cntxt0.pixels), &tx_config));
      ESP_ERROR_CHECK(rmt_tx_wait_all_done(led_chan, portMAX_DELAY));
    } else {
      rdwr_i2c(i2c_master, buf, tx_buf, ws_pkt.len);
    }
    ws_pkt.payload = tx_buf;
  }
  // ESP_LOGI(TAG, "Packet type: %d", ws_pkt.type);
  // if (
  //   ws_pkt.type == HTTPD_WS_TYPE_BINARY &&
  //   strcmp((char*)ws_pkt.payload,"Trigger async") == 0
  // ) {
  //   free(buf);
  //   return trigger_async_send(req->handle, req);
  // }

  ret = httpd_ws_send_frame(req, &ws_pkt);

  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "httpd_ws_send_frame failed with %d", ret);
  }

  // free(buf);
  return ret;
}

/* Our URI handler function to be called during POST /uri request */

httpd_uri_t uri_get = {
  .uri      = "/",
  .method   = HTTP_GET,
  .handler  = index_get_handler,
  .user_ctx = NULL
};

httpd_uri_t msg_get = {
  .uri      = "/dev1",
  .method   = HTTP_GET,
  .handler  = msg_handler,
  .user_ctx = NULL,
  .is_websocket = true
};

httpd_uri_t msg_post = {
  .uri      = "/dev1",
  .method   = HTTP_POST,
  .handler  = msg_handler,
  .user_ctx = NULL,
  .is_websocket = true
};

httpd_handle_t start_webserver() {
  /* Generate default configuration */
  httpd_config_t config = HTTPD_DEFAULT_CONFIG();
  config.stack_size = 16384;

  /* Empty handle to esp_http_server */
  httpd_handle_t server = NULL;

  /* Start the httpd server */
  if (httpd_start(&server, &config) == ESP_OK) {
    /* Register URI handlers */
    httpd_register_uri_handler(server, &uri_get);
    httpd_register_uri_handler(server, &msg_get);
    httpd_register_uri_handler(server, &msg_post);
  }
  /* If server failed to start, handle will be NULL */
  return server;
}

void config_map_init(app_context_t *cntxt) {
  const esp_partition_t *config_partition = esp_partition_find_first(
    ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_ANY, "config"
  );

  assert(config_partition != NULL);

  esp_partition_mmap_handle_t config_map_handle;
  ESP_ERROR_CHECK(esp_partition_mmap(
    config_partition, // partition -- Pointer to partition structure obtained using esp_partition_find_first or esp_partition_get. Must be non-NULL
    0, // offset -- Offset from the beginning of partition where mapping should start.
    config_partition->size, // size -- Size of the area to be mapped.
    ESP_PARTITION_MMAP_DATA, // memory -- Memory space where the region should be mapped
    &cntxt->config_ptr, // const void **out_ptr -- Output, pointer to the mapped memory region
    &config_map_handle // out_handle -- Output, handle which should be used for esp_partition_munmap call
  ));
}

void app_main(void) {
  {
    //Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
      ESP_ERROR_CHECK(nvs_flash_erase());
      ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
  }

  ESP_LOGI(TAG, "HELLO 20:59");

  cntxt0 = (app_context_t) { .pixels = {
    0, 0, 5,    // r g b
    100,0, 0,    // G R B
  }};
  
  config_map_init(&cntxt0);

  // I2C init
  ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_mst_config, &bus_handle));
  ESP_ERROR_CHECK(i2c_master_bus_add_device(bus_handle, &dev_cfg_1, &dev_handle_1));
  ESP_ERROR_CHECK(i2c_master_bus_add_device(bus_handle, &dev_cfg_2, &dev_handle_2));

  uint8_t default0 [] = {0, 0, 6, 125};
  uint8_t default1 [] = {1, 0, 8, 4};
  rdwr_i2c(i2c_master, default0, default0, 4);
  rdwr_i2c(i2c_master, default1, default1, 4);

  // RGB init
  ESP_LOGI(TAG, "Create RMT TX channel");
  rmt_tx_channel_config_t tx_chan_config = {
      .clk_src = RMT_CLK_SRC_DEFAULT, // select source clock
      .gpio_num = RMT_LED_STRIP_GPIO_NUM,
      .mem_block_symbols = 64, // increase the block size can make the LED less flickering
      .resolution_hz = RMT_LED_STRIP_RESOLUTION_HZ,
      .trans_queue_depth = 4, // set the number of transactions that can be pending in the background
  };
  ESP_ERROR_CHECK(rmt_new_tx_channel(&tx_chan_config, &led_chan));


  ESP_LOGI(TAG, "Install led strip encoder");
  rmt_encoder_handle_t led_encoder = NULL;
  led_strip_encoder_config_t encoder_config = {
    .resolution = RMT_LED_STRIP_RESOLUTION_HZ,
  };
  ESP_ERROR_CHECK(rmt_new_led_strip_encoder(&encoder_config, &led_encoder));
  led_encoder0 = led_encoder;

  ESP_LOGI(TAG, "Enable RMT TX channel");
  ESP_ERROR_CHECK(rmt_enable(led_chan));

  // rmt_transmit_config_t tx_config = {
  //   .loop_count = 0, // no transfer loop
  // };



  ESP_ERROR_CHECK(rmt_transmit(led_chan, led_encoder0, cntxt0.pixels, sizeof(cntxt0.pixels), &tx_config));
  ESP_ERROR_CHECK(rmt_tx_wait_all_done(led_chan, portMAX_DELAY));

  // SPI LCD
  // lcd_init(spi);

  wifi_init_sta(&cntxt0);
  start_webserver();
  while(1) {
    vTaskDelay(10 / portTICK_PERIOD_MS); // 10ms polling
  };
}
