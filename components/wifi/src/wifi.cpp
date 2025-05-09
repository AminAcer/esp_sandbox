#include "wifi/wifi.h"

#include <esp_log.h>
#include <esp_wifi.h>
#include <freertos/FreeRTOS.h>
#include <nvs_flash.h>

#include "constants/general.h"
#include "logger/logger.h"

// These MUST be bit masks in order for xEventGroupSetBits to work
#define WIFI_SUCCESS 1 << 0
#define WIFI_FAILURE 1 << 1

// Wifi Settings
#define WIFI_SSID CONFIG_WIFI_SSID
#define WIFI_PASS CONFIG_WIFI_PASSWORD

#define MAX_FAILURES CONFIG_WIFI_CONNECT_ATTEMPTS

namespace wifi {
    const static char* TAG = "WIFI";
    static int retry_num = 0;
    static EventGroupHandle_t wifi_event_group;

    static void wifi_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id,
                                   void* event_data) {
        if (event_id == WIFI_EVENT_STA_START) {
            D_LOGI(TAG, "Connecting to AP...");
            esp_wifi_connect();
        } else if (event_id == WIFI_EVENT_STA_DISCONNECTED) {
            if (retry_num < MAX_FAILURES) {
                retry_num++;
                D_LOGI(TAG, "Reconnecting to AP...\n|-- attempt: %d/%d", retry_num, MAX_FAILURES);
                esp_wifi_connect();
            } else {
                xEventGroupSetBits(wifi_event_group, WIFI_FAILURE);
            }
        }
    }

    static void ip_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id,
                                 void* event_data) {
        if (event_id == IP_EVENT_STA_GOT_IP) {
            ip_event_got_ip_t* event = (ip_event_got_ip_t*)event_data;
            D_LOGI(TAG, "Station IP: " IPSTR, IP2STR(&event->ip_info.ip));
            retry_num = 0;
            xEventGroupSetBits(wifi_event_group, WIFI_SUCCESS);
        }
    }

    esp_err_t connect_wifi() {
        int status = FAILURE;

        ESP_ERROR_CHECK(esp_netif_init());
        ESP_ERROR_CHECK(esp_event_loop_create_default());
        esp_netif_create_default_wifi_sta();

        wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
        ESP_ERROR_CHECK(esp_wifi_init(&cfg));

        wifi_event_group = xEventGroupCreate();

        esp_event_handler_instance_t wifi_handler_event_instance;
        ESP_ERROR_CHECK(esp_event_handler_instance_register(
            WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, &wifi_handler_event_instance));

        esp_event_handler_instance_t ip_event_instance;
        ESP_ERROR_CHECK(esp_event_handler_instance_register(
            IP_EVENT, IP_EVENT_STA_GOT_IP, &ip_event_handler, NULL, &ip_event_instance));

        wifi_config_t wifi_cfg = {
            .sta{
                .ssid = WIFI_SSID,
                .password = WIFI_PASS,
                .threshold{.rssi = 0, .authmode = WIFI_AUTH_WPA2_PSK, .rssi_5g_adjustment = 0},
                .pmf_cfg = {.capable = true, .required = false},
            },
        };

        ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
        ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_cfg));
        ESP_ERROR_CHECK(esp_wifi_start());

        EventBits_t bits = xEventGroupWaitBits(wifi_event_group, WIFI_SUCCESS | WIFI_FAILURE,
                                               pdFALSE, pdFALSE, portMAX_DELAY);

        if (bits == WIFI_SUCCESS) {
            D_LOGI(TAG, "Connected to access point!");
            status = SUCCESS;
        } else {
            D_LOGE(TAG, "Failed to connect to access point!");
        }

        ESP_ERROR_CHECK(esp_event_handler_instance_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP,
                                                              ip_event_instance));
        ESP_ERROR_CHECK(esp_event_handler_instance_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID,
                                                              wifi_handler_event_instance));
        vEventGroupDelete(wifi_event_group);

        return status;
    }

    int init_wifi() {
        // Init Non-Volitile storage in flash
        esp_err_t ret = nvs_flash_init();
        display::display_text("Running NVS flash init...");
        if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
            ESP_ERROR_CHECK(nvs_flash_erase());
            ret = nvs_flash_init();
        }
        ESP_ERROR_CHECK(ret);

        // Establish a wifi connection
        display::display_text("Connecting to WIFI...");
        if (connect_wifi() != SUCCESS) {
            D_LOGE(TAG, "Failed to connect to wifi!");
            return FAILURE;
        }
        display::display_text("WIFI Connected!");

        return SUCCESS;
    }
}  // namespace wifi
