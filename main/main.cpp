/*
 * Alexa Voice Service (AVS) Client for ESP32
 *
 * Copyright (C) 2016, www.goldelec.com
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <string.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_wifi.h"
#include "esp_event_loop.h"
#include "esp_log.h"
#include "esp_system.h"
#include "nvs_flash.h"

#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include "lwip/netdb.h"
#include "lwip/dns.h"

#include "mbedtls/platform.h"
#include "mbedtls/net.h"
#include "mbedtls/debug.h"
#include "mbedtls/ssl.h"
#include "mbedtls/entropy.h"
#include "mbedtls/ctr_drbg.h"
#include "mbedtls/error.h"
#include "mbedtls/certs.h"

#include "nghttp2/nghttp2.h"

#include "wm8978.h"
#include "driver/i2c.h"
#include "DriverUtil.h"

#include "driver/i2s.h"
#include <math.h>
#include "Http2Client.h"
#include "sdkconfig.h"



/* FreeRTOS event group to signal when we are connected & ready to make a request */
static EventGroupHandle_t wifi_event_group;

/* The event group allows multiple bits for each event,
   but we only care about one event - are we connected
   to the AP with an IP? */
const int CONNECTED_BIT = BIT0;

/* Constants that aren't configurable in menuconfig */
//#define WEB_SERVER "www.howsmyssl.com"
#define WEB_SERVER "avs-alexa-na.amazon.com"
#define WEB_PORT "443"

static const char *TAG = "Huan";

#define GE_DEBUG_ON 1

#define I2C_MASTER_SCL_IO    19    /*!< gpio number for I2C master clock */
#define I2C_MASTER_SDA_IO    18    /*!< gpio number for I2C master data  */
#define I2C_MASTER_NUM I2C_NUM_1   /*!< I2C port number for master dev */
#define I2C_MASTER_TX_BUF_DISABLE   0   /*!< I2C master do not need buffer */
#define I2C_MASTER_RX_BUF_DISABLE   0   /*!< I2C master do not need buffer */
#define I2C_MASTER_FREQ_HZ    100000     /*!< I2C master clock frequency */

#define SAMPLE_RATE     (44100)
#define I2S_NUM         (0)
#define WAVE_FREQ_HZ    (100)
#define PI 3.14159265

#define SAMPLE_PER_CYCLE (SAMPLE_RATE/WAVE_FREQ_HZ)

#define WIFI_SSID "GOLDELEC"
#define WIFI_PASS "gold2015++"

#define GE_DEBUG_ON 1

#define I2C_MASTER_SCL_IO    19    /*!< gpio number for I2C master clock */
#define I2C_MASTER_SDA_IO    18    /*!< gpio number for I2C master data  */
#define I2C_MASTER_NUM I2C_NUM_1   /*!< I2C port number for master dev */
#define I2C_MASTER_TX_BUF_DISABLE   0   /*!< I2C master do not need buffer */
#define I2C_MASTER_RX_BUF_DISABLE   0   /*!< I2C master do not need buffer */
#define I2C_MASTER_FREQ_HZ    100000     /*!< I2C master clock frequency */

#define SAMPLE_RATE     (44100)
#define I2S_NUM         (0)
#define WAVE_FREQ_HZ    (100)
#define PI 3.14159265

#define SAMPLE_PER_CYCLE (SAMPLE_RATE/WAVE_FREQ_HZ)

#define WIFI_SSID "GOLDELEC"
#define WIFI_PASS "gold2015++"


void i2c_master_init()
{
    int i2c_master_port = I2C_MASTER_NUM;
    i2c_config_t conf;
    conf.mode = I2C_MODE_MASTER;
    conf.sda_io_num = (gpio_num_t)I2C_MASTER_SDA_IO;
    conf.sda_pullup_en = GPIO_PULLUP_ENABLE;
    conf.scl_io_num = (gpio_num_t)I2C_MASTER_SCL_IO;
    conf.scl_pullup_en = GPIO_PULLUP_ENABLE;
    conf.master.clk_speed = I2C_MASTER_FREQ_HZ;
    i2c_param_config((i2c_port_t)i2c_master_port, &conf);
    i2c_driver_install((i2c_port_t)i2c_master_port, conf.mode, I2C_MASTER_RX_BUF_DISABLE, I2C_MASTER_TX_BUF_DISABLE, 0);
}


