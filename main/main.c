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

#include "driver/i2s.h"
#include <math.h>


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

extern const uint8_t server_root_cert_pem_start[] asm("_binary_server_root_cert_pem_start");
extern const uint8_t server_root_cert_pem_end[]   asm("_binary_server_root_cert_pem_end");

#ifdef MBEDTLS_DEBUG_C

#define MBEDTLS_DEBUG_LEVEL 4

/* mbedtls debug function that translates mbedTLS debug output
   to ESP_LOGx debug output.

   MBEDTLS_DEBUG_LEVEL 4 means all mbedTLS debug output gets sent here,
   and then filtered to the ESP logging mechanism.
*/
static void mbedtls_debug(void *ctx, int level,
                     const char *file, int line,
                     const char *str)
{
    const char *MBTAG = "mbedtls";
    char *file_sep;

    /* Shorten 'file' from the whole file path to just the filename

       This is a bit wasteful because the macros are compiled in with
       the full _FILE_ path in each case.
    */
    file_sep = rindex(file, '/');
    if(file_sep)
        file = file_sep+1;

    switch(level) {
    case 1:
        ESP_LOGI(MBTAG, "%s:%d %s", file, line, str);
        break;
    case 2:
    case 3:
        ESP_LOGD(MBTAG, "%s:%d %s", file, line, str);
    case 4:
        ESP_LOGV(MBTAG, "%s:%d %s", file, line, str);
        break;
    default:
        ESP_LOGE(MBTAG, "Unexpected log level %d: %s", level, str);
        break;
    }
}

#endif


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
    wifi_config_t wifi_config = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASS,
        },
    };
    ESP_LOGI(TAG, "Setting WiFi configuration SSID %s...", wifi_config.sta.ssid);
    ESP_ERROR_CHECK( esp_wifi_set_mode(WIFI_MODE_STA) );
    ESP_ERROR_CHECK( esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config) );
    ESP_ERROR_CHECK( esp_wifi_start() );
}

enum { IO_NONE, WANT_READ, WANT_WRITE };

#define MAKE_NV(NAME, VALUE)                                                   \
  {                                                                            \
    (uint8_t *)NAME, (uint8_t *)VALUE, sizeof(NAME) - 1, sizeof(VALUE) - 1,    \
        NGHTTP2_NV_FLAG_NONE                                                   \
  }

#define MAKE_NV_CS(NAME, VALUE)                                                \
  {                                                                            \
    (uint8_t *)NAME, (uint8_t *)VALUE, sizeof(NAME) - 1, strlen(VALUE),        \
        NGHTTP2_NV_FLAG_NONE                                                   \
  }

struct Connection {
  mbedtls_ssl_context *ssl;
  nghttp2_session *session;
  /* WANT_READ if SSL/TLS connection needs more input; or WANT_WRITE
     if it needs more output; or IO_NONE. This is necessary because
     SSL/TLS re-negotiation is possible at any time. nghttp2 API
     offers similar functions like nghttp2_session_want_read() and
     nghttp2_session_want_write() but they do not take into account
     SSL/TSL connection. */
  int want_io;
};

struct Request {
  char *host;
  /* In this program, path contains query component as well. */
  char *path;
  /* This is the concatenation of host and port with ":" in
     between. */
  char *hostport;
  /* Stream ID for this request. */
  int32_t stream_id;
  uint16_t port;
};

/*
 * The implementation of nghttp2_send_callback type. Here we write
 * |data| with size |length| to the network and return the number of
 * bytes actually written. See the documentation of
 * nghttp2_send_callback for the details.
 */
static ssize_t send_callback(nghttp2_session *session, const uint8_t *data,
                             size_t length, int flags, void *user_data) {
	ESP_LOGI(TAG, "nghttp2 event step6...");

  struct Connection *connection;
  int rv;
  connection = (struct Connection *)user_data;
  connection->want_io = IO_NONE;

  ESP_LOGI(TAG, "nghttp2 event step7...");

  //ERR_clear_error();
  rv = mbedtls_ssl_write(connection->ssl, data, (int)length);

  ESP_LOGI(TAG, "nghttp2 event step8...");

  if (rv <= 0) {
	  ESP_LOGI(TAG, "nghttp2 event step9...");

      if(rv != MBEDTLS_ERR_SSL_WANT_READ && rv != MBEDTLS_ERR_SSL_WANT_WRITE)
      {
    	  ESP_LOGI(TAG, "nghttp2 event step10...");

          ESP_LOGE(TAG, "mbedtls_ssl_write returned -0x%d", -rv);
          abort();
      }
  }
  return rv;
}

