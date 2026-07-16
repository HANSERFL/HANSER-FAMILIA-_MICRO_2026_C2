/*******************************************************
 * ESP32 + WiFi + MQTT + Máquina de Estados
 *******************************************************/

#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include "esp_err.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"

#include "esp_system.h"
#include "esp_event.h"
#include "esp_wifi.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "nvs_flash.h"

#include "mqtt_client.h"

#include "driver/gpio.h"

/*******************************************************
 * DEFINICIÓN DE PINES
 *******************************************************/

#define LED_GPIO        GPIO_NUM_48
#define BUTTON_GPIO     GPIO_NUM_0

/*******************************************************
 * DATOS WIFI
 *******************************************************/

#define WIFI_SSID      "HONOR X5B"
#define WIFI_PASSWORD  "123456789b"

/*******************************************************
 * DATOS MQTT
 *******************************************************/

#define MQTT_BROKER   "mqtt://test.mosquitto.org"
#define MQTT_TOPIC    "itla/led"

/*******************************************************
 * VARIABLES GLOBALES
 *******************************************************/

static const char *TAG = "ESP32_MQTT";

static EventGroupHandle_t wifi_event_group;

#define WIFI_CONNECTED_BIT BIT0

esp_mqtt_client_handle_t mqtt_client;

/*******************************************************
 * MÁQUINA DE ESTADOS
 *******************************************************/

typedef enum
{
    ESTADO_INICIO = 0,
    ESTADO_WIFI,
    ESTADO_MQTT,
    ESTADO_ESPERA,
    ESTADO_ERROR

} estado_t;

estado_t estado_actual = ESTADO_INICIO;

/*******************************************************
 * CONFIGURAR LED
 *******************************************************/

void configurar_led(void)
{
    gpio_config_t io_conf = {

        .pin_bit_mask = (1ULL << LED_GPIO),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE

    };

    gpio_config(&io_conf);

    gpio_set_level(LED_GPIO,0);
}

/*******************************************************
 * CONFIGURAR BOTÓN
 *******************************************************/

void configurar_boton(void)
{
    gpio_config_t io_conf = {

        .pin_bit_mask = (1ULL << BUTTON_GPIO),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE

    };

    gpio_config(&io_conf);
}
static void wifi_event_handler(void *arg,
                               esp_event_base_t event_base,
                               int32_t event_id,
                               void *event_data)
{

    if(event_base == WIFI_EVENT &&
       event_id == WIFI_EVENT_STA_START)
    {
        esp_wifi_connect();
    }

    else if(event_base == WIFI_EVENT &&
            event_id == WIFI_EVENT_STA_DISCONNECTED)
    {
        ESP_LOGI(TAG,"Reconectando WiFi...");

        esp_wifi_connect();

        xEventGroupClearBits(wifi_event_group,
                             WIFI_CONNECTED_BIT);
    }

    else if(event_base == IP_EVENT &&
            event_id == IP_EVENT_STA_GOT_IP)
    {
        ESP_LOGI(TAG,"WiFi conectado");

        xEventGroupSetBits(wifi_event_group,
                           WIFI_CONNECTED_BIT);
    }

}
void wifi_init(void)
{

    wifi_event_group = xEventGroupCreate();

    esp_netif_init();

    esp_event_loop_create_default();

    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();

    esp_wifi_init(&cfg);

    esp_event_handler_instance_register(
            WIFI_EVENT,
            ESP_EVENT_ANY_ID,
            &wifi_event_handler,
            NULL,
            NULL);

    esp_event_handler_instance_register(
            IP_EVENT,
            IP_EVENT_STA_GOT_IP,
            &wifi_event_handler,
            NULL,
            NULL);

    wifi_config_t wifi_config = {

        .sta = {

            .ssid = WIFI_SSID,

            .password = WIFI_PASSWORD,

            .threshold.authmode = WIFI_AUTH_WPA2_PSK

        }

    };

    esp_wifi_set_mode(WIFI_MODE_STA);

    esp_wifi_set_config(WIFI_IF_STA,
                        &wifi_config);

    esp_wifi_start();

}
/*******************************************************
 * MQTT EVENT HANDLER
 *******************************************************/

