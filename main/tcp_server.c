/* tcp_server Example
Example written for Laprota by Sasoun Torossian

Combines example code from the the "esp-idf/examples/protocols/sockets/tcp_server/main/tcp_server.c" repository and "stations_AP.c"

Must first use mingw32.exe console to navigate to ~/station_AP folder,
and execute "make menuconfig".

From menuconfig, serial port that ESP32 is connected to must be chosen.
*/


/* PROBLEMS:
 *
 * Upon connecting phone send ESP32 127 + 31 bytes of data, establishing an immediate TCP/IP link. Prevents further connections unless
 * TCP/IP client app connects, then we restart the wifi connection.
 *
 * When ESP32 receives router ssid and password, unable to initialise STA_CONNECTION. Connection is constantly dropped.
 *
 */

#include <string.h>
#include <sys/param.h>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"
//#include "esp_event_loop.h"
#include "esp_log.h"
// #include "nvs_flash.h"

#include "esp_camera.h"
#include "esp_timer.h"

#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include <lwip/netdb.h>

//static esp_err_t event_handler(void *ctx, system_event_t *event);
static void tcp_server_task(void *pvParameters);
//void wifi_init_ap();
//void wifi_init_sta(char* gateway_ssid, char* gateway_password);
//char* getTagValue(char* a_tag_list, char* a_tag);


/* The examples use simple WiFi configuration that you can set via
   'make menuconfig'.
   If you'd rather not, just change the below entries to strings with
   the config you want - ie #define EXAMPLE_WIFI_SSID "mywifissid"
*/

#define GATEWAY_SSID			"VM837614-2G" 	//Home wifi ssid
#define GATEWAY_PASS			"jwwdxwfq"		//Home wifi password
#define ESP_AP_SSID				"esp32-AP"			//Ssid for ESP32 access point
#define ESP_AP_PASS				"pass1234" 		//password for ESP32 access point
#define GATEWAY_MAX_RETRY		10 				//Number of times ESP32 will attempt to reconnect to router
#define ESP_AP_MAX_CONNECT		2				//Maximum stations that can connect to ESP32
#define PORT 					63912

/* FreeRTOS event group to signal when we are connected & ready to make a request */
static EventGroupHandle_t wifi_event_group;

const int IPV4_GOTIP_BIT = BIT0;

static const char *TAG_STA = "wifi station";
static const char *TAG_AP = "wifi softAP";
static const char *TAG = "TCP/IP socket";

//static int s_retry_num = 0;		//Initialise current retry to 0
//static uint8_t MAC_array[6];	//Initialise MAC address
//static char MAC_char[18];		//Display MAC characters

int client_socket;
int ip_protocol;
int socket_id;
int bind_err;
int listen_error;

// static esp_err_t event_handler(void *ctx, system_event_t *event)
// {
//     switch (event->event_id)
//     {
//     case SYSTEM_EVENT_STA_START: //Upon esp_wifi_start(), this event will arise
//     	ESP_LOGI(TAG_STA, "SYSTEM_EVENT_STA_START");
//     	ESP_ERROR_CHECK(esp_wifi_connect()); //Connect to detected Wifi network
//         break;

//     case SYSTEM_EVENT_STA_CONNECTED: //When esp_wifi_connect() is succesful
//     	ESP_LOGI(TAG_STA, "SYSTEM_EVENT_STA_CONNECTED");
//     	ESP_ERROR_CHECK(esp_wifi_get_mac(WIFI_MODE_STA, MAC_array));
//         for (int i = 0; i < sizeof(MAC_array); ++i)
//         {
//            	sprintf(MAC_char,"%s%02x:",MAC_char,MAC_array[i]);
//         }
//         ESP_LOGI(TAG_STA, "got MAC: %s", MAC_char);
//         xEventGroupSetBits(wifi_event_group, IPV4_GOTIP_BIT);
//         break;

//     case SYSTEM_EVENT_STA_GOT_IP: //When AP DHCP server provides an IP address to DHCP client (ESP32) this event will arise.
//     	ESP_LOGI(TAG_STA, "SYSTEM_EVENT_STA_GOT_IP");
//     	ESP_LOGI(TAG_STA, "got ip: %s",
//                  ip4addr_ntoa(&event->event_info.got_ip.ip_info.ip));
//         s_retry_num = 0;
//         xEventGroupSetBits(wifi_event_group, IPV4_GOTIP_BIT);
//         break;

