#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "sdkconfig.h"
#include "esp_log.h"
#include "esp_err.h"
#include "esp_system.h"

#include "esp_wifi.h"
#include "nvs_flash.h"

#include <netdb.h>

static const char *TAG = "wifi";

wifi_config_t wifi_sta_config;
wifi_config_t wifi_ap_config;

static void wifi_sta_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
    if (event_base == WIFI_EVENT) {
        switch (event_id) {
            case WIFI_EVENT_STA_START:
                //ESP_LOGI(TAG, "WIFI_EVENT_STA_START");
                esp_wifi_connect();
                break;
            case WIFI_EVENT_STA_DISCONNECTED:
                //ESP_LOGI(TAG, "WIFI_EVENT_STA_DISCONNECTED");
                esp_wifi_connect();
                break;
        }
    }
    else if (event_base == IP_EVENT) {
        switch (event_id) {
            case IP_EVENT_STA_GOT_IP: {
                ip_event_got_ip_t* event = event_data;
                ESP_LOGI(TAG, "Station connected with IP: "IPSTR", GW: "IPSTR", Mask: "IPSTR".",
                    IP2STR(&event->ip_info.ip),
                    IP2STR(&event->ip_info.gw),
                    IP2STR(&event->ip_info.netmask));
                break;
            }
        }
    }
}

static void wifi_ap_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
    switch (event_id) {
        case IP_EVENT_AP_STAIPASSIGNED: {
            ip_event_ap_staipassigned_t* event = event_data;
            ESP_LOGI(TAG, "SoftAP client connected with IP: "IPSTR".",
                IP2STR(&event->ip));
            break;
        }
    }
}

void WIFI_INIT() {
    // NVS: Required by WiFi Driver
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
      ESP_ERROR_CHECK(nvs_flash_erase());
      ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_ERROR_CHECK(esp_netif_init()); 
    ESP_ERROR_CHECK(esp_event_loop_create_default());
}

void AP_INIT_IP(char *ssid, char *password, char *ip, char *gw, char *nmask) {
    esp_netif_t *esp_netif_ap = esp_netif_create_default_wifi_ap();
    
    esp_netif_ip_info_t IP_settings_ap;
    IP_settings_ap.ip.addr=ipaddr_addr(ip);
    IP_settings_ap.netmask.addr=ipaddr_addr(nmask);
    IP_settings_ap.gw.addr=ipaddr_addr(gw);
    esp_netif_dhcps_stop(esp_netif_ap);
    esp_netif_set_ip_info(esp_netif_ap, &IP_settings_ap);
    esp_netif_dhcps_start(esp_netif_ap);
 
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_AP_STAIPASSIGNED, &wifi_ap_event_handler, NULL, NULL));

    strcpy((char *)wifi_ap_config.ap.ssid, ssid); 
    strcpy((char *)wifi_ap_config.ap.password, password);
    wifi_ap_config.ap.authmode = WIFI_AUTH_WPA2_PSK;
    wifi_ap_config.ap.max_connection = 4;
}

void AP_INIT(char *ssid, char *password) {
    esp_netif_create_default_wifi_ap();
    
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_AP_STAIPASSIGNED, &wifi_ap_event_handler, NULL, NULL));

    strcpy((char *)wifi_ap_config.ap.ssid, ssid); 
    strcpy((char *)wifi_ap_config.ap.password, password);
    wifi_ap_config.ap.authmode = WIFI_AUTH_WPA2_PSK;
    wifi_ap_config.ap.max_connection = 4;
}

void STA_INIT_IP(char *ssid, char *password, char *ip, char *gw, char *nmask) {
    esp_netif_t *esp_netif_sta = esp_netif_create_default_wifi_sta();

    esp_err_t ret = esp_netif_dhcpc_stop(esp_netif_sta);
    if(ret == ESP_OK) ESP_LOGI(TAG, "esp_netif_dhcpc_stop OK");
    else ESP_LOGI(TAG, "esp_netif_dhcpc_stop ERROR");

    esp_netif_ip_info_t IP_settings_sta;
    IP_settings_sta.ip.addr=ipaddr_addr(ip);
    IP_settings_sta.netmask.addr=ipaddr_addr(nmask);
    IP_settings_sta.gw.addr=ipaddr_addr(gw);
    esp_netif_set_ip_info(esp_netif_sta, &IP_settings_sta);
  
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_sta_event_handler, NULL, NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_sta_event_handler, NULL, NULL));
    
    strcpy((char *)wifi_sta_config.sta.ssid, ssid); 
    strcpy((char *)wifi_sta_config.sta.password, password);
}

