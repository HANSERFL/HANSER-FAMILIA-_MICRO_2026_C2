#include <stdio.h>
#include <string.h>
#include <stdbool.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"

#include "driver/gpio.h"

#include "esp_log.h"
#include "esp_system.h"
#include "esp_err.h"

#include "esp_event.h"
#include "esp_netif.h"
#include "esp_wifi.h"

#include "mqtt_client.h"

#include "nvs_flash.h"

static const char *TAG = "KACATA_RC433";

static EventGroupHandle_t wifi_event_group;

#define WIFI_CONNECTED_BIT BIT0

esp_mqtt_client_handle_t mqtt_client;

#define WIFI_SSID      "HONOR X5B"
#define WIFI_PASSWORD  "123456789b"
#define MQTT_BROKER   "mqtt://test.mosquitto.org"

#define MQTT_TOPIC    "itla/led"
#define BTN_UP      GPIO_NUM_NC
#define BTN_DOWN    GPIO_NUM_NC
#define BTN_LEFT    GPIO_NUM_NC
#define BTN_RIGHT   GPIO_NUM_NC

#define BTN_A       GPIO_NUM_NC
#define BTN_B       GPIO_NUM_NC

#define LED_STATUS  GPIO_NUM_NC

typedef enum
{
    ESTADO_INICIO = 0,

    ESTADO_WIFI,

    ESTADO_MQTT,

    ESTADO_ESPERA,

    ESTADO_RF433,

    ESTADO_ERROR

}estado_t;

estado_t estado_actual = ESTADO_INICIO;

bool wifi_ok = false;

bool mqtt_ok = false;

bool rf_ok = false;

void configurar_gpio(void);

void wifi_init(void);

void mqtt_init(void);

void rf433_init(void);

void maquina_estado(void);

void configurar_gpio(void)
{

    gpio_config_t io = {

        .mode = GPIO_MODE_INPUT,

        .pull_up_en = GPIO_PULLUP_ENABLE,

        .pull_down_en = GPIO_PULLDOWN_DISABLE,

        .intr_type = GPIO_INTR_DISABLE

    };

    // Los GPIO reales se agregarán cuando termine
    // de analizar el esquemático.

}
/*******************************************************
 * ESTRUCTURA DE BOTONES
 *******************************************************/

typedef struct
{
    gpio_num_t pin;
    bool estado;
    bool anterior;

} boton_t;
boton_t boton_up;
boton_t boton_down;
boton_t boton_left;
boton_t boton_right;

boton_t boton_A;
boton_t boton_B;
void botones_init(void)
{

    boton_up.pin    = BTN_UP;
    boton_down.pin  = BTN_DOWN;
    boton_left.pin  = BTN_LEFT;
    boton_right.pin = BTN_RIGHT;

    boton_A.pin     = BTN_A;
    boton_B.pin     = BTN_B;

}
bool leer_boton(boton_t *b)
{

    b->anterior = b->estado;

    b->estado = (gpio_get_level(b->pin)==0);

    if(b->estado && !b->anterior)
    {
        vTaskDelay(pdMS_TO_TICKS(20));

        if(gpio_get_level(b->pin)==0)
        {
            return true;
        }
    }

    return false;

}
void escanear_botones(void)
{

    if(leer_boton(&boton_up))
    {
        ESP_LOGI(TAG,"UP");
    }

    if(leer_boton(&boton_down))
    {
        ESP_LOGI(TAG,"DOWN");
    }

    if(leer_boton(&boton_left))
    {
        ESP_LOGI(TAG,"LEFT");
    }

    if(leer_boton(&boton_right))
    {
        ESP_LOGI(TAG,"RIGHT");
    }

    if(leer_boton(&boton_A))
    {
        ESP_LOGI(TAG,"A");
    }

    if(leer_boton(&boton_B))
    {
        ESP_LOGI(TAG,"B");
    }

}
case ESTADO_ESPERA:

    escanear_botones();

    break;

    /*******************************************************
 * COMANDOS DEL CONTROL
 *******************************************************/

typedef enum
{
    CMD_NONE = 0,
    CMD_UP,
    CMD_DOWN,
    CMD_LEFT,
    CMD_RIGHT,
    CMD_A,
    CMD_B

} comando_t;
comando_t comando_actual = CMD_NONE;
comando_t obtener_comando(void)
{

    if(leer_boton(&boton_up))
        return CMD_UP;

    if(leer_boton(&boton_down))
        return CMD_DOWN;

    if(leer_boton(&boton_left))
        return CMD_LEFT;

    if(leer_boton(&boton_right))
        return CMD_RIGHT;

    if(leer_boton(&boton_A))void procesar_comando(comando_t cmd)
{

    switch(cmd)
    {

        case CMD_UP:

            ESP_LOGI(TAG,"BOTON UP");

        break;

        case CMD_DOWN:

            ESP_LOGI(TAG,"BOTON DOWN");

        break;

        case CMD_LEFT:

            ESP_LOGI(TAG,"BOTON LEFT");

        break;

        case CMD_RIGHT:

            ESP_LOGI(TAG,"BOTON RIGHT");

        break;

        case CMD_A:

            ESP_LOGI(TAG,"BOTON A");

        break;

        case CMD_B:

            ESP_LOGI(TAG,"BOTON B");

        break;

        default:

        break;

    }

}
        return CMD_A;

    if(leer_boton(&boton_B))
        return CMD_B;

    return CMD_NONE;

}
case ESTADO_ESPERA:

    comando_actual = obtener_comando();

    if(comando_actual != CMD_NONE)
    {

        procesar_comando(comando_actual);

    }

break;

/*******************************************************
 * RF433
 *******************************************************/

typedef enum
{
    RF_OK = 0,
    RF_ERROR

}rf_estado_t;

rf_estado_t rf_estado = RF_OK;

void rf433_init(void)
{

    ESP_LOGI(TAG,"Inicializando RF433");

    // Aquí se inicializará el hardware
    // una vez identificado en el esquemático.

}
void rf433_enviar(comando_t comando)
{

    switch(comando)
    {

        case CMD_UP:

            ESP_LOGI(TAG,"Transmitiendo UP");

        break;

        case CMD_DOWN:

            ESP_LOGI(TAG,"Transmitiendo DOWN");

        break;

        case CMD_LEFT:

            ESP_LOGI(TAG,"Transmitiendo LEFT");

        break;

        case CMD_RIGHT:

            ESP_LOGI(TAG,"Transmitiendo RIGHT");

        break;

        case CMD_A:

            ESP_LOGI(TAG,"Transmitiendo A");

        break;

        case CMD_B:

            ESP_LOGI(TAG,"Transmitiendo B");

        break;

        default:

        break;

    }

}
rf433_enviar(cmd);
case CMD_UP:

    ESP_LOGI(TAG,"BOTON UP");

    rf433_enviar(cmd);

break;
case ESTADO_RF433:

    ESP_LOGI(TAG,"RF433 listo");

    estado_actual = ESTADO_ESPERA;
    estado_actual = ESTADO_RF433;


break;
