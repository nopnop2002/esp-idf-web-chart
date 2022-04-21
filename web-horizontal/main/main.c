/*
	 Example using WEB Socket.
	 This example code is in the Public Domain (or CC0 licensed, at your option.)
	 Unless required by applicable law or agreed to in writing, this
	 software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
	 CONDITIONS OF ANY KIND, either express or implied.
*/

#include "string.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/event_groups.h"
#include "freertos/message_buffer.h"

#include "esp_wifi.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "cJSON.h"
#include "mdns.h"

#include "soc/adc_channel.h"
#include "driver/adc_common.h"
#include "esp_adc_cal.h"

#include "websocket_server.h"

static QueueHandle_t client_queue;
MessageBufferHandle_t xMessageBufferMain;

const static int client_queue_size = 10;

/* FreeRTOS event group to signal when we are connected*/
static EventGroupHandle_t s_wifi_event_group;

/* The event group allows multiple bits for each event, but we only care about two events:
 * - we are connected to the AP with an IP
 * - we failed to connect after the maximum amount of retries */
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT			 BIT1

static const char *TAG = "main";

static int s_retry_num = 0;

//ADC Attenuation
#define ADC_EXAMPLE_ATTEN	ADC_ATTEN_DB_11

//ADC Calibration
#if CONFIG_IDF_TARGET_ESP32
#define ADC_EXAMPLE_CALI_SCHEME	ESP_ADC_CAL_VAL_EFUSE_VREF
#elif CONFIG_IDF_TARGET_ESP32S2
#define ADC_EXAMPLE_CALI_SCHEME	ESP_ADC_CAL_VAL_EFUSE_TP
#elif CONFIG_IDF_TARGET_ESP32C3
#define ADC_EXAMPLE_CALI_SCHEME	ESP_ADC_CAL_VAL_EFUSE_TP
#elif CONFIG_IDF_TARGET_ESP32S3
#define ADC_EXAMPLE_CALI_SCHEME	ESP_ADC_CAL_VAL_EFUSE_TP_FIT
#endif

static esp_adc_cal_characteristics_t adc1_chars;

static void event_handler(void* arg, esp_event_base_t event_base,
													int32_t event_id, void* event_data)
{
	if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
		esp_wifi_connect();
	} else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
		if (s_retry_num < CONFIG_ESP_MAXIMUM_RETRY) {
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

void wifi_init_sta(void)
{
	s_wifi_event_group = xEventGroupCreate();

	ESP_ERROR_CHECK(esp_netif_init());

	ESP_ERROR_CHECK(esp_event_loop_create_default());
	esp_netif_create_default_wifi_sta();

	wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
	ESP_ERROR_CHECK(esp_wifi_init(&cfg));

	esp_event_handler_instance_t instance_any_id;
	esp_event_handler_instance_t instance_got_ip;
	ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
									ESP_EVENT_ANY_ID,
									&event_handler,
									NULL,
									&instance_any_id));
	ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
									IP_EVENT_STA_GOT_IP,
									&event_handler,
									NULL,
									&instance_got_ip));

	wifi_config_t wifi_config = {
		.sta = {
			.ssid = CONFIG_ESP_WIFI_SSID,
			.password = CONFIG_ESP_WIFI_PASSWORD,
			/* Setting a password implies station will connect to all security modes including WEP/WPA.
			 * However these modes are deprecated and not advisable to be used. Incase your Access point
			 * doesn't support WPA2, these mode can be enabled by commenting below line */
			.threshold.authmode = WIFI_AUTH_WPA2_PSK,

			.pmf_cfg = {
				.capable = true,
				.required = false
			},
		},
	};
	ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA) );
	ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config) );
	ESP_ERROR_CHECK(esp_wifi_start() );

	ESP_LOGI(TAG, "wifi_init_sta finished.");

	/* Waiting until either the connection is established (WIFI_CONNECTED_BIT) or connection failed for the maximum
	 * number of re-tries (WIFI_FAIL_BIT). The bits are set by event_handler() (see above) */
	EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
		WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
		pdFALSE,
		pdFALSE,
		portMAX_DELAY);

	/* xEventGroupWaitBits() returns the bits before the call returned, hence we can test which event actually
	 * happened. */
	if (bits & WIFI_CONNECTED_BIT) {
		ESP_LOGI(TAG, "connected to ap SSID:%s password:%s",
						 CONFIG_ESP_WIFI_SSID, CONFIG_ESP_WIFI_PASSWORD);
	} else if (bits & WIFI_FAIL_BIT) {
		ESP_LOGI(TAG, "Failed to connect to SSID:%s, password:%s",
						 CONFIG_ESP_WIFI_SSID, CONFIG_ESP_WIFI_PASSWORD);
	} else {
		ESP_LOGE(TAG, "UNEXPECTED EVENT");
	}

	/* The event will not be processed after unregister */
	ESP_ERROR_CHECK(esp_event_handler_instance_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, instance_got_ip));
	ESP_ERROR_CHECK(esp_event_handler_instance_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, instance_any_id));
	vEventGroupDelete(s_wifi_event_group);
}