void STA_INIT(char *ssid, char *password) {
    esp_netif_create_default_wifi_sta();

    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_sta_event_handler, NULL, NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_sta_event_handler, NULL, NULL));
        
    strcpy((char *)wifi_sta_config.sta.ssid, ssid);
    strcpy((char *)wifi_sta_config.sta.password, password);
}

void WIFI_START(wifi_mode_t mode) {
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
   
    ESP_ERROR_CHECK(esp_wifi_set_mode(mode));
  
    if (mode==WIFI_MODE_APSTA || mode==WIFI_MODE_STA) ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_sta_config));
    if (mode==WIFI_MODE_APSTA || mode==WIFI_MODE_AP) ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_ap_config));
    
    ESP_ERROR_CHECK(esp_wifi_start());
}

void wifi_init(void) {
    ///*
    // WIFI_INIT();
    // STA_INIT_IP("wifi_ssid","password","192.168.1.19","192.168.1.1","255.255.255.0");
    // AP_INIT_IP("esp32_AP","password","192.168.254.1","192.168.254.1","255.255.255.0");
    // WIFI_START(WIFI_MODE_APSTA); // WIFI_MODE_APSTA | WIFI_MODE_STA | WIFI_MODE_AP
    //*/

    /*
    WIFI_INIT();
    STA_INIT_IP("wifi_ssid","password","192.168.1.19","192.168.1.1","255.255.255.0");
    AP_INIT("esp32_AP","password");
    WIFI_START(WIFI_MODE_APSTA); // WIFI_MODE_APSTA | WIFI_MODE_STA | WIFI_MODE_AP
    */

    
    // WIFI_INIT();
    // STA_INIT("wifi_ssid","attila");
    // AP_INIT("esp32_AP","attila939");
    // WIFI_START(WIFI_MODE_APSTA); 
    


    WIFI_INIT();
    STA_INIT_IP("System-Teton", "tet09System", "192.168.1.52", "192.168.1.1", "255.255.255.0");
    WIFI_START(WIFI_MODE_STA);
    

    /*
    WIFI_INIT();
    AP_INIT("wifi_ssid","password");
    WIFI_START(WIFI_MODE_AP); // WIFI_MODE_APSTA | WIFI_MODE_STA | WIFI_MODE_AP
    */
}
















// #include <stdio.h>
// #include <string.h>

// #include "freertos/FreeRTOS.h"
// #include "freertos/task.h"
// #include "freertos/timers.h"
// #include "freertos/event_groups.h"
// #include "freertos/queue.h"

// #include "esp_wifi.h"
// #include "esp_log.h"
// #include "esp_check.h"
// #include "esp_mac.h"
// #include "esp_eth.h"
// #include "esp_netif.h"
// #include "esp_http_server.h"
// #include "esp_http_client.h"
// #include "esp_event.h"
// #include "esp_system.h"

// #include "lwip/inet.h"
// #include "lwip/netdb.h"
// #include "lwip/sockets.h"
// #include "lwip/ip_addr.h"

// #include "nvs_flash.h"
// #include "ping/ping_sock.h"
// #include "driver/gpio.h"


// static void wifi_event_handler(void *event_handler_arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
// {
//     switch (event_id)
//     {
//     case WIFI_EVENT_STA_START:
//         printf("WiFi connecting WIFI_EVENT_STA_START ... \n");
//         break;
//     case WIFI_EVENT_STA_CONNECTED:
//         printf("WiFi connected WIFI_EVENT_STA_CONNECTED ... \n");
//         break;
//     case WIFI_EVENT_STA_DISCONNECTED:
//         printf("WiFi lost connection WIFI_EVENT_STA_DISCONNECTED ... \n");
//         break;
//     case IP_EVENT_STA_GOT_IP:
//         printf("WiFi got IP ... \n\n");
//         break;
//     default:
//         break;
//     }
// }

// void wifi_connection()
// {
//     nvs_flash_init();
//     esp_netif_init();
//     esp_event_loop_create_default();
//     esp_netif_create_default_wifi_sta();
//     wifi_init_config_t wifi_initiation = WIFI_INIT_CONFIG_DEFAULT();
//     esp_wifi_init(&wifi_initiation);
//     esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, wifi_event_handler, NULL);
//     esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, wifi_event_handler, NULL);
//     wifi_config_t wifi_configuration = {
//         .sta = {
//             .ssid = "System-Teton",
//             .password = "tet09System"}};
//     esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_configuration);
//     esp_wifi_set_mode(WIFI_MODE_STA);
//     esp_wifi_start();
//     esp_wifi_connect();
// }

