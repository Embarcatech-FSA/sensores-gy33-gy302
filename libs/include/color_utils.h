// color_utils.h

#ifndef COLOR_UTILS_H
#define COLOR_UTILS_H

#include <stdint.h> // Necessário para usar uint8_t

// --- Estruturas e Enumerações para a Lógica de Cor ---
typedef enum {
    INDEFINIDO, PRETO, BRANCO, VERMELHO, AMARELO, VERDE, CIANO, AZUL, MAGENTA
} CorIdentificada;

typedef struct {
    uint8_t r;
    uint8_t g;
    uint8_t b;
} CorRGB;


// --- Protótipos das Funções ---

uint8_t map(long x, long in_min, long in_max, long out_min, long out_max);
void RGBtoHSV(float r, float g, float b, float *h, float *s, float *v);
CorIdentificada identificar_cor_hsv(float h, float s, float v);
CorRGB obter_rgb_para_cor(CorIdentificada cor);
const char* obter_nome_para_cor(CorIdentificada cor);


#endif // COLOR_UTILS_H