void initialise_mdns(void)
{
	//initialize mDNS
	ESP_ERROR_CHECK( mdns_init() );
	//set mDNS hostname (required if you want to advertise services)
	ESP_ERROR_CHECK( mdns_hostname_set(CONFIG_MDNS_HOSTNAME) );
	ESP_LOGI(TAG, "mdns hostname set to: [%s]", CONFIG_MDNS_HOSTNAME);

#if 0
	//set default mDNS instance name
	ESP_ERROR_CHECK( mdns_instance_name_set("ESP32 with mDNS") );
#endif
}

// handles websocket events
void websocket_callback(uint8_t num,WEBSOCKET_TYPE_t type,char* msg,uint64_t len) {
	const static char* TAG = "websocket_callback";
	//int value;

	switch(type) {
		case WEBSOCKET_CONNECT:
			ESP_LOGI(TAG,"client %i connected!",num);
			break;
		case WEBSOCKET_DISCONNECT_EXTERNAL:
			ESP_LOGI(TAG,"client %i sent a disconnect message",num);
			break;
		case WEBSOCKET_DISCONNECT_INTERNAL:
			ESP_LOGI(TAG,"client %i was disconnected",num);
			break;
		case WEBSOCKET_DISCONNECT_ERROR:
			ESP_LOGE(TAG,"client %i was disconnected due to an error",num);
			break;
		case WEBSOCKET_TEXT:
			if(len) { // if the message length was greater than zero
				ESP_LOGI(TAG, "got message length %i: %s", (int)len, msg);
				//size_t xBytesSent = xMessageBufferSend(xMessageBufferMain, msg, len, portMAX_DELAY);
				BaseType_t xHigherPriorityTaskWoken = pdFALSE;
				size_t xBytesSent = xMessageBufferSendFromISR(xMessageBufferMain, msg, len, &xHigherPriorityTaskWoken);
				if (xBytesSent != len) {
					ESP_LOGE(TAG, "xMessageBufferSend fail");
				}
			}
			break;
		case WEBSOCKET_BIN:
			ESP_LOGI(TAG,"client %i sent binary message of size %i:\n%s",num,(uint32_t)len,msg);
			break;
		case WEBSOCKET_PING:
			ESP_LOGI(TAG,"client %i pinged us with message of size %i:\n%s",num,(uint32_t)len,msg);
			break;
		case WEBSOCKET_PONG:
			ESP_LOGI(TAG,"client %i responded to the ping",num);
			break;
	}
}