/*
 * The implementation of nghttp2_recv_callback type. Here we read data
 * from the network and write them in |buf|. The capacity of |buf| is
 * |length| bytes. Returns the number of bytes stored in |buf|. See
 * the documentation of nghttp2_recv_callback for the details.
 */
static ssize_t recv_callback(nghttp2_session *session, uint8_t *buf,
                             size_t length, int flags, void *user_data) {
  struct Connection *connection;
  int rv;
  connection = (struct Connection *)user_data;
  connection->want_io = IO_NONE;
  //ERR_clear_error();

  //#define MBEDTLS_ERR_SSL_PEER_CLOSE_NOTIFY                 -0x7880  /**< The peer notified us that the connection is going to be closed. */

  //rv = mbedtls_ssl_get_bytes_avail(connection->ssl);

 // if(rv > 0){
	  rv = mbedtls_ssl_read(connection->ssl, buf, (int)length);
  //}


  ESP_LOGI(TAG, "got data, data length:%d data:", length);

 // fwrite(buf, 1, length, stdout);

  if (rv < 0) {
      ESP_LOGE(TAG, "mbedtls_ssl_read returned -0x%x", -rv);
  } else if (rv == 0) {
	ESP_LOGI(TAG, "connection closed");
    rv = NGHTTP2_ERR_EOF;
  }
  else{
	ESP_LOGI(TAG, "mbedtls_ssl_read successed, return:%d", rv);
  }
  return rv;
}


static int on_frame_send_callback(nghttp2_session *session,
                                  const nghttp2_frame *frame,
                                  void *user_data) {
	ESP_LOGI(TAG, "nghttp2 event step4...");

  size_t i;
  switch (frame->hd.type) {
  case NGHTTP2_HEADERS:
    if (nghttp2_session_get_stream_user_data(session, frame->hd.stream_id)) {
      const nghttp2_nv *nva = frame->headers.nva;
      printf("[INFO] C ----------------------------> S (HEADERS)\n");
      for (i = 0; i < frame->headers.nvlen; ++i) {
        fwrite(nva[i].name, 1, nva[i].namelen, stdout);
        printf(": ");
        fwrite(nva[i].value, 1, nva[i].valuelen, stdout);
        printf("\n");
      }
    }
    break;
  case NGHTTP2_RST_STREAM:
    printf("[INFO] C ----------------------------> S (RST_STREAM)\n");
    break;
  case NGHTTP2_GOAWAY:
    printf("[INFO] C ----------------------------> S (GOAWAY)\n");
    break;
  }
  return 0;
}

static int on_frame_recv_callback(nghttp2_session *session,
                                  const nghttp2_frame *frame,
                                  void *user_data) {

	  ESP_LOGI(TAG, "on_frame_recv_callback step%d", 1);

  size_t i;
  switch (frame->hd.type) {
  case NGHTTP2_HEADERS:
    if (frame->headers.cat == NGHTTP2_HCAT_RESPONSE) {
      const nghttp2_nv *nva = frame->headers.nva;
      struct Request *req;
      req = (struct Request *)nghttp2_session_get_stream_user_data(session, frame->hd.stream_id);
      if (req) {
        printf("[INFO] C <---------------------------- S (HEADERS)\n");
        for (i = 0; i < frame->headers.nvlen; ++i) {
          fwrite(nva[i].name, 1, nva[i].namelen, stdout);
          printf(": ");
          fwrite(nva[i].value, 1, nva[i].valuelen, stdout);
          printf("\n");
        }
      }
    }
    break;
  case NGHTTP2_RST_STREAM:
    printf("[INFO] C <---------------------------- S (RST_STREAM)\n");
    break;
  case NGHTTP2_GOAWAY:
    printf("[INFO] C <---------------------------- S (GOAWAY)\n");
    break;
  default:
	  ESP_LOGI(TAG, "get frame type:%d", frame->hd.type);
  }
  return 0;
}

