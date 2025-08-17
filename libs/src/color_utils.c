#include "color_utils.h"
#include <math.h>   // Para as funções de ponto flutuante (HSV)

/**
 * @brief Mapeia um número de uma faixa de valores para outra, mantendo a proporção.
 * 
 * @param x O número a ser mapeado.
 * @param in_min O limite inferior da faixa de entrada.
 * @param in_max O limite superior da faixa de entrada.
 * @param out_min O limite inferior da faixa de saída.
 * @param out_max O limite superior da faixa de saída.
 * @return O valor mapeado na faixa de saída.
 */
uint8_t map(long x, long in_min, long in_max, long out_min, long out_max) {
    // Garante que o valor de entrada não esteja fora dos limites esperados
    if (x < in_min) x = in_min;
    if (x > in_max) x = in_max;
    // Aplica a fórmula de conversão de escala (semelhante a uma regra de três)
    return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min; 
}

// --- Funções de Lógica de Cor (HSV) ---

/**
 * @brief Converte um valor de cor do espaço RGB para o espaço HSV.
 * @param r Componente Vermelho (0 a 255).
 * @param g Componente Verde (0 a 255).
 * @param b Componente Azul (0 a 255).
 * @param h Ponteiro para armazenar o Hue (Matiz) resultante (0 a 360).
 * @param s Ponteiro para armazenar a Saturation (Saturação) resultante (0 a 1).
 * @param v Ponteiro para armazenar o Value (Valor/Brilho) resultante (0 a 1).
 */
void RGBtoHSV(float r, float g, float b, float *h, float *s, float *v) {
    // Normaliza os valores R, G, B para a faixa de 0 a 1
    r /= 255.0f; 
    g /= 255.0f; 
    b /= 255.0f; 
    
    // Encontra os componentes máximo (cmax) e mínimo (cmin)
    float cmax = fmaxf(r, fmaxf(g, b));
    float cmin = fminf(r, fminf(g, b));
    float diff = cmax - cmin;
    
    // Calcula o Hue (H)
    *h = -1; // Valor padrão
    if (cmax == cmin) {
        *h = 0; // Se cmax == cmin, a cor é um tom de cinza, Hue é 0
    } else if (cmax == r) {
        *h = fmodf(60 * ((g - b) / diff) + 360, 360);
    } else if (cmax == g) {
        *h = fmodf(60 * ((b - r) / diff) + 120, 360);
    } else if (cmax == b) {
        *h = fmodf(60 * ((r - g) / diff) + 240, 360);
    }
    
    // Calcula a Saturation (S)
    if (cmax == 0) {
        *s = 0; // Se a cor é preta, a saturação é 0
    } else {
        *s = (diff / cmax);
    }
    
    // O Value (V) é simplesmente o componente máximo
    *v = cmax;
}

/**
 * @brief Identifica o nome de uma cor com base em seus componentes HSV.
 * @param h Hue (Matiz) da cor (0 a 360).
 * @param s Saturation (Saturação) da cor (0 a 1).
 * @param v Value (Valor/Brilho) da cor (0 a 1).
 * @return Uma enumeração `CorIdentificada` representando a cor detectada.
 */
CorIdentificada identificar_cor_hsv(float h, float s, float v) {
    // Se o brilho é muito baixo, a cor é indefinida (ou preta)
    if (v < 0.20f) return INDEFINIDO;
    // Se a saturação é muito baixa, é um tom de cinza ou branco
    if (s < 0.25f) {
        if (v > 0.9f) return BRANCO;
    }
    // Compara o Hue com as faixas de cada cor no círculo cromático
    if ((h >= 0 && h < 15) || (h >= 345 && h <= 360)) return VERMELHO;
    if (h >= 40 && h < 75) return AMARELO;
    if (h >= 75 && h < 165) return VERDE;
    if (h >= 165 && h < 195) return CIANO;
    if (h >= 195 && h < 255) return AZUL;
    if (h >= 285 && h < 345) return MAGENTA;
    
    return INDEFINIDO;
}

/**
 * @brief Retorna uma cor RGB "pura" (valor máximo) para uma dada cor identificada.
 * @param cor A cor identificada pela enumeração.
 * @return Uma estrutura `CorRGB` com os valores R, G, B puros.
 */
CorRGB obter_rgb_para_cor(CorIdentificada cor) {
    switch (cor) {
        case VERMELHO:  return (CorRGB){255, 0, 0};
        case VERDE:     return (CorRGB){0, 255, 0};
        case AZUL:      return (CorRGB){0, 0, 255};
        case BRANCO:    return (CorRGB){255, 255, 255};
        case AMARELO:   return (CorRGB){255, 255, 0};
        case CIANO:     return (CorRGB){0, 255, 255};
        case MAGENTA:   return (CorRGB){255, 0, 255};
        default:        return (CorRGB){0, 0, 0}; // Cor preta para indefinido/preto
    }
}

/**
 * @brief Converte uma enumeração `CorIdentificada` em uma string de texto para exibição.
 * @param cor A cor identificada.
 * @return Uma string com o nome da cor.
 */
const char* obter_nome_para_cor(CorIdentificada cor) {
    switch (cor) {
        case VERMELHO:  return "Vermelho";
        case VERDE:     return "Verde";
        case AZUL:      return "Azul";
        case BRANCO:    return "Branco";
        case AMARELO:   return "Amarelo";
        case CIANO:     return "Ciano";
        case MAGENTA:   return "Magenta";
        case PRETO:     return "Preto";
        default:        return "Indefinido";
    }
}