#include <stdio.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

#include "driver/twai.h"
#include "driver/gpio.h"
#include "esp_err.h"
#include "esp_log.h"

#include "ac_signals.h"

static const char *TAG = "AC_ECU";

// ─── Pinos TWAI ↔ TJA1050 ─────────────────────────────────────────────────
// TJA1050 TXD ← GPIO21 (TX do ESP32)
// TJA1050 RXD → GPIO22 (RX do ESP32)
#define CAN_TX_GPIO     GPIO_NUM_21
#define CAN_RX_GPIO     GPIO_NUM_22

// ─── Pinos das chaves físicas (conforme diagrama) ────────────────────────
#define PIN_AC_SW       GPIO_NUM_34   // Chave AC
#define PIN_REC_SW      GPIO_NUM_35   // Chave REC

// =============================================================================
// TASK DE TRANSMISSÃO
// =============================================================================
// Lê as chaves físicas, monta o frame StateInfo (ID 0x100) e transmite a 10 Hz.
// Adapte aqui para ler Sensor de Temperatura, seletor de velocidade/direção etc.
void can_tx_task(void *arg)
{
    ac_state_info_t state = {
        .direcao     = DIRECAO_FRENTE_ROSTO,
        .velocidade  = VELOCIDADE_2,
        .ac          = AC_OFF,
        .display     = DISPLAY_ON,
        .rec         = REC_OFF,
        .temperatura = 22,
    };

    while (1) {

        // Leitura das chaves físicas (ativo baixo, pull-up interno)
        state.ac  = gpio_get_level(PIN_AC_SW)  == 0 ? AC_ON  : AC_OFF;
        state.rec = gpio_get_level(PIN_REC_SW) == 0 ? REC_ON : REC_OFF;

        // TODO: ler temperatura do sensor e preencher state.temperatura
        // TODO: ler seletor de velocidade e direção (encoder/botões)

        twai_message_t tx_msg = {
            .identifier       = AC_MSG_ID,
            .extd             = 0,   // Standard frame (11 bits)
            .rtr              = 0,   // Data frame
            .data_length_code = AC_MSG_DLC,
        };
        ac_encode(&state, tx_msg.data);

        if (twai_transmit(&tx_msg, pdMS_TO_TICKS(100)) == ESP_OK) {
            ESP_LOGD(TAG, "TX OK -> AC=%d REC=%d T=%d C",
                     state.ac, state.rec, state.temperatura);
        } else {
            ESP_LOGW(TAG, "Erro ao enviar frame CAN");
        }

        vTaskDelay(pdMS_TO_TICKS(100));   // 10 Hz
    }
}

// =============================================================================
// TASK DE RECEPÇÃO
// =============================================================================
void can_rx_task(void *arg)
{
    twai_message_t rx_msg;
    ac_state_info_t state;

    while (1) {

        if (twai_receive(&rx_msg, portMAX_DELAY) == ESP_OK) {

            printf("RX -> ID: 0x%lx DLC: %d DATA: ",
                   rx_msg.identifier,
                   rx_msg.data_length_code);

            for (int i = 0; i < rx_msg.data_length_code; i++) {
                printf("%02X ", rx_msg.data[i]);
            }
            printf("\n");

            if (rx_msg.identifier == AC_MSG_ID && rx_msg.data_length_code == AC_MSG_DLC) {
                ac_decode(rx_msg.data, &state);

                ESP_LOGI(TAG, "---- StateInfo recebido ----------------------");
                ESP_LOGI(TAG, "  Direcao    : %s", ac_direcao_str(state.direcao));
                ESP_LOGI(TAG, "  Velocidade : %.2f (raw=%d)",
                         ac_velocidade_fisica(state.velocidade), state.velocidade);
                ESP_LOGI(TAG, "  AC         : %s", state.ac      == AC_ON      ? "LIGADO"  : "DESLIGADO");
                ESP_LOGI(TAG, "  Display    : %s", state.display == DISPLAY_ON  ? "LIGADO"  : "DESLIGADO");
                ESP_LOGI(TAG, "  REC        : %s", state.rec     == REC_ON      ? "LIGADO"  : "DESLIGADO");
                ESP_LOGI(TAG, "  Temperatura: %d C", state.temperatura);
                ESP_LOGI(TAG, "------------------------------------------------");
            }
        }
    }
}

// ─── Configuração de GPIOs de entrada (chaves) ───────────────────────────
static void gpio_switches_init(void)
{
    gpio_config_t in_cfg = {
        .pin_bit_mask = (1ULL << PIN_AC_SW) | (1ULL << PIN_REC_SW),
        .mode         = GPIO_MODE_INPUT,
        .pull_up_en   = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type    = GPIO_INTR_DISABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&in_cfg));
}

// =============================================================================
// FUNÇÃO PRINCIPAL
// =============================================================================
void app_main(void)
{
    vTaskDelay(pdMS_TO_TICKS(5000));

    gpio_switches_init();

    //
    // Configuração geral do driver TWAI
    //
    twai_general_config_t g_config =
        TWAI_GENERAL_CONFIG_DEFAULT(
            CAN_TX_GPIO,
            CAN_RX_GPIO,
            TWAI_MODE_NORMAL          // NORMAL = participa do ACK do barramento
        );

    //
    // Bitrate CAN = 500 kbps
    //
    twai_timing_config_t t_config =
        TWAI_TIMING_CONFIG_500KBITS();

    //
    // Aceita todas as mensagens
    //
    twai_filter_config_t f_config =
        TWAI_FILTER_CONFIG_ACCEPT_ALL();

    //
    // Instala driver
    //
    ESP_ERROR_CHECK(
        twai_driver_install(
            &g_config,
            &t_config,
            &f_config
        )
    );

    //
    // Inicializa barramento CAN
    //
    ESP_ERROR_CHECK(twai_start());

    ESP_LOGI(TAG, "CAN (TWAI) inicializado - 500 kbps");

    //
    // Cria task de transmissão
    //
    xTaskCreate(
        can_tx_task,
        "can_tx_task",
        4096,
        NULL,
        5,
        NULL
    );

    //
    // Cria task de recepção
    //
    xTaskCreate(
        can_rx_task,
        "can_rx_task",
        4096,
        NULL,
        5,
        NULL
    );
}