/*
 * The implementation of nghttp2_on_stream_close_callback type. We use
 * this function to know the response is fully received. Since we just
 * fetch 1 resource in this program, after reception of the response,
 * we submit GOAWAY and close the session.
 */
static int on_stream_close_callback(nghttp2_session *session, int32_t stream_id,
                                    uint32_t error_code,
                                    void *user_data) {
	ESP_LOGI(TAG, "on_stream_close_callback");

  struct Request *req;
  req = nghttp2_session_get_stream_user_data(session, stream_id);
  if (req) {
    int rv;
    rv = nghttp2_session_terminate_session(session, NGHTTP2_NO_ERROR);

    if (rv != 0) {
    	ESP_LOGE(TAG, "nghttp2_session_terminate_session:%d", rv);
    }
  }
  return 0;
}

/*
 * The implementation of nghttp2_on_data_chunk_recv_callback type. We
 * use this function to print the received response body.
 */
static int on_data_chunk_recv_callback(nghttp2_session *session,
                                       uint8_t flags, int32_t stream_id,
                                       const uint8_t *data, size_t len,
                                       void *user_data) {

	ESP_LOGI(TAG, "on_data_chunk_recv_callback");
  struct Request *req;
  req = nghttp2_session_get_stream_user_data(session, stream_id);
  if (req) {
    printf("[INFO] C <---------------------------- S (DATA chunk)\n"
           "%lu bytes\n",
           (unsigned long int)len);
    fwrite(data, 1, len, stdout);
    printf("\n");
  }
  return 0;
}


/*
 * Setup callback functions. nghttp2 API offers many callback
 * functions, but most of them are optional. The send_callback is
 * always required. Since we use nghttp2_session_recv(), the
 * recv_callback is also required.
 */
static void setup_nghttp2_callbacks(nghttp2_session_callbacks *callbacks) {
  nghttp2_session_callbacks_set_send_callback(callbacks, send_callback);

  nghttp2_session_callbacks_set_recv_callback(callbacks, recv_callback);


  nghttp2_session_callbacks_set_on_frame_send_callback(callbacks,
                                                       on_frame_send_callback);


  nghttp2_session_callbacks_set_on_frame_recv_callback(callbacks,
                                                       on_frame_recv_callback);

  nghttp2_session_callbacks_set_on_stream_close_callback(
      callbacks, on_stream_close_callback);

  nghttp2_session_callbacks_set_on_data_chunk_recv_callback(
      callbacks, on_data_chunk_recv_callback);
}

/*
 * Submits the request |req| to the connection |connection|.  This
 * function does not send packets; just append the request to the
 * internal queue in |connection->session|.
 */
static void submit_request(struct Connection *connection, struct Request *req) {
  int32_t stream_id;
  /* Make sure that the last item is NULL */
  const nghttp2_nv nva[] = {MAKE_NV(":method", "GET"),
                            MAKE_NV_CS(":path", req->path),
                            MAKE_NV(":scheme", "https"),
                            MAKE_NV_CS(":authority", req->host),
                            MAKE_NV("accept", "*/*"),
                            MAKE_NV("user-agent", "nghttp2/" NGHTTP2_VERSION)};

  stream_id = nghttp2_submit_request(connection->session, NULL, nva,
                                     sizeof(nva) / sizeof(nva[0]), NULL, req);

  if (stream_id < 0) {
	  ESP_LOGE(TAG, "nghttp2_submit_request:%d", stream_id);
	  abort();
  }

  req->stream_id = stream_id;
  printf("[INFO] Stream ID = %d\n", stream_id);
}


/*
 * Performs the network I/O.
 */