//     case SYSTEM_EVENT_STA_DISCONNECTED: //If disconnection occurs, this event will arise
//         {
//         	ESP_LOGI(TAG_STA, "SYSTEM_EVENT_STA_DISCONNECTED");
//             if (s_retry_num < GATEWAY_MAX_RETRY)
//             {
//                 ESP_ERROR_CHECK(esp_wifi_connect());
//                 xEventGroupClearBits(wifi_event_group, IPV4_GOTIP_BIT);
//                 s_retry_num++;
//                 ESP_LOGI(TAG_STA,"retry to connect to the AP");
//             }
//             ESP_LOGI(TAG_STA,"connect to the AP fail\n");
//             break;
//         }

//     case SYSTEM_EVENT_AP_STACONNECTED: //When new stations connects to AP (ESP32), display MAC address and AID
//     	ESP_LOGI(TAG_STA, "SYSTEM_EVENT_AP_STACONNECTED");
//     	ESP_LOGI(TAG_AP, "station:"MACSTR" join, AID= %d",
//                  MAC2STR(event->event_info.sta_connected.mac),
//                  event->event_info.sta_connected.aid);
//         break;

//     case SYSTEM_EVENT_AP_STADISCONNECTED: //When stations disconnect from AP (ESP32), display deisconnected stations' MAC and AID
//     	ESP_LOGI(TAG_STA, "SYSTEM_EVENT_AP_STADISCONNECTED");
//     	ESP_LOGI(TAG_AP, "station:"MACSTR"leave, AID= %d",
//                  MAC2STR(event->event_info.sta_disconnected.mac),
//                  event->event_info.sta_disconnected.aid);
//     	//On disconnet, close TCP socket client
//     	if (client_socket != -1)
//     	{
//     		ESP_LOGE(TAG, "Shutting down socket and restarting...");
//     		shutdown(client_socket, 0);
//     		close(client_socket);
//     	}
//         break;

//     default:
//         break;
//     }
//     return ESP_OK;
// }

/* PROBLEMS
 * Set up AP first, then when router info is received, start APSTA mode.
 *
 * Causes issues with ESP32 connecting with router as STA
 *
 * ??Deactivate AP mode first, then activate APSTA mode??
 *
 * ??Reinitialise WiFi from scratch??
 *
 * ??Issues with ESP32 channels??
 *
 * ??Try hardcoding router ssid/password, then seeing result??
 *
 * ??Hardcoding station ID works?
 *
 * ??Why does passing variable not work??????????????????????
 * Need to brush up on char array manipulation techniques
 *
 */

// void wifi_init_ap()
// {
//     wifi_event_group = xEventGroupCreate(); 					//Create listener thread

//     tcpip_adapter_init(); 										//Initialise lwIP
//     ESP_ERROR_CHECK(esp_event_loop_init(event_handler, NULL) ); //Start event_handler loop

//     wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT(); 	//Create instance of wifi_init_config_t cfg, and assign default values to all members
//     ESP_ERROR_CHECK(esp_wifi_init(&cfg)); 					//Initialise instance of wifi_init_config_t

//     wifi_config_t wifi_config_ap = { 						//Set configuration parameters for AP mode
//         .ap = {
//             .ssid = ESP_AP_SSID, 							//SSID of ESP32 AP
//             .ssid_len = strlen(ESP_AP_SSID), 				//Length of SSID
//             .password = ESP_AP_PASS, 						//ESP32 AP password
//             .max_connection = ESP_AP_MAX_CONNECT, 			//Maximum allowed connections
//             .authmode = WIFI_AUTH_WPA_WPA2_PSK 				//Authorization type. Secure WPA2_PSK protocol
//         },
//     };

//     ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP) ); 						//Set Wifi mode as AP+Station
//     ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_AP, &wifi_config_ap)); 		//Initialise AP configuration
//     ESP_LOGI(TAG_AP, "wifi_init_ap finished SSID: %s, password: %s",
//     		ESP_AP_SSID, ESP_AP_PASS);									//Print what credentials the ESP32 is broadcasting as an AP

//     ESP_ERROR_CHECK(esp_wifi_start()); 										//Start the Wifi driver
// }

// void wifi_init_sta(char* gateway_ssid, char* gateway_password)
// {
// 	/*
// 	char* ssid_copy = malloc(strlen(gateway_ssid) + 1);
// 	char* password_copy = malloc(strlen(gateway_password) + 1);

