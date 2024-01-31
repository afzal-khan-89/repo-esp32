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
#define PORT 					6969  //63912 //

/* FreeRTOS event group to signal when we are connected & ready to make a request */
static EventGroupHandle_t wifi_event_group;

const int IPV4_GOTIP_BIT = BIT0;

static const char *TAG_STA = "wifi station";
static const char *TAG_AP = "wifi softAP";
static const char *TAG = "TCP/IP socket";

int client_socket;
int ip_protocol;
int socket_id;
int bind_err;
int listen_error;

esp_err_t send_video_data(int video_client){
    uint32_t timeStart=0 ,timeEnd=0;
    int wait_for_req = 1;

    uint8_t rx_buffer[12];

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
        // if(wait_for_req == 1)
        // {   
        //     memset(rx_buffer, 0, sizeof(rx_buffer));
        //     int len = recv(video_client, rx_buffer, 7, 0);
        //     if (len < 0) {           
        //         ESP_LOGE(TAG, "Error occurred during receiving: errno %d", errno);
        //     } else if (len == 0) {
        //         ESP_LOGW(TAG, "Connection closed");
        //         break;
        //     }
        //     else{
        //         ESP_LOGI(TAG, "Received  %s ...", rx_buffer);
        //         if(!strstr((char *)rx_buffer, "GETDATA"))
        //         {
        //             ESP_LOGW(TAG, "Request fail \n");
        //             continue ;
        //         }
        //         //ESP_LOGW(TAG, "Received get data . start streaming");
        //         wait_for_req = 0;
        //         timeStart = esp_timer_get_time();
        //         timeEnd = 0;
        //     }
        // }
        
        // timeEnd = esp_timer_get_time();
        // if((timeEnd - timeStart) > 5000000U)
        // {
        //     ESP_LOGI(TAG, "5s elapsed \n ");
        //     wait_for_req = 1 ;
        //     timeStart=0 ;
        //     timeEnd=0;
        // }

       // vTaskDelay(10/portTICK_PERIOD_MS);


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
        //    ESP_LOGE(TAG, "jpeg frame length %d bytes ...", _jpg_buf_len);
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




static void frame_test(void *pvParameters)
{
    camera_fb_t * fb = NULL;
    esp_err_t res = ESP_OK;
    size_t _jpg_buf_len;
    uint8_t * _jpg_buf;

    uint32_t timeStart=0 ,timeEnd=0;
    int wait_for_req = 1;
    int frame_count = 0;
    while (1)
    {

        if(wait_for_req == 1)
        {   
            wait_for_req = 0;
            timeStart = esp_timer_get_time();
        }       
        timeEnd = esp_timer_get_time();
        if((timeEnd - timeStart) > 5000000U)
        {
            ESP_LOGI(TAG, "5s elapsed .got %d frames  ", frame_count);
            wait_for_req = 1 ;
            frame_count = 0 ;
        }
    



        fb = esp_camera_fb_get();
        if (!fb) {
            ESP_LOGE(TAG, "Camera capture failed");
            res = ESP_FAIL;
            continue;
        }
        if(fb->format != PIXFORMAT_JPEG){
            bool jpeg_converted = frame2jpg(fb, 80, &_jpg_buf, &_jpg_buf_len);
            if(!jpeg_converted){
                ESP_LOGE(TAG, "JPEG compression failed");
                esp_camera_fb_return(fb);
                res = ESP_FAIL;
            }
 //           ESP_LOGI(TAG, "frame count ");
        } else {
            _jpg_buf_len = fb->len;
            _jpg_buf = fb->buf;
            ESP_LOGE(TAG, "JPEG compression ok");
        }
        if(fb->format != PIXFORMAT_JPEG){
            free(_jpg_buf);
        }
        esp_camera_fb_return(fb);
        if(res != ESP_OK){
            break;
        }
        frame_count++ ;
        vTaskDelay(10/portTICK_PERIOD_MS);
    }
    vTaskDelete(NULL);
}


void TcpServer_Init()
{
   // ESP_ERROR_CHECK( nvs_flash_init() );
   // wifi_init_ap();
   // xTaskCreate(tcp_server_task, "tcp_server", 4096, NULL, 5, NULL);
    xTaskCreate(frame_test, "tcp_server", 16384, NULL, 5, NULL);
   // xTaskCreatePinnedToCore(tcp_server_task, "tcp_server", 8192, NULL, 5, NULL, 1);
}