static void handleIo(struct Connection *connection) {
	int result;

	if (nghttp2_session_want_write(connection->session)) {
		result = nghttp2_session_send(connection->session);

		if (result != 0) {
#if GE_DEBUG_ON
			if (result == NGHTTP2_ERR_CALLBACK_FAILURE) {
				ESP_LOGI(TAG,
						"nghttp2_session_send result: NGHTTP2_ERR_CALLBACK_FAILURE");
			} else if (result == NGHTTP2_ERR_CALLBACK_FAILURE) {
				ESP_LOGI(TAG,
						"nghttp2_session_send result: NGHTTP2_ERR_CALLBACK_FAILURE");
			} else {
				ESP_LOGI(TAG, "nghttp2_session_send result: Unknown");
			}
#endif
			ESP_LOGI(TAG, "***nghttp2_session_send:%d", result);
		}
	}

	vTaskDelay(3000 / portTICK_PERIOD_MS);

	if (!nghttp2_session_want_read(connection->session)) {
		//ESP_LOGI(TAG, "nghttp2_session_want_read: not data to read");
		return;
	}

	//if (mbedtls_ssl_get_bytes_avail(connection->ssl) > 0) {
		result = nghttp2_session_recv(connection->session);

		if (result != 0) {
#if GE_DEBUG_ON
			switch (result) {
			case NGHTTP2_ERR_EOF:
				ESP_LOGI(TAG,
						"nghttp2_session_recv result: NGHTTP2_ERR_EOF. The peer performed a shutdown on the connection");
				break;
			case NGHTTP2_ERR_NOMEM:
				ESP_LOGI(TAG,
						"nghttp2_session_recv result: NGHTTP2_ERR_NOMEM.");
				break;
			case NGHTTP2_ERR_CALLBACK_FAILURE:
				ESP_LOGI(TAG,
						"nghttp2_session_recv result: NGHTTP2_ERR_CALLBACK_FAILURE.");
				break;
			case NGHTTP2_ERR_FLOODED:
				ESP_LOGI(TAG,
						"nghttp2_session_recv result: NGHTTP2_ERR_FLOODED.");
				break;
			default:
				ESP_LOGI(TAG, "nghttp2_session_recv result: Unknown");
				break;
			}
#endif
			ESP_LOGI(TAG, "***nghttp2_session_recv:%d", result);
		}
	//} else {
		//ESP_LOGI(TAG, "mbedtls_ssl_get_bytes_avail: not data to available");
	//}
}


struct Connection connection;

static void initNghttp2(){
	struct Request req = {
			"avs-alexa-na.amazon.com",
			"/",
			"avs-alexa-na.amazon.com:443",
			-1,
			"443"
	};

	nghttp2_session_callbacks *callbacks;

	int rv = nghttp2_session_callbacks_new(&callbacks);

	if (rv != 0) {
		ESP_LOGE(TAG, "nghttp2_session_callbacks_new failed: %d", rv);
		abort();
	}

	setup_nghttp2_callbacks(callbacks);

	rv = nghttp2_session_client_new(&connection.session, callbacks, &connection);

	nghttp2_session_callbacks_del(callbacks);

	rv = nghttp2_submit_settings(connection.session, NGHTTP2_FLAG_NONE, NULL, 0);

	if (rv != 0) {
		ESP_LOGE(TAG, "nghttp2_submit_settings failed:%d", rv);		    abort();
	}

	/* Submit the HTTP request to the outbound queue. */
	submit_request(&connection, &req);
}

static void audio_ouput_task(void *pvParameters){
    unsigned int sample_val;
     float sin_float, triangle_float, triangle_step = 65536.0 / SAMPLE_PER_CYCLE;

     //for 36Khz sample rates, we create 100Hz sine wave, every cycle need 36000/100 = 360 samples (4-bytes each sample)
     //using 6 buffers, we need 60-samples per buffer
     //2-channels, 16-bit each channel, total buffer is 360*4 = 1440 bytes
     i2s_config_t i2s_config = {
         .mode = I2S_MODE_MASTER | I2S_MODE_TX,                                  // Only TX
         .sample_rate = SAMPLE_RATE,
         .bits_per_sample = 16,                                                  //16-bit per channel
         .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,                           //2-channels
         .communication_format = I2S_COMM_FORMAT_I2S,
         .dma_buf_count = 6,
         .dma_buf_len = 60,                                                      //
         .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1                                //Interrupt level 1
     };
     i2s_pin_config_t pin_config = {
         .bck_io_num = 26,
         .ws_io_num = 25,
         .data_out_num = 22,
         .data_in_num = -1                                                       //Not used
     };

     i2s_driver_install(I2S_NUM, &i2s_config, 0, NULL);
     i2s_set_pin(I2S_NUM, &pin_config);

     i2s_set_sample_rates(I2S_NUM, 22050);

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

		int ret = i2s_push_sample(I2S_NUM, (char *) &sample_val, portMAX_DELAY);

		//ESP_LOGI(TAG, "i2s_push_sample return:%d", ret);
    }
		vTaskDelay(1000 / portTICK_PERIOD_MS);
	}
}


