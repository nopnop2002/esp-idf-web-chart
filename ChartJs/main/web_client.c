/*
	Example using WEB Socket.
	This example code is in the Public Domain (or CC0 licensed, at your option.)
	Unless required by applicable law or agreed to in writing, this
	software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
	CONDITIONS OF ANY KIND, either express or implied.
*/

#include <stdio.h>
#include <inttypes.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/event_groups.h"
#include "freertos/message_buffer.h"
#include "esp_log.h"
#include "cJSON.h"

static const char *TAG = "web_client";

#include "soc/adc_channel.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "esp_adc/adc_oneshot.h"

#include "websocket_server.h"

extern MessageBufferHandle_t xMessageBufferToClient;

// Timer callback
static void timer_cb(TimerHandle_t xTimer)
{
	TickType_t nowTick;
	nowTick = xTaskGetTickCount();
	ESP_LOGD(TAG, "timer is called, now=%"PRIu32, nowTick);

	cJSON *request;
	request = cJSON_CreateObject();
	cJSON_AddStringToObject(request, "id", "timer-request");
	char *my_json_string = cJSON_Print(request);
	ESP_LOGD(TAG, "my_json_string\n%s",my_json_string);
	size_t xBytesSent = xMessageBufferSendFromISR(xMessageBufferToClient, my_json_string, strlen(my_json_string), NULL);
	if (xBytesSent != strlen(my_json_string)) {
		ESP_LOGE(TAG, "xMessageBufferSend fail");
	}
	cJSON_Delete(request);
	cJSON_free(my_json_string);
}

// convert from gpio to adc1 channel
adc_channel_t gpio2adc(int gpio) {
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

#elif CONFIG_IDF_TARGET_ESP32C2 || CONFIG_IDF_TARGET_ESP32C3
	if (gpio == 0) return ADC1_GPIO0_CHANNEL;
	if (gpio == 1) return ADC1_GPIO1_CHANNEL;
	if (gpio == 2) return ADC1_GPIO2_CHANNEL;
	if (gpio == 3) return ADC1_GPIO3_CHANNEL;
	if (gpio == 4) return ADC1_GPIO4_CHANNEL;

#elif CONFIG_IDF_TARGET_ESP32C6
	if (gpio == 0) return ADC1_GPIO0_CHANNEL;
	if (gpio == 1) return ADC1_GPIO1_CHANNEL;
	if (gpio == 2) return ADC1_GPIO2_CHANNEL;
	if (gpio == 3) return ADC1_GPIO3_CHANNEL;
	if (gpio == 4) return ADC1_GPIO4_CHANNEL;
	if (gpio == 5) return ADC1_GPIO5_CHANNEL;
	if (gpio == 6) return ADC1_GPIO6_CHANNEL;

#endif
	return -1;
}

static bool adc_calibration_init(adc_unit_t unit, adc_atten_t atten, adc_cali_handle_t *out_handle)
{
	adc_cali_handle_t handle = NULL;
	esp_err_t ret = ESP_FAIL;
	bool calibrated = false;

#if ADC_CALI_SCHEME_CURVE_FITTING_SUPPORTED
	if (!calibrated) {
		ESP_LOGI(TAG, "calibration scheme version is %s", "Curve Fitting");
		adc_cali_curve_fitting_config_t cali_config = {
			.unit_id = unit,
			.atten = atten,
			.bitwidth = ADC_BITWIDTH_DEFAULT,
		};
		ret = adc_cali_create_scheme_curve_fitting(&cali_config, &handle);
		if (ret == ESP_OK) {
			calibrated = true;
		}
	}
#endif

#if ADC_CALI_SCHEME_LINE_FITTING_SUPPORTED
	if (!calibrated) {
		ESP_LOGI(TAG, "calibration scheme version is %s", "Line Fitting");
		adc_cali_line_fitting_config_t cali_config = {
			.unit_id = unit,
			.atten = atten,
			.bitwidth = ADC_BITWIDTH_DEFAULT,
		};
		ret = adc_cali_create_scheme_line_fitting(&cali_config, &handle);
		if (ret == ESP_OK) {
			calibrated = true;
		}
	}
#endif

	*out_handle = handle;
	if (ret == ESP_OK) {
		ESP_LOGI(TAG, "Calibration Success");
	} else if (ret == ESP_ERR_NOT_SUPPORTED || !calibrated) {
		ESP_LOGW(TAG, "eFuse not burnt, skip software calibration");
	} else {
		ESP_LOGE(TAG, "Invalid arg or no memory");
	}

	return calibrated;
}