static esp_err_t event_handler(void *ctx, system_event_t *event)
{
    switch(event->event_id) {
    case SYSTEM_EVENT_STA_START:
        esp_wifi_connect();
        break;
    case SYSTEM_EVENT_STA_GOT_IP:
        xEventGroupSetBits(wifi_event_group, CONNECTED_BIT);
        break;
    case SYSTEM_EVENT_STA_DISCONNECTED:
        /* This is a workaround as ESP32 WiFi libs don't currently
           auto-reassociate. */
        esp_wifi_connect();
        xEventGroupClearBits(wifi_event_group, CONNECTED_BIT);
        break;
    default:
        break;
    }
    return ESP_OK;
}


static void initialise_wifi(void)
{
    tcpip_adapter_init();
    wifi_event_group = xEventGroupCreate();
    ESP_ERROR_CHECK( esp_event_loop_init(event_handler, NULL) );
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK( esp_wifi_init(&cfg) );
    ESP_ERROR_CHECK( esp_wifi_set_storage(WIFI_STORAGE_RAM) );

    wifi_config_t wifi_config;

    memcpy(wifi_config.sta.ssid, WIFI_SSID, strlen(WIFI_SSID) + 1);
    memcpy(wifi_config.sta.password, WIFI_PASS, strlen(WIFI_PASS) + 1);

    ESP_LOGI(TAG, "Setting WiFi configuration SSID %s...", wifi_config.sta.ssid);
    ESP_ERROR_CHECK( esp_wifi_set_mode(WIFI_MODE_STA) );
    ESP_ERROR_CHECK( esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config) );
    ESP_ERROR_CHECK( esp_wifi_start() );
}


static void audio_ouput_task(void *pvParameters) {
	unsigned int sample_val;
	float sin_float, triangle_float, triangle_step = 65536.0 / SAMPLE_PER_CYCLE;

	//for 36Khz sample rates, we create 100Hz sine wave, every cycle need 36000/100 = 360 samples (4-bytes each sample)
	//using 6 buffers, we need 60-samples per buffer
	//2-channels, 16-bit each channel, total buffer is 360*4 = 1440 bytes
	int mode = I2S_MODE_MASTER | I2S_MODE_TX;

	i2s_config_t i2s_config = {
				.mode = (i2s_mode_t) mode,            // Only TX
				.sample_rate = SAMPLE_RATE,
				.bits_per_sample = (i2s_bits_per_sample_t) 16,          //16-bit per channel
				.channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,           //2-channels
				.communication_format = I2S_COMM_FORMAT_I2S,
				.intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,               //Interrupt level 1
				.dma_buf_count = 6,
				.dma_buf_len = 60                            //
			};

	i2s_pin_config_t pin_config = {
			.bck_io_num = 26,
			.ws_io_num = 25,
			.data_out_num = 22,
			.data_in_num = -1                    //Not used
			};

	i2s_driver_install((i2s_port_t) I2S_NUM, &i2s_config, 0, NULL);
	i2s_set_pin((i2s_port_t) I2S_NUM, &pin_config);

	i2s_set_sample_rates((i2s_port_t) I2S_NUM, 22050);

	triangle_float = -32767;

	while (1) {
		for (int i = 0; i < SAMPLE_PER_CYCLE; i++) {
			sin_float = sin(i * PI / 180.0);
			if (sin_float >= 0)
				triangle_float += triangle_step;
			else
				triangle_float -= triangle_step;
			sin_float *= 32767;

			sample_val = 0;
			sample_val += (short) triangle_float;
			sample_val = sample_val << 16;
			sample_val += (short) sin_float;

			int ret = i2s_push_sample((i2s_port_t) I2S_NUM, (char *) &sample_val, portMAX_DELAY);

			//ESP_LOGI(TAG, "i2s_push_sample return:%d", ret);
		}
		vTaskDelay(1000 / portTICK_PERIOD_MS);
	}
}


static void http2_client_task(void *pvParameters) {
	xEventGroupWaitBits(wifi_event_group, CONNECTED_BIT, false, true, portMAX_DELAY);
	ESP_LOGI(TAG, "Connected to AP");

	Http2Client *htt2pClient = new Http2Client();
	htt2pClient->begin("avs-alexa-na.amazon.com", "/", 443);
}


extern "C" void app_main() {
	nvs_flash_init();
	initialise_wifi();
	i2c_master_init();
	WM8978_Init();

	//xTaskCreate(&audio_ouput_task, "audio_ouput_task", 8192, NULL, 5, NULL);
	xTaskCreate(&http2_client_task, "http2_client_task", 8192, NULL, 5, NULL);
}
