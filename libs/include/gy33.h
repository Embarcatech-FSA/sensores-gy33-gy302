#ifndef GY33_H
#define GY33_H

#include <stdint.h>
// Declaração das funções do módulo GY-33

void gy33_init();
void gy33_read_color(uint16_t *r, uint16_t *g, uint16_t *b, uint16_t *c);
void gy33_write_register(uint8_t reg, uint8_t value);
uint16_t gy33_read_register(uint8_t reg);
#endif // GY33_H