#include "gy33.h"
#include "config.h"
#include "hardware/i2c.h"

/**
 * @brief Inicializa o sensor de cor GY-33 com configurações padrão.
 * Ativa o sensor, define o tempo de integração e o ganho para 16x.
 */
void gy33_init() {
    gy33_write_register(ENABLE_REG, 0x01);
    sleep_ms(3);
    gy33_write_register(ENABLE_REG, 0x03);
    gy33_write_register(ATIME_REG, 0xD5);
    gy33_write_register(CONTROL_REG, 0x02); // Ganho 16x
}

/**
 * @brief Lê os valores brutos dos canais Clear, Red, Green e Blue do sensor.
 * @param r Ponteiro para armazenar o valor do canal Vermelho.
 * @param g Ponteiro para armazenar o valor do canal Verde.
 * @param b Ponteiro para armazenar o valor do canal Azul.
 * @param c Ponteiro para armazenar o valor do canal Clear (intensidade).
 */
void gy33_read_color(uint16_t *r, uint16_t *g, uint16_t *b, uint16_t *c) {
    *c = gy33_read_register(CDATA_REG);
    *r = gy33_read_register(RDATA_REG);
    *g = gy33_read_register(GDATA_REG);
    *b = gy33_read_register(BDATA_REG);
}

/**
 * @brief Envia um comando de escrita de um byte para um registrador do sensor GY-33.
 * @param reg O endereço do registrador.
 * @param value O valor a ser escrito.
 */
void gy33_write_register(uint8_t reg, uint8_t value) {
    uint8_t buffer[2] = {reg | GY33_COMMAND_BIT, value};
    i2c_write_blocking(I2C_PORT_SENSORS, GY33_I2C_ADDR, buffer, 2, false);
}

/**
 * @brief Lê um valor de 16 bits (dois bytes) de um registrador do sensor GY-33.
 * @param reg O endereço do registrador a ser lido.
 * @return O valor de 16 bits lido.
 */
uint16_t gy33_read_register(uint8_t reg) {
    uint8_t buffer[2];
    uint8_t val = reg | GY33_COMMAND_BIT;
    i2c_write_blocking(I2C_PORT_SENSORS, GY33_I2C_ADDR, &val, 1, true);
    i2c_read_blocking(I2C_PORT_SENSORS, GY33_I2C_ADDR, buffer, 2, false);
    return (buffer[1] << 8) | buffer[0];
}