#define NO_OF_SAMPLES 64 //Multisampling


void client_task(void* pvParameters) {
	// Create Timer (Trigger a measurement every second)
	ESP_LOGI(TAG, "CONFIG_ADC_CYCLE=%d", CONFIG_ADC_CYCLE);
	TimerHandle_t timerHandle = xTimerCreate("MY Trigger", CONFIG_ADC_CYCLE, pdTRUE, NULL, timer_cb);
	if (timerHandle != NULL) {
		if (xTimerStart(timerHandle, 0) != pdPASS) {
			ESP_LOGE(TAG, "Unable to start Timer");
			vTaskDelete(NULL);
		} else {
			ESP_LOGI(TAG, "Success to start Timer");
		}
	} else {
		ESP_LOGE(TAG, "Unable to create Timer");
		vTaskDelete(NULL);
	}

	// ADC1 Calibration Init
	adc_cali_handle_t adc1_cali_handle = NULL;
	bool do_calibration = adc_calibration_init(ADC_UNIT_1, ADC_ATTEN_DB_12, &adc1_cali_handle);
	if (do_calibration == false) {
		ESP_LOGE(TAG, "calibration fail");
		vTaskDelete(NULL);
	}

	// ADC1 Init
	adc_oneshot_unit_handle_t adc1_handle;
	adc_oneshot_unit_init_cfg_t init_config1 = {
		.unit_id = ADC_UNIT_1,
	};
	ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_config1, &adc1_handle));

	// ADC1 config
	adc_oneshot_chan_cfg_t config = {
		.bitwidth = ADC_BITWIDTH_DEFAULT,
		.atten = ADC_ATTEN_DB_12,
	};
	adc_channel_t adc1_channel1 = gpio2adc(CONFIG_METER1_GPIO);
	ESP_LOGI(TAG, "CONFIG_METER1_GPIO=%d adc1_channel1=%d", CONFIG_METER1_GPIO, adc1_channel1);
	ESP_ERROR_CHECK(adc_oneshot_config_channel(adc1_handle, adc1_channel1, &config));

	char meter1[8];
	sprintf(meter1, "GPIO%02d", CONFIG_METER1_GPIO);
	char meter2[8];
	memset(meter2, 0, sizeof(meter2));
	char meter3[8];
	memset(meter3, 0, sizeof(meter3));

#if CONFIG_ENABLE_METER2
	adc_channel_t adc1_channel2 = gpio2adc(CONFIG_METER2_GPIO);
	ESP_LOGI(TAG, "CONFIG_METER2_GPIO=%d adc1_channel2=%d", CONFIG_METER2_GPIO, adc1_channel2);
	ESP_ERROR_CHECK(adc_oneshot_config_channel(adc1_handle, adc1_channel2, &config));
	sprintf(meter2, "GPIO%02d", CONFIG_METER2_GPIO);
#endif

#if CONFIG_ENABLE_METER3
	adc_channel_t adc1_channel3 = gpio2adc(CONFIG_METER3_GPIO);
	ESP_LOGI(TAG, "CONFIG_METER3_GPIO=%d adc1_channel3=%d", CONFIG_METER3_GPIO, adc1_channel3);
	ESP_ERROR_CHECK(adc_oneshot_config_channel(adc1_handle, adc1_channel3, &config));
	sprintf(meter3, "GPIO%02d", CONFIG_METER3_GPIO);
