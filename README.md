# ECU Ar-Condicionado — ESP32 (TWAI nativo) + TJA1050

Versão usando o controlador **CAN/TWAI embutido no ESP32** — sem MCP2515,
sem SPI. O TJA1050 conecta direto nos pinos GPIO de TX/RX do ESP32.

---

## Mensagem CAN

| Campo | ID | DLC |
|---|---|---|
| `StateInfo` | `0x100` (256) | 8 bytes |

### Layout dos bytes (little-endian, conforme `Ar_condicionado.dbc`)

| Byte(s) | Sinal | Tipo | Fator | Valores |
|---|---|---|---|---|
| 0 | `Seletor_de_Direcao` | uint8 | 1 | 0=Pés, 1=Pés+Desemb, 2=Desemb, 3=Frente |
| 1 | `Seletor_de_Velocidade` | uint8 | 0.25 | 0–4 |
| 2 | `AC` | int8 | 1 | 0=OFF, 1=ON |
| 3 | `Display` | uint8 | 1 | 0=OFF, 1=ON |
| 4 | `REC` | int8 | 1 | 0=OFF, 1=ON |
| 5 | — | reservado/gap | — | não usado pelo DBC |
| 6–7 | `TEMP` | int16 LE | 1 | °C |

---

## Ligações — ESP32 ↔ TJA1050

| ESP32 GPIO | TJA1050 | Descrição |
|---|---|---|
| 21 | TXD | Saída CAN (TX) |
| 22 | RXD | Entrada CAN (RX) |
| 3V3 ou 5V* | VCC | Alimentação do transceiver |
| GND | GND | Terra |

\* Confira o datasheet do seu módulo TJA1050: a maioria opera em **5V**, mas os
pinos TXD/RXD costumam tolerar nível lógico 3.3V do ESP32 sem adaptação.
Verifique o módulo específico antes de ligar.

O TJA1050 então vai para o barramento diferencial **CAN H** e **CAN L**
(com resistor de terminação de 120Ω em cada extremidade do barramento).

## Chaves de entrada (GPIO)

| GPIO | Função |
|---|---|
| 34 | Chave AC (pull-up interno, ativo em LOW) |
| 35 | Chave REC (pull-up interno, ativo em LOW) |

> GPIO 34 e 35 são **input-only** no ESP32 (sem pull-up/down físico no chip
> em alguns módulos) — o código já habilita pull-up via software
> (`GPIO_PULLUP_ENABLE`), mas em alguns ESP32 esses pinos não suportam
> pull-up interno de fato. Se a leitura ficar instável, adicione um resistor
> de pull-up externo de 10kΩ entre o pino e 3.3V.

---

## Estrutura do projeto

```
ac_ecu_twai/
├── CMakeLists.txt
├── sdkconfig.defaults
└── main/
    ├── CMakeLists.txt
    ├── ac_signals.h     ← encode/decode conforme DBC
    └── main.c            ← app_main, tasks TX/RX usando driver TWAI
```

---

## Build & Flash

```bash
. $HOME/esp/esp-idf/export.sh    # ou o caminho da sua instalação IDF v5.5

cd ac_ecu_twai
idf.py set-target esp32
idf.py build
idf.py -p COM_PORTA flash monitor
```

---

## Diferenças em relação à versão MCP2515

| | TWAI nativo (este projeto) | MCP2515 (versão anterior) |
|---|---|---|
| Hardware extra | Só o transceiver (TJA1050) | Módulo MCP2515 + transceiver |
| Interface | GPIO direto (TX/RX) | SPI (4 fios) + GPIO de interrupção |
| Driver | `driver/twai.h` (nativo ESP-IDF) | Driver SPI escrito manualmente |
| Complexidade | Menor | Maior |

Como o ESP32 já tem controlador CAN integrado, essa é a abordagem mais simples
e recomendada quando você só precisa de **um transceiver** (TJA1050) — o que é
exatamente o seu caso.

---

## Modo TWAI: NORMAL vs NO_ACK

No código de referência fornecido, o modo era `TWAI_MODE_NO_ACK` — útil para
**testar sozinho**, sem outro nó no barramento, pois o ESP32 não exige ACK de
ninguém. Troquei para `TWAI_MODE_NORMAL`, que é o modo correto quando você
tiver **mais de um nó real** no barramento CAN (ex: testando contra um
analisador CAN, outro ESP32, ou um simulador via PC).

Se for testar sozinho com um único ESP32 e um osciloscópio/analisador que não
responde ACK, volte para:

```c
twai_general_config_t g_config =
    TWAI_GENERAL_CONFIG_DEFAULT(CAN_TX_GPIO, CAN_RX_GPIO, TWAI_MODE_NO_ACK);
```

---

## Pontos de extensão em `main.c`

- **Sensor de Temperatura** → leia o sensor (I²C, OneWire, ADC) e preencha `state.temperatura`.
- **Seletores de Velocidade e Direção** → leia os botões/encoders do diagrama e atualize `state.velocidade` / `state.direcao`.
- **Display SSD1306** → use uma lib I²C/SPI para exibir os valores decodificados na `can_rx_task`.
- **Sensor de Bateria** → leia via ADC e transmita conforme necessário.
