#ifndef CONFIG_H
#define CONFIG_H

// --- Configurações dos Pinos e Periféricos ---
#define I2C_PORT_SENSORS i2c0
#define I2C_SDA_SENSORS 0
#define I2C_SCL_SENSORS 1
#define I2C_PORT_DISPLAY i2c1
#define I2C_SDA_DISPLAY 14
#define I2C_SCL_DISPLAY 15
#define ADDRESS_DISPLAY 0x3C
#define WS2812_PIN 7
#define BUZZER_PIN 10

// --- Configurações do Sensor de Cor ---
#define GY33_I2C_ADDR   0x29
#define GY33_COMMAND_BIT 0x80
#define ENABLE_REG      0x00
#define ATIME_REG       0x01
#define CONTROL_REG     0x0F
#define CDATA_REG       0x14
#define RDATA_REG       0x16
#define GDATA_REG       0x18
#define BDATA_REG       0x1A

// --- Limiares e Configurações de Lógica ---
#define LUMINOSITY_THRESHOLD 10 // Limite de luminosidade para alerta [ATENÇÃO: Insira o valor mínimo que o sensor consegue ler no seu ambiente]
#define LUMINOSITY_MAX 300 // Limite máximo de luminosidade para ajuste de brilho [ATENÇÃO: Insira o valor máximo que o sensor consegue ler no seu ambiente com luz intensa]
#define SENSOR_COLOR_MAX_VALUE 4095 // Valor para normalização dos dados brutos

// --- Pinos do LED RGB e Botões ---
#define LED_RED 13
#define LED_BLUE 12
#define LED_GREEN 11
#define BUTTON_A 5
#define BUTTON_B 6
#define BTN_JOYSTICK 22

#endif