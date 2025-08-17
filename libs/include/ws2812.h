#ifndef WS2812_H
#define WS2812_H

#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "hardware/clocks.h" 
#include "ws2812.pio.h"



#define LEDS_COUNT 25 // Define a quantidade de LEDs da matriz


// Variáveis para o controle da máquina PIO
PIO np_pio;
uint np_sm;


// Converte os valores de vermelho (R), verde (G) e azul (B) em um único número de 32 bits no formato GRB, utilizado pelos LEDs WS2812.
static inline uint32_t rgb_u32(uint8_t r, uint8_t g, uint8_t b)
{
    return ((uint32_t)(r) << 8) | ((uint32_t)(g) << 16) | (uint32_t)(b);
}

// Escreve o buffer de LEDS na máquina PIO
static inline void put_pixel(uint32_t pixel_grb)
{
    pio_sm_put_blocking(np_pio, np_sm, pixel_grb << 8u);
}

// Define a cor dos LEDs da matriz
void np_set_leds(bool *matriz,uint8_t r, uint8_t g, uint8_t b)
{
    // Define a cor com base nos parâmetros fornecidos
    uint32_t color = rgb_u32(r, g, b);

    // Define todos os LEDs com a cor especificada
    for (int i = 0; i < LEDS_COUNT; i++)
    {
        if (matriz[24 - i])
        {
            put_pixel(color); // Liga o LED com um no buffer
        }
        else
        {
            put_pixel(0);  // Desliga os LEDs com zero no buffer
        }
    }
}

// Inicializa a máquina PIO
void np_init(uint pin){
    // Cria uma instância da máquina PIO
    uint offset = pio_add_program(pio0, &ws2812_program);
    np_pio = pio0;

    // Aloca uma máquina PIO
    np_sm = pio_claim_unused_sm(np_pio, false);
    if(np_sm < 0){
        np_pio = pio1;
        np_sm = pio_claim_unused_sm(np_pio, true); // Se não houver máquinas disponíveis, panic
    }

    // Inicializa a máquina PIO com o programa ws2812_program
    ws2812_program_init(np_pio, np_sm, offset,pin, 800000, false);
}

void np_clear(){
    for (int i = 0; i < LEDS_COUNT; i++)
    {
        put_pixel(0); // Desliga todos os LEDs
    }
}

#endif