// 	strcpy(ssid_copy, gateway_ssid);
// 	strcpy(password_copy, gateway_password);
// 	*/

// 	wifi_config_t wifi_config_sta;
// 	strcpy((char*)wifi_config_sta.sta.ssid, gateway_ssid);
// 	strcpy((char*)wifi_config_sta.sta.password, gateway_password);

// 	/*
//     wifi_config_t wifi_config_sta = { 						//Set configuration parameters for station mode
//         .sta = {
//             .ssid = ssid_copy,							//Home router SSID
//             .password = password_copy, 						//Home router password
// 			.scan_method = WIFI_ALL_CHANNEL_SCAN 			//Scan mode to detect home router
//         },
// 	};
// 	*/
//     ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA) ); 					//Set Wifi mode as AP+Station
//     ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config_sta)); 	//Initialise Station configuration
//     ESP_LOGI(TAG_STA, "wifi_init_sta finished SSID: %s, password: %s",
//     		GATEWAY_SSID, GATEWAY_PASS); 								//Print which AP the ESP32 will be connecting to
//     ESP_LOGI(TAG_STA, "wifi_init_sta finished provided SSID: %s, password: %s",
//     		gateway_ssid, gateway_password);
//     ESP_ERROR_CHECK(esp_wifi_start());
// }






esp_err_t send_video_data(int video_client){

    char rx_buffer[]="hello ... ";

    int tcp_error = -1 ;
    camera_fb_t * fb = NULL;
    esp_err_t res = ESP_OK;
    size_t _jpg_buf_len;
    uint8_t * _jpg_buf;
    char * part_buf[64];
    static int64_t last_frame = 0;
    if(!last_frame) {
        last_frame = esp_timer_get_time();
    }

    // res = httpd_resp_set_type(req, _STREAM_CONTENT_TYPE);
    // if(res != ESP_OK){
    //     return res;
    // }

    while(true){

        int len = recv(video_client, rx_buffer, sizeof(rx_buffer) - 1, 0);
        if (len < 0) {
            
            ESP_LOGE(TAG, "Error occurred during receiving: errno %d", errno);

        } else if (len == 0) {
            ESP_LOGW(TAG, "Connection closed");
            break;
        }
        else{
            ESP_LOGI(TAG, "Received  %s ...", rx_buffer);
        }
        vTaskDelay(100/portTICK_PERIOD_MS);


        fb = esp_camera_fb_get();
        if (!fb) {
            ESP_LOGE(TAG, "Camera capture failed");
            res = ESP_FAIL;
            break;
        }
        if(fb->format != PIXFORMAT_JPEG){
            bool jpeg_converted = frame2jpg(fb, 80, &_jpg_buf, &_jpg_buf_len);
            if(!jpeg_converted){
                ESP_LOGE(TAG, "JPEG compression failed");
                esp_camera_fb_return(fb);
                res = ESP_FAIL;
            }
        } else {
            _jpg_buf_len = fb->len;
            _jpg_buf = fb->buf;
            ESP_LOGE(TAG, "JPEG compression ok");
        }
        if(res == ESP_OK){
            //res = httpd_resp_send_chunk(req, (const char *)_jpg_buf, _jpg_buf_len);
            tcp_error = send(video_client, _jpg_buf, _jpg_buf_len, 0);
            if (tcp_error < 0) 
            {
                ESP_LOGE(TAG, "Error occurred during sending: errno %d", errno);
            }
            else ESP_LOGI(TAG, "send %d bytes to client ... ", _jpg_buf_len);
        }
        if(fb->format != PIXFORMAT_JPEG){
            free(_jpg_buf);
        }
        esp_camera_fb_return(fb);
        if(res != ESP_OK || tcp_error < 0){
            break;
        }
        int64_t fr_end = esp_timer_get_time();
        int64_t frame_time = fr_end - last_frame;
        last_frame = fr_end;
        frame_time /= 1000;
        // ESP_LOGI(TAG, "MJPG: %uKB %ums (%.1ffps)",
        //     (uint32_t)(_jpg_buf_len/1024),
        //     (uint32_t)frame_time, 1000.0 / (uint32_t)frame_time);
    }

    last_frame = 0;
    return res;
}



