// serves any clients
static void http_serve(struct netconn *conn) {
	const static char* TAG = "http_server";
	const static char HTML_HEADER[] = "HTTP/1.1 200 OK\nContent-type: text/html\n\n";
	const static char ERROR_HEADER[] = "HTTP/1.1 404 Not Found\nContent-type: text/html\n\n";
	const static char JS_HEADER[] = "HTTP/1.1 200 OK\nContent-type: text/javascript\n\n";
	const static char CSS_HEADER[] = "HTTP/1.1 200 OK\nContent-type: text/css\n\n";
	//const static char PNG_HEADER[] = "HTTP/1.1 200 OK\nContent-type: image/png\n\n";
	const static char ICO_HEADER[] = "HTTP/1.1 200 OK\nContent-type: image/x-icon\n\n";
	//const static char PDF_HEADER[] = "HTTP/1.1 200 OK\nContent-type: application/pdf\n\n";
	//const static char EVENT_HEADER[] = "HTTP/1.1 200 OK\nContent-Type: text/event-stream\nCache-Control: no-cache\nretry: 3000\n\n";
	struct netbuf* inbuf;
	static char* buf;
	static uint16_t buflen;
	static err_t err;

	// default page
	extern const uint8_t root_html_start[] asm("_binary_root_html_start");
	extern const uint8_t root_html_end[] asm("_binary_root_html_end");
	const uint32_t root_html_len = root_html_end - root_html_start;

	// main.js
	extern const uint8_t main_js_start[] asm("_binary_main_js_start");
	extern const uint8_t main_js_end[] asm("_binary_main_js_end");
	const uint32_t main_js_len = main_js_end - main_js_start;

	// gauge.min.js
	extern const uint8_t gauge_min_js_start[] asm("_binary_gauge_min_js_start");
	extern const uint8_t gauge_min_js_end[] asm("_binary_gauge_min_js_end");
	const uint32_t gauge_min_js_len = gauge_min_js_end - gauge_min_js_start;

	// main.css
	extern const uint8_t main_css_start[] asm("_binary_main_css_start");
	extern const uint8_t main_css_end[] asm("_binary_main_css_end");
	const uint32_t main_css_len = main_css_end - main_css_start;

	// favicon.ico
	extern const uint8_t favicon_ico_start[] asm("_binary_favicon_ico_start");
	extern const uint8_t favicon_ico_end[] asm("_binary_favicon_ico_end");
	const uint32_t favicon_ico_len = favicon_ico_end - favicon_ico_start;

	// error page
	extern const uint8_t error_html_start[] asm("_binary_error_html_start");
	extern const uint8_t error_html_end[] asm("_binary_error_html_end");
	const uint32_t error_html_len = error_html_end - error_html_start;

	netconn_set_recvtimeout(conn,1000); // allow a connection timeout of 1 second
	ESP_LOGI(TAG,"reading from client...");
	err = netconn_recv(conn, &inbuf);
	ESP_LOGI(TAG,"read from client");
	if(err==ERR_OK) {
		netbuf_data(inbuf, (void**)&buf, &buflen);
		if(buf) {

			ESP_LOGD(TAG, "buf=[%s]", buf);
			// default page
			if		 (strstr(buf,"GET / ")
					&& !strstr(buf,"Upgrade: websocket")) {
				ESP_LOGI(TAG,"Sending /");
				netconn_write(conn, HTML_HEADER, sizeof(HTML_HEADER)-1,NETCONN_NOCOPY);
				netconn_write(conn, root_html_start,root_html_len,NETCONN_NOCOPY);
				netconn_close(conn);
				netconn_delete(conn);
				netbuf_delete(inbuf);
			}

			// default page websocket
			else if(strstr(buf,"GET / ")
					 && strstr(buf,"Upgrade: websocket")) {
				ESP_LOGI(TAG,"Requesting websocket on /");
				ws_server_add_client(conn,buf,buflen,"/",websocket_callback);
				netbuf_delete(inbuf);
			}

			else if(strstr(buf,"GET /main.js ")) {
				ESP_LOGI(TAG,"Sending /main.js");
				netconn_write(conn, JS_HEADER, sizeof(JS_HEADER)-1,NETCONN_NOCOPY);
				netconn_write(conn, main_js_start, main_js_len,NETCONN_NOCOPY);
				netconn_close(conn);
				netconn_delete(conn);
				netbuf_delete(inbuf);
			}

			else if(strstr(buf,"GET /gauge.min.js ")) {
				ESP_LOGI(TAG,"Sending /gauge_min.js");
				netconn_write(conn, JS_HEADER, sizeof(JS_HEADER)-1,NETCONN_NOCOPY);
				netconn_write(conn, gauge_min_js_start, gauge_min_js_len,NETCONN_NOCOPY);
				netconn_close(conn);
				netconn_delete(conn);
				netbuf_delete(inbuf);
			}

			else if(strstr(buf,"GET /main.css ")) {
				ESP_LOGI(TAG,"Sending /main.css");
				netconn_write(conn, CSS_HEADER, sizeof(CSS_HEADER)-1,NETCONN_NOCOPY);
				netconn_write(conn, main_css_start, main_css_len,NETCONN_NOCOPY);
				netconn_close(conn);
				netconn_delete(conn);
				netbuf_delete(inbuf);
			}

			else if(strstr(buf,"GET /favicon.ico ")) {
				ESP_LOGI(TAG,"Sending favicon.ico");
				netconn_write(conn,ICO_HEADER,sizeof(ICO_HEADER)-1,NETCONN_NOCOPY);
				netconn_write(conn,favicon_ico_start,favicon_ico_len,NETCONN_NOCOPY);
				netconn_close(conn);
				netconn_delete(conn);
				netbuf_delete(inbuf);
			}

			else if(strstr(buf,"GET /")) {
				ESP_LOGE(TAG,"Unknown request, sending error page: %s",buf);
				netconn_write(conn, ERROR_HEADER, sizeof(ERROR_HEADER)-1,NETCONN_NOCOPY);
				netconn_write(conn, error_html_start, error_html_len,NETCONN_NOCOPY);
				netconn_close(conn);
				netconn_delete(conn);
				netbuf_delete(inbuf);
			}

			else {
				ESP_LOGE(TAG,"Unknown request");
				netconn_close(conn);
				netconn_delete(conn);
				netbuf_delete(inbuf);
			}
		}
		else {
			ESP_LOGI(TAG,"Unknown request (empty?...)");
			netconn_close(conn);
			netconn_delete(conn);
			netbuf_delete(inbuf);
		}
	}
	else { // if err==ERR_OK
		ESP_LOGI(TAG,"error on read, closing connection");
		netconn_close(conn);
		netconn_delete(conn);
		netbuf_delete(inbuf);
	}
}