static void http2_client_task(void *pvParameters)
{
    char buf[512];
    int ret, flags, len;

    mbedtls_entropy_context entropy;
    mbedtls_ctr_drbg_context ctr_drbg;
    mbedtls_ssl_context ssl;
    mbedtls_x509_crt cacert;
    mbedtls_ssl_config conf;
    mbedtls_net_context server_fd;

    mbedtls_ssl_init(&ssl);
    mbedtls_x509_crt_init(&cacert);
    mbedtls_ctr_drbg_init(&ctr_drbg);
    ESP_LOGI(TAG, "Seeding the random number generator");

    mbedtls_ssl_config_init(&conf);

    mbedtls_entropy_init(&entropy);
    if((ret = mbedtls_ctr_drbg_seed(&ctr_drbg, mbedtls_entropy_func, &entropy,
                                    NULL, 0)) != 0)
    {
        ESP_LOGE(TAG, "mbedtls_ctr_drbg_seed returned %d", ret);
        abort();
    }

    ESP_LOGI(TAG, "Loading the CA root certificate...");

    //certify£ºhttps://s3.amazonaws.com/echo.api/echo-api-cert-3.pem
    ret = mbedtls_x509_crt_parse(&cacert, server_root_cert_pem_start,
                                 server_root_cert_pem_end-server_root_cert_pem_start);

    if(ret < 0)
    {
        ESP_LOGE(TAG, "mbedtls_x509_crt_parse returned -0x%x\n\n", -ret);
        abort();
    }

    ESP_LOGI(TAG, "Setting hostname for TLS session...");

     /* Hostname set here should match CN in server certificate */
    if((ret = mbedtls_ssl_set_hostname(&ssl, WEB_SERVER)) != 0)
    {
        ESP_LOGE(TAG, "mbedtls_ssl_set_hostname returned -0x%x", -ret);
        abort();
    }

    ESP_LOGI(TAG, "Setting up the SSL/TLS structure...");

    if((ret = mbedtls_ssl_config_defaults(&conf,
                                          MBEDTLS_SSL_IS_CLIENT,
                                          MBEDTLS_SSL_TRANSPORT_STREAM,
                                          MBEDTLS_SSL_PRESET_DEFAULT)) != 0)
    {
        ESP_LOGE(TAG, "mbedtls_ssl_config_defaults returned %d", ret);
        goto exit;
    }

    /* MBEDTLS_SSL_VERIFY_OPTIONAL is bad for security, in this example it will print
       a warning if CA verification fails but it will continue to connect.

       You should consider using MBEDTLS_SSL_VERIFY_REQUIRED in your own code.
    */
    mbedtls_ssl_conf_authmode(&conf, MBEDTLS_SSL_VERIFY_NONE);
    mbedtls_ssl_conf_ca_chain(&conf, &cacert, NULL);
    mbedtls_ssl_conf_rng(&conf, mbedtls_ctr_drbg_random, &ctr_drbg);
#ifdef MBEDTLS_DEBUG_C
    mbedtls_debug_set_threshold(MBEDTLS_DEBUG_LEVEL);
    mbedtls_ssl_conf_dbg(&conf, mbedtls_debug, NULL);
#endif

    if ((ret = mbedtls_ssl_setup(&ssl, &conf)) != 0)
    {
        ESP_LOGE(TAG, "mbedtls_ssl_setup returned -0x%x\n\n", -ret);
        goto exit;
    }

    while(1) {
        /* Wait for the callback to set the CONNECTED_BIT in the
           event group.
        */
        xEventGroupWaitBits(wifi_event_group, CONNECTED_BIT,
                            false, true, portMAX_DELAY);
        ESP_LOGI(TAG, "Connected to AP");

        mbedtls_net_init(&server_fd);

        ESP_LOGI(TAG, "Connecting to %s:%s...", WEB_SERVER, WEB_PORT);

        if ((ret = mbedtls_net_connect(&server_fd, WEB_SERVER,
                                      WEB_PORT, MBEDTLS_NET_PROTO_TCP)) != 0)
        {
            ESP_LOGE(TAG, "mbedtls_net_connect returned -%x", -ret);
            goto exit;
        }
        else{
        	ESP_LOGI(TAG, "mbedtls Connected.");
        }


        mbedtls_ssl_set_bio(&ssl, &server_fd, mbedtls_net_send, mbedtls_net_recv, NULL);

        ESP_LOGI(TAG, "Performing the SSL/TLS handshake...");

        while ((ret = mbedtls_ssl_handshake(&ssl)) != 0)
        {
            if (ret != MBEDTLS_ERR_SSL_WANT_READ && ret != MBEDTLS_ERR_SSL_WANT_WRITE)
            {
                ESP_LOGE(TAG, "mbedtls_ssl_handshake returned -0x%x", -ret);
                goto exit;
            }
            else{
            	ESP_LOGI(TAG, "SSL/TLS is handshaked");
            }
        }

        ESP_LOGI(TAG, "Verifying peer X.509 certificate...");

        if ((flags = mbedtls_ssl_get_verify_result(&ssl)) != 0)
        {
            /* In real life, we probably want to close connection if ret != 0 */
            ESP_LOGW(TAG, "Failed to verify peer certificate!");
            bzero(buf, sizeof(buf));
            mbedtls_x509_crt_verify_info(buf, sizeof(buf), "  ! ", flags);
            ESP_LOGW(TAG, "verification info: %s", buf);
        }
        else {
            ESP_LOGI(TAG, "X.509 Certificate verified.");
        }

        ESP_LOGI(TAG, "Writing HTTPs request...");

        initNghttp2();
        connection.ssl = &ssl;

        /* Event loop */
        while (nghttp2_session_want_read(connection.session) ||
               nghttp2_session_want_write(connection.session)) {
        	handleIo(&connection);
        }

		 /* Resource cleanup */
        nghttp2_session_del(connection.session);

        mbedtls_ssl_close_notify(&ssl);

    exit:
        mbedtls_ssl_session_reset(&ssl);
        mbedtls_net_free(&server_fd);

        if(ret != 0)
        {
            mbedtls_strerror(ret, buf, 100);
            ESP_LOGE(TAG, "Last error was: -0x%x - %s", -ret, buf);
        }

        for(int countdown = 10; countdown >= 0; countdown--) {
            ESP_LOGI(TAG, "%d...", countdown);
            vTaskDelay(1000 / portTICK_PERIOD_MS);
        }
        ESP_LOGI(TAG, "Starting again!");
    }
}


/**
 * @brief i2c master initialization
 */
void i2c_master_init()
{
    int i2c_master_port = I2C_MASTER_NUM;
    i2c_config_t conf;
    conf.mode = I2C_MODE_MASTER;
    conf.sda_io_num = I2C_MASTER_SDA_IO;
    conf.sda_pullup_en = GPIO_PULLUP_ENABLE;
    conf.scl_io_num = I2C_MASTER_SCL_IO;
    conf.scl_pullup_en = GPIO_PULLUP_ENABLE;
    conf.master.clk_speed = I2C_MASTER_FREQ_HZ;
    i2c_param_config(i2c_master_port, &conf);
    i2c_driver_install(i2c_master_port, conf.mode, I2C_MASTER_RX_BUF_DISABLE, I2C_MASTER_TX_BUF_DISABLE, 0);
}


void app_main() {
	nvs_flash_init();
	initialise_wifi();
	i2c_master_init();
	WM8978_Init();

	xTaskCreate(&audio_ouput_task, "audio_ouput_task", 8192, NULL, 5, NULL);
	//xTaskCreate(&http2_client_task, "http2_client_task", 8192, NULL, 5, NULL);
}