static void tcp_server_task(void *pvParameters)
{
    char rx_buffer[32];	// char array to store received data
    char tx_buff[] = "hello from server" ;
    char addr_str[128];		// char array to store client IP
    int bytes_received;		// immediate bytes received
    int addr_family;		// Ipv4 address protocol variable

    while (1)
    {
        struct sockaddr_in destAddr;
        destAddr.sin_addr.s_addr = htonl(INADDR_ANY); //Change hostname to network byte order
        destAddr.sin_family = AF_INET;		//Define address family as Ipv4
        destAddr.sin_port = htons(PORT); 	//Define PORT
        addr_family = AF_INET;				//Define address family as Ipv4
        ip_protocol = IPPROTO_TCP;			//Define protocol as TCP
        inet_ntoa_r(destAddr.sin_addr, addr_str, sizeof(addr_str) - 1);

        /* Create TCP socket*/
        socket_id = socket(addr_family, SOCK_STREAM, ip_protocol);
        if (socket_id < 0)
        {
            ESP_LOGE(TAG, "Unable to create socket: errno %d", errno);
            break;
        }
        ESP_LOGI(TAG, "Socket created");

        /* Bind a socket to a specific IP + port */
        bind_err = bind(socket_id, (struct sockaddr *)&destAddr, sizeof(destAddr));
        if (bind_err != 0)
        {
            ESP_LOGE(TAG, "Socket unable to bind: errno %d", errno);
            break;
        }
        ESP_LOGI(TAG, "Socket binded");

        /* Begin listening for clients on socket */
        listen_error = listen(socket_id, 3);
        if (listen_error != 0)
        {
            ESP_LOGE(TAG, "Error occured during listen: errno %d", errno);
            break;
        }
        ESP_LOGI(TAG, "Socket listening ...");

        while (1)
        {
        	struct sockaddr_in sourceAddr; 
        	uint addrLen = sizeof(sourceAddr);
        	client_socket = accept(socket_id, (struct sockaddr *)&sourceAddr, &addrLen);
        	if (client_socket < 0)
        	{
        		ESP_LOGE(TAG, "Unable to accept connection: errno %d", errno);
        		break;
        	}
        	ESP_LOGI(TAG, "Socket accepted ... ");
            send_video_data(client_socket);
            // while(1)
            // {
            //     // bytes_received = recv(client_socket, rx_buffer, sizeof(rx_buffer) - 1, 0);
            //     // if (bytes_received < 0)
			// 	// {
			// 	// 	ESP_LOGI(TAG, "Waiting for data...");
			// 	// 	vTaskDelay(100 / portTICK_PERIOD_MS);
			// 	// }
            //     // else if (bytes_received == 0)
			// 	// {
			// 	// 	ESP_LOGI(TAG, "Connection closed...");
			// 	// 	break;
			// 	// }
            //     // else
            //     // {
            //     //     ESP_LOGI(TAG, "Received : %s", rx_buffer);

            //     //     memcpy(tx_buff, "hello client", 12);
            //     //     int err = send(client_socket, tx_buff, sizeof(tx_buff), 0);
            //     //     if (err < 0) 
            //     //     {
            //     //         ESP_LOGE(TAG, "Error occurred during sending: errno %d", errno);
            //     //         break;
            //     //     }
            //     //     ESP_LOGI(TAG, "successfully sent to client ... ");
            //     //     bzero(rx_buffer, sizeof(rx_buffer));
            //     //     bzero(tx_buff, sizeof(tx_buff));
            //     // }
            //     send_video_data(client_socket);
            //     break;
            // }
        }
    }
    vTaskDelete(NULL);
}

// char* getTagValue(char* a_tag_list, char* a_tag)
// {
//     /* 'strtok' modifies the string. */
//     char* tag_list_copy = malloc(strlen(a_tag_list) + 1);
//     char* result        = 0;
//     char* s;

//     strcpy(tag_list_copy, a_tag_list); //original to copy


//     s = strtok(tag_list_copy, "&"); //Use delimiter "&"
//     while (s)
//     {
//         char* equals_sign = strchr(s, '=');
//         if (equals_sign)
//         {
//             *equals_sign = 0;
//             //Use string compare to find required tag
//             if (0 == strcmp(s, a_tag))
//             {
//                 equals_sign++;
//                 result = malloc(strlen(equals_sign) + 1);
//                 strcpy(result, equals_sign);
//             }
//         }
//         s = strtok(0, "&");
//     }
//     free(tag_list_copy);
//     return result;
// }



void TcpServer_Init()
{
   // ESP_ERROR_CHECK( nvs_flash_init() );
   // wifi_init_ap();
    xTaskCreate(tcp_server_task, "tcp_server", 4096, NULL, 5, NULL);
}