// handles clients when they first connect. passes to a queue
static void server_task(void* pvParameters) {
	const static char* TAG = "server_task";
	char *task_parameter = (char *)pvParameters;
	ESP_LOGI(TAG, "Start task_parameter=%s", task_parameter);
	char url[64];
	sprintf(url, "http://%s", task_parameter);
	ESP_LOGI(TAG, "Starting server on %s", url);

	struct netconn *conn, *newconn;
	static err_t err;
	client_queue = xQueueCreate(client_queue_size,sizeof(struct netconn*));
	configASSERT( client_queue );

	conn = netconn_new(NETCONN_TCP);
	netconn_bind(conn,NULL,80);
	netconn_listen(conn);
	ESP_LOGI(TAG,"server listening");
	do {
		err = netconn_accept(conn, &newconn);
		ESP_LOGI(TAG,"new client");
		if(err == ERR_OK) {
			xQueueSendToBack(client_queue,&newconn,portMAX_DELAY);
			//http_serve(newconn);
		}
	} while(err == ERR_OK);
	netconn_close(conn);
	netconn_delete(conn);
	ESP_LOGE(TAG,"task ending, rebooting board");
	esp_restart();
}

// receives clients from queue, handles them
static void server_handle_task(void* pvParameters) {
	const static char* TAG = "server_handle_task";
	struct netconn* conn;
	ESP_LOGI(TAG,"task starting");
	for(;;) {
		xQueueReceive(client_queue,&conn,portMAX_DELAY);
		if(!conn) continue;
		http_serve(conn);
	}
	vTaskDelete(NULL);
}

static bool adc_calibration_init(void)
{
	esp_err_t ret;
	bool cali_enable = false;

	ret = esp_adc_cal_check_efuse(ADC_EXAMPLE_CALI_SCHEME);
	if (ret == ESP_ERR_NOT_SUPPORTED) {
		ESP_LOGW(TAG, "Calibration scheme not supported, skip software calibration");
	} else if (ret == ESP_ERR_INVALID_VERSION) {
		ESP_LOGW(TAG, "eFuse not burnt, skip software calibration");
	} else if (ret == ESP_OK) {
		cali_enable = true;
		esp_adc_cal_characterize(ADC_UNIT_1, ADC_EXAMPLE_ATTEN, ADC_WIDTH_BIT_DEFAULT, 0, &adc1_chars);
	} else {
		ESP_LOGE(TAG, "Invalid arg");
	}

	return cali_enable;
}