static void mqtt_event_handler(void *handler_args,
                               esp_event_base_t base,
                               int32_t event_id,
                               void *event_data)
{
    esp_mqtt_event_handle_t event = event_data;

    if(strcmp(mensaje,"ON")==0)
{
    led_estado = true;
    gpio_set_level(LED_GPIO,1);
}

else if(strcmp(mensaje,"OFF")==0)
{
    led_estado = false;
    gpio_set_level(LED_GPIO,0);
}

    switch ((esp_mqtt_event_id_t)event_id)
    {

        //-------------------------------------------------
        // Conectado al broker
        //-------------------------------------------------

        case MQTT_EVENT_CONNECTED:

            ESP_LOGI(TAG, "MQTT conectado");

            esp_mqtt_client_subscribe(
                    mqtt_client,
                    MQTT_TOPIC,
                    0);

        break;

        //-------------------------------------------------
        // Mensaje recibido
        //-------------------------------------------------

        case MQTT_EVENT_DATA:

        {
            char mensaje[20];

            memset(mensaje,0,sizeof(mensaje));

            memcpy(mensaje,
                   event->data,
                   event->data_len);

            ESP_LOGI(TAG,
                     "Mensaje recibido: %s",
                     mensaje);

            if(strcmp(mensaje,"ON")==0)
            {
                gpio_set_level(LED_GPIO,1);
            }

            else if(strcmp(mensaje,"OFF")==0)
            {
                gpio_set_level(LED_GPIO,0);
            }

        }

        break;

        //-------------------------------------------------

        case MQTT_EVENT_DISCONNECTED:

            ESP_LOGI(TAG,
                     "MQTT desconectado");

        break;

        default:

        break;
    }

}
/*******************************************************
 * INICIALIZAR MQTT
 *******************************************************/

void mqtt_init(void)
{

    esp_mqtt_client_config_t mqtt_cfg = {

        .broker.address.uri = MQTT_BROKER

    };

    mqtt_client = esp_mqtt_client_init(&mqtt_cfg);

    esp_mqtt_client_register_event(
            mqtt_client,
            ESP_EVENT_ANY_ID,
            mqtt_event_handler,
            NULL);

    esp_mqtt_client_start(mqtt_client);

}
/*******************************************************
 * PUBLICAR ESTADO
 *******************************************************/

void publicar_estado_led(bool estado)
{

    if(estado)
    {
        esp_mqtt_client_publish(
                mqtt_client,
                MQTT_TOPIC,
                "ON",
                0,
                1,
                0);
    }
    else
    {
        esp_mqtt_client_publish(
                mqtt_client,
                MQTT_TOPIC,
                "OFF",
                0,
                1,
                0);
    }

}
/*******************************************************
 * MAQUINA DE ESTADOS
 *******************************************************/

void maquina_estado(void)
{

    switch(estado_actual)
    {

        /**********************************************
         * ESTADO INICIO
         **********************************************/

        case ESTADO_INICIO:

            ESP_LOGI(TAG,"Inicializando...");

            configurar_led();

            configurar_boton();

            estado_actual = ESTADO_WIFI;

        break;

        /**********************************************
         * ESTADO WIFI
         **********************************************/

        case ESTADO_WIFI:

            ESP_LOGI(TAG,"Conectando WiFi...");

            wifi_init();

            xEventGroupWaitBits(
                    wifi_event_group,
                    WIFI_CONNECTED_BIT,
                    pdFALSE,
                    pdTRUE,
                    portMAX_DELAY);

            estado_actual = ESTADO_MQTT;

        break;

        /**********************************************
         * ESTADO MQTT
         **********************************************/

        case ESTADO_MQTT:

            ESP_LOGI(TAG,"Conectando MQTT...");

            mqtt_init();

            vTaskDelay(pdMS_TO_TICKS(3000));

            estado_actual = ESTADO_ESPERA;

        break;

        /**********************************************
         * ESTADO ESPERA
         **********************************************/

        case ESTADO_ESPERA:

            if(gpio_get_level(BUTTON_GPIO)==0)
            {

                vTaskDelay(pdMS_TO_TICKS(50));

                if(gpio_get_level(BUTTON_GPIO)==0)
                {

                    led_estado = !led_estado;

                    gpio_set_level(
                            LED_GPIO,
                            led_estado);

                    publicar_estado_led(
                            led_estado);

                    while(gpio_get_level(BUTTON_GPIO)==0);

                    vTaskDelay(pdMS_TO_TICKS(20));

                }

            }

        break;

        /**********************************************
         * ESTADO ERROR
         **********************************************/

        case ESTADO_ERROR:

            while(1)
            {

                gpio_set_level(LED_GPIO,1);

                vTaskDelay(pdMS_TO_TICKS(250));

                gpio_set_level(LED_GPIO,0);

                vTaskDelay(pdMS_TO_TICKS(250));

            }

        break;

    }

}
/*******************************************************
 * PROGRAMA PRINCIPAL
 *******************************************************/

void app_main(void)
{

    esp_err_t ret = nvs_flash_init();

    if(ret == ESP_ERR_NVS_NO_FREE_PAGES ||
       ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());

        ret = nvs_flash_init();
    }

    ESP_ERROR_CHECK(ret);

    ESP_LOGI(TAG,"Sistema iniciado");

    while(1)
    {

        maquina_estado();

        vTaskDelay(pdMS_TO_TICKS(10));

    }

}