#endif

	char cRxBuffer[512];
	char DEL = 0x04;
	char outBuffer[64];

	while (1) {
		size_t readBytes = xMessageBufferReceive(xMessageBufferToClient, cRxBuffer, sizeof(cRxBuffer), portMAX_DELAY );
		ESP_LOGD(TAG, "readBytes=%d", readBytes);
		ESP_LOGD(TAG, "cRxBuffer=[%.*s]", readBytes, cRxBuffer);
		cJSON *root = cJSON_Parse(cRxBuffer);
		if (cJSON_GetObjectItem(root, "id")) {
			char *id = cJSON_GetObjectItem(root,"id")->valuestring;
			ESP_LOGD(TAG, "id=[%s]",id);

			if ( strcmp (id, "init") == 0) {
				//sprintf(outBuffer,"HEAD%cADC Channel is %d", DEL, adc1_channel1);
				sprintf(outBuffer,"HEAD%cChart Display using ESP32", DEL);
				ESP_LOGD(TAG, "outBuffer=[%s]", outBuffer);
				ws_server_send_text_all(outBuffer,strlen(outBuffer));

				sprintf(outBuffer,"METER%c%s%c%s%c%s", DEL,meter1,DEL,meter2,DEL,meter3);
				ESP_LOGD(TAG, "outBuffer=[%s]", outBuffer);
				ws_server_send_text_all(outBuffer,strlen(outBuffer));
			} // end if

			if ( strcmp (id, "timer-request") == 0) {
				int voltage1 = 0;
				int32_t adc_reading1 = 0;
				int adc_raw;
				for (int i = 0; i < NO_OF_SAMPLES; i++) {
					ESP_ERROR_CHECK(adc_oneshot_read(adc1_handle, adc1_channel1, &adc_raw));
					adc_reading1 += adc_raw;
				}
				adc_reading1 /= NO_OF_SAMPLES;
				ESP_ERROR_CHECK(adc_cali_raw_to_voltage(adc1_cali_handle, adc_reading1, &voltage1));
#if CONFIG_ENABLE_STDOUT
				ESP_LOGI(TAG, "GPIO%02d adc1_channel1: %d Raw: %"PRIi32" Voltage: %dmV", CONFIG_METER1_GPIO, adc1_channel1, adc_reading1, voltage1);
#endif

				int voltage2 = 0;
#if CONFIG_ENABLE_METER2
				int32_t adc_reading2 = 0;
				for (int i = 0; i < NO_OF_SAMPLES; i++) {
					ESP_ERROR_CHECK(adc_oneshot_read(adc1_handle, adc1_channel2, &adc_raw));
					adc_reading2 += adc_raw;
				}
				adc_reading2 /= NO_OF_SAMPLES;
				ESP_ERROR_CHECK(adc_cali_raw_to_voltage(adc1_cali_handle, adc_reading2, &voltage2));
#if CONFIG_ENABLE_STDOUT
				ESP_LOGI(TAG, "GPIO%02d adc1_channel2: %d Raw: %"PRIi32" Voltage: %dmV", CONFIG_METER2_GPIO, adc1_channel2, adc_reading2, voltage2);
#endif
#endif // CONFIG_ENABLE_METER2

				int voltage3 = 0;
#if CONFIG_ENABLE_METER3
				int32_t adc_reading3 = 0;
				for (int i = 0; i < NO_OF_SAMPLES; i++) {
					ESP_ERROR_CHECK(adc_oneshot_read(adc1_handle, adc1_channel3, &adc_raw));
					adc_reading3 += adc_raw;
				}
				adc_reading3 /= NO_OF_SAMPLES;
				ESP_ERROR_CHECK(adc_cali_raw_to_voltage(adc1_cali_handle, adc_reading3, &voltage3));
#if CONFIG_ENABLE_STDOUT
				ESP_LOGI(TAG, "GPIO%02d adc1_channel3: %d Raw: %"PRIi32" Voltage: %dmV", CONFIG_METER3_GPIO, adc1_channel3, adc_reading3, voltage3);
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

	// Never reach here
	vTaskDelete(NULL);
}