// Timer callback
static void timer_cb(TimerHandle_t xTimer)
{
	TickType_t nowTick;
	nowTick = xTaskGetTickCount();
	ESP_LOGD(TAG, "timer is called, now=%d",nowTick);

	cJSON *request;
	request = cJSON_CreateObject();
	cJSON_AddStringToObject(request, "id", "timer-request");
	char *my_json_string = cJSON_Print(request);
	ESP_LOGD(TAG, "my_json_string\n%s",my_json_string);
	//size_t xBytesSent = xMessageBufferSend(xMessageBufferMain, my_json_string, strlen(my_json_string), portMAX_DELAY);
	BaseType_t xHigherPriorityTaskWoken = pdFALSE;
	size_t xBytesSent = xMessageBufferSendFromISR(xMessageBufferMain, my_json_string, strlen(my_json_string), &xHigherPriorityTaskWoken);
	if (xBytesSent != strlen(my_json_string)) {
		ESP_LOGE(TAG, "xMessageBufferSend fail");
	}
	cJSON_Delete(request);
	cJSON_free(my_json_string);
}

// convert from gpio to adc1 channel
adc1_channel_t gpio2adc(int gpio) {
#if CONFIG_IDF_TARGET_ESP32
	if (gpio == 32) return ADC1_GPIO32_CHANNEL;
	if (gpio == 33) return ADC1_GPIO33_CHANNEL;
	if (gpio == 34) return ADC1_GPIO34_CHANNEL;
	if (gpio == 35) return ADC1_GPIO35_CHANNEL;
	if (gpio == 36) return ADC1_GPIO36_CHANNEL;
	if (gpio == 37) return ADC1_GPIO37_CHANNEL;
	if (gpio == 38) return ADC1_GPIO38_CHANNEL;
	if (gpio == 39) return ADC1_GPIO39_CHANNEL;

#elif CONFIG_IDF_TARGET_ESP32S2 || CONFIG_IDF_TARGET_ESP32S3
	if (gpio == 1) return ADC1_GPIO1_CHANNEL;
	if (gpio == 2) return ADC1_GPIO2_CHANNEL;
	if (gpio == 3) return ADC1_GPIO3_CHANNEL;
	if (gpio == 4) return ADC1_GPIO4_CHANNEL;
	if (gpio == 5) return ADC1_GPIO5_CHANNEL;
	if (gpio == 6) return ADC1_GPIO6_CHANNEL;
	if (gpio == 7) return ADC1_GPIO7_CHANNEL;
	if (gpio == 8) return ADC1_GPIO8_CHANNEL;
	if (gpio == 9) return ADC1_GPIO9_CHANNEL;
	if (gpio == 10) return ADC1_GPIO10_CHANNEL;

#elif CONFIG_IDF_TARGET_ESP32C3
	if (gpio == 0) return ADC1_GPIO0_CHANNEL;
	if (gpio == 1) return ADC1_GPIO1_CHANNEL;
	if (gpio == 2) return ADC1_GPIO2_CHANNEL;
	if (gpio == 3) return ADC1_GPIO3_CHANNEL;
	if (gpio == 4) return ADC1_GPIO4_CHANNEL;

#endif
	return -1;
}

#define NO_OF_SAMPLES		64					//Multisampling

