#pragma once

#include <stdint.h>
#include <stdbool.h>

// ─── Mensagem CAN: StateInfo (ID = 0x100 = 256) ──────────────────────────────
// 8 bytes, transmitida pelo nó Air_control
//
// Layout conforme DBC (little-endian, @1 = Intel/LSB-first):
//   Byte 0      : Seletor_de_Direcao    [bit 0,  8@1+]  unsigned
//   Byte 1      : Seletor_de_Velocidade [bit 8,  8@1+]  unsigned, fator 0.25
//   Byte 2      : AC                    [bit 16, 8@1-]  signed
//   Byte 3      : Display               [bit 24, 8@1+]  unsigned
//   Byte 4      : REC                   [bit 32, 8@1-]  signed
//   Byte 5      : (não utilizado / gap entre REC e TEMP no DBC)
//   Bytes 6-7   : TEMP                  [bit 48, 16@1-] signed (°C)

#define AC_MSG_ID       0x100U   // 256 decimal
#define AC_MSG_DLC      8

// ─── Enums de valor ──────────────────────────────────────────────────────────

typedef enum {
    DIRECAO_PES             = 0,
    DIRECAO_PES_DESEMB      = 1,
    DIRECAO_DESEMB          = 2,
    DIRECAO_FRENTE_ROSTO    = 3,
} seletor_direcao_t;

typedef enum {
    VELOCIDADE_OFF = 0,
    VELOCIDADE_1   = 1,
    VELOCIDADE_2   = 2,
    VELOCIDADE_3   = 3,
    VELOCIDADE_4   = 4,
} seletor_velocidade_t;

typedef enum {
    AC_OFF = 0,
    AC_ON  = 1,
} ac_state_t;

typedef enum {
    DISPLAY_OFF = 0,
    DISPLAY_ON  = 1,
} display_state_t;

typedef enum {
    REC_OFF = 0,
    REC_ON  = 1,
} rec_state_t;

// ─── Estrutura de dados do ar-condicionado ───────────────────────────────────

typedef struct {
    seletor_direcao_t    direcao;        // Direção do fluxo
    seletor_velocidade_t velocidade;     // Velocidade do ventilador (raw: *0.25)
    ac_state_t           ac;            // Compressor ligado/desligado
    display_state_t      display;       // Display ligado/desligado
    rec_state_t          rec;           // Recirculação
    int16_t              temperatura;   // Temperatura em °C (signed)
} ac_state_info_t;

// ─── Funções de encode/decode ────────────────────────────────────────────────

/**
 * @brief  Serializa ac_state_info_t nos 8 bytes de payload CAN.
 * @param  state  Dados a serializar
 * @param  buf    Buffer de saída (8 bytes)
 */
static inline void ac_encode(const ac_state_info_t *state, uint8_t buf[8])
{
    buf[0] = (uint8_t)(state->direcao  & 0xFF);
    // Velocidade: valor físico = raw * 0.25  →  raw = valor_enum (0-4)
    buf[1] = (uint8_t)(state->velocidade & 0xFF);
    buf[2] = (uint8_t)(state->ac      & 0xFF);
    buf[3] = (uint8_t)(state->display & 0xFF);
    buf[4] = (uint8_t)(state->rec     & 0xFF);
    buf[5] = 0x00;   // gap — não utilizado pelo DBC (bit 40-47 livre)
    // TEMP: 16 bits little-endian signed, começa no byte 6 (bit 48)
    buf[6] = (uint8_t)(state->temperatura & 0xFF);
    buf[7] = (uint8_t)((state->temperatura >> 8) & 0xFF);
}

/**
 * @brief  Desserializa 8 bytes de payload CAN em ac_state_info_t.
 * @param  buf    Buffer de entrada (8 bytes)
 * @param  state  Estrutura de saída
 */
static inline void ac_decode(const uint8_t buf[8], ac_state_info_t *state)
{
    state->direcao    = (seletor_direcao_t)buf[0];
    state->velocidade = (seletor_velocidade_t)buf[1];
    state->ac         = (ac_state_t)buf[2];
    state->display    = (display_state_t)buf[3];
    state->rec        = (rec_state_t)buf[4];
    // byte 5 ignorado (gap)
    state->temperatura = (int16_t)(buf[6] | ((uint16_t)buf[7] << 8));
}

// ─── Helpers de depuração ────────────────────────────────────────────────────

static inline const char *ac_direcao_str(seletor_direcao_t d) {
    switch (d) {
        case DIRECAO_PES:          return "Pés";
        case DIRECAO_PES_DESEMB:   return "Pés + Desembaçador";
        case DIRECAO_DESEMB:       return "Desembaçador";
        case DIRECAO_FRENTE_ROSTO: return "Frente (Rosto)";
        default:                   return "?";
    }
}

static inline float ac_velocidade_fisica(seletor_velocidade_t v) {
    return (float)v * 0.25f;   // conforme fator do DBC
}