void app_main() {
	//Initialize NVS
	esp_err_t ret = nvs_flash_init();
	if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
		ESP_ERROR_CHECK(nvs_flash_erase());
		ret = nvs_flash_init();
	}
	ESP_ERROR_CHECK(ret);

	ESP_LOGI(TAG, "ESP_WIFI_MODE_STA");
	wifi_init_sta();
	initialise_mdns();

	// Create Message Buffer
	xMessageBufferMain = xMessageBufferCreate(1024);
	configASSERT( xMessageBufferMain );

	/* Get the local IP address */
	//tcpip_adapter_ip_info_t ip_info;
	//ESP_ERROR_CHECK(tcpip_adapter_get_ip_info(TCPIP_ADAPTER_IF_STA, &ip_info));
	esp_netif_ip_info_t ip_info;
	ESP_ERROR_CHECK(esp_netif_get_ip_info(esp_netif_get_handle_from_ifkey("WIFI_STA_DEF"), &ip_info));
	char cparam0[64];
	//sprintf(cparam0, "%s", ip4addr_ntoa(&ip_info.ip));
	sprintf(cparam0, IPSTR, IP2STR(&ip_info.ip));

	ws_server_start();
	xTaskCreate(&server_task, "server_task", 1024*2, (void *)cparam0, 9, NULL);
	xTaskCreate(&server_handle_task, "server_handle_task", 1024*3, NULL, 6, NULL);

	// Create Timer (Trigger a measurement every second)
	ESP_LOGI(TAG, "CONFIG_ADC_CYCLE=%d", CONFIG_ADC_CYCLE);
	TimerHandle_t timerHandle = xTimerCreate("MY Trigger", CONFIG_ADC_CYCLE, pdTRUE, NULL, timer_cb);
	if (timerHandle != NULL) {
		if (xTimerStart(timerHandle, 0) != pdPASS) {
			ESP_LOGE(TAG, "Unable to start Timer");
			while(1) { vTaskDelay(1); }
		} else {
			ESP_LOGI(TAG, "Success to start Timer");
		}
	} else {
		ESP_LOGE(TAG, "Unable to create Timer");
		while(1) { vTaskDelay(1); }
	}


	bool cali_enable = adc_calibration_init();
	if (cali_enable == false) {
		ESP_LOGE(TAG, "calibration fail");
		while(1) { vTaskDelay(1); }
	}

	//ADC1 config
	ESP_LOGI(TAG, "CONFIG_METER1_GPIO=%d", CONFIG_METER1_GPIO);
	adc1_channel_t adc1_channel1 = gpio2adc(CONFIG_METER1_GPIO);
	ESP_LOGI(TAG, "adc1_channel1=%d", adc1_channel1);
	ESP_ERROR_CHECK(adc1_config_width(ADC_WIDTH_BIT_DEFAULT));
	ESP_ERROR_CHECK(adc1_config_channel_atten(adc1_channel1, ADC_EXAMPLE_ATTEN));
	char meter1[8];
	sprintf(meter1, "GPIO%02d", CONFIG_METER1_GPIO);

	char meter2[8];
	memset(meter2, 0, sizeof(meter2));
#if CONFIG_ENABLE_METER2
	ESP_LOGI(TAG, "CONFIG_METER2_GPIO=%d", CONFIG_METER2_GPIO);
	adc1_channel_t adc1_channel2 = gpio2adc(CONFIG_METER2_GPIO);
	ESP_LOGI(TAG, "adc1_channel2=%d", adc1_channel2);
	ESP_ERROR_CHECK(adc1_config_channel_atten(adc1_channel2, ADC_EXAMPLE_ATTEN));
	sprintf(meter2, "GPIO%02d", CONFIG_METER2_GPIO);
#endif

	char meter3[8];
	memset(meter3, 0, sizeof(meter3));
#if CONFIG_ENABLE_METER3
	ESP_LOGI(TAG, "CONFIG_METER3_GPIO=%d", CONFIG_METER3_GPIO);
	adc1_channel_t adc1_channel3 = gpio2adc(CONFIG_METER3_GPIO);
	ESP_LOGI(TAG, "adc1_channel3=%d", adc1_channel3);
	ESP_ERROR_CHECK(adc1_config_channel_atten(adc1_channel3, ADC_EXAMPLE_ATTEN));
	sprintf(meter3, "GPIO%02d", CONFIG_METER3_GPIO);
#endif

	char cRxBuffer[512];
	char DEL = 0x04;
	char outBuffer[64];

	while (1) {
		size_t readBytes = xMessageBufferReceive(xMessageBufferMain, cRxBuffer, sizeof(cRxBuffer), portMAX_DELAY );
		ESP_LOGD(TAG, "readBytes=%d", readBytes);
		cJSON *root = cJSON_Parse(cRxBuffer);
		if (cJSON_GetObjectItem(root, "id")) {
			char *id = cJSON_GetObjectItem(root,"id")->valuestring;
			ESP_LOGD(TAG, "id=%s",id);

			if ( strcmp (id, "init") == 0) {
				//sprintf(outBuffer,"HEAD%cADC Channel is %d", DEL, adc1_channel1);
				sprintf(outBuffer,"HEAD%cAnalog Value Display using ESP32", DEL);
				ESP_LOGD(TAG, "outBuffer=[%s]", outBuffer);
				ws_server_send_text_all(outBuffer,strlen(outBuffer));

				sprintf(outBuffer,"METER%c%s%c%s%c%s", DEL,meter1,DEL,meter2,DEL,meter3);
				ESP_LOGD(TAG, "outBuffer=[%s]", outBuffer);
				ws_server_send_text_all(outBuffer,strlen(outBuffer));
			}

			if ( strcmp (id, "timer-request") == 0) {
		
#if 0
				//Single sampling
				int adc_raw;
				adc_raw = adc1_get_raw(adc1_channel1);
				if (cali_enable) {
					uint32_t voltage = esp_adc_cal_raw_to_voltage(adc_raw, &adc1_chars);
					ESP_LOGI(TAG, "adc1_channel1: %d Raw: %d Voltage: %dmV", adc1_channel1, adc_raw, voltage);
					sprintf(outBuffer,"DATA%c%d", DEL, voltage);
					ESP_LOGD(TAG, "outBuffer=[%s]", outBuffer);
					ws_server_send_text_all(outBuffer,strlen(outBuffer));
				} else {
					ESP_LOGW(TAG, "calibration fail");
				}

#endif

				//Multi sampling
				uint32_t adc_reading1 = 0;
				uint32_t voltage1 = 0;
				for (int i = 0; i < NO_OF_SAMPLES; i++) {
					adc_reading1 += adc1_get_raw(adc1_channel1);
				}
				adc_reading1 /= NO_OF_SAMPLES;
				voltage1 = esp_adc_cal_raw_to_voltage(adc_reading1, &adc1_chars);
#if CONFIG_ENABLE_STDOUT
				ESP_LOGI(TAG, "GPIO%02d adc1_channel1: %d Raw: %d Voltage: %dmV", CONFIG_METER1_GPIO, adc1_channel1, adc_reading1, voltage1);
#endif

				uint32_t voltage2 = 0;
#if CONFIG_ENABLE_METER2
				uint32_t adc_reading2 = 0;
				for (int i = 0; i < NO_OF_SAMPLES; i++) {
					adc_reading2 += adc1_get_raw(adc1_channel2);
				}
				adc_reading2 /= NO_OF_SAMPLES;
				voltage2 = esp_adc_cal_raw_to_voltage(adc_reading2, &adc1_chars);
#if CONFIG_ENABLE_STDOUT
				ESP_LOGI(TAG, "GPIO%02d adc1_channel2: %d Raw: %d Voltage: %dmV", CONFIG_METER2_GPIO, adc1_channel2, adc_reading2, voltage2);
#endif
#endif // CONFIG_ENABLE_METER2

				uint32_t voltage3 = 0;
#if CONFIG_ENABLE_METER3
				uint32_t adc_reading3 = 0;
				for (int i = 0; i < NO_OF_SAMPLES; i++) {
					adc_reading3 += adc1_get_raw(adc1_channel3);
				}
				adc_reading3 /= NO_OF_SAMPLES;
				voltage3 = esp_adc_cal_raw_to_voltage(adc_reading3, &adc1_chars);
#if CONFIG_ENABLE_STDOUT
				ESP_LOGI(TAG, "GPIO%02d adc1_channel3: %d Raw: %d Voltage: %dmV", CONFIG_METER3_GPIO, adc1_channel3, adc_reading3, voltage3);
#endif
#endif // CONFIG_ENABLE_METER3

				sprintf(outBuffer,"DATA%c%d%c%d%c%d", DEL, voltage1, DEL, voltage2, DEL, voltage3);
				ESP_LOGD(TAG, "outBuffer=[%s]", outBuffer);
				ws_server_send_text_all(outBuffer,strlen(outBuffer));

				ESP_LOGD(TAG,"free_size:%d %d", heap_caps_get_free_size(MALLOC_CAP_8BIT), heap_caps_get_free_size(MALLOC_CAP_32BIT));


			} // end if
		} // end if

		// Delete a cJSON structure
		cJSON_Delete(root);

	
	} // end while
}
