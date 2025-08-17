#include <stdio.h>
#include <string.h>
#include <stdlib.h> // Para a função abs()
#include <math.h>   // Para as funções de ponto flutuante (HSV)
#include "pico/stdlib.h"
#include "hardware/i2c.h"
#include "hardware/gpio.h"
#include "hardware/pwm.h"

// Inclusão das bibliotecas dos periféricos
#include "bh1750.h"
#include "ssd1306.h"
#include "ws2812.h"

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
#define LUMINOSITY_THRESHOLD 0
#define SENSOR_COLOR_MAX_VALUE 3000 // Valor para normalização dos dados brutos

#define LED_RED 13
#define LED_BLUE 12
#define LED_GREEN 11
#define BUTTON_A 5
#define BUTTON_B 6

volatile int led_state = 0;            // 0=Vermelho, 1=Verde, 2=Azul
volatile uint32_t last_press_time = 0; // Armazena o tempo do último aperto para o debounce
volatile bool led_enabled = false; // Flag para controlar o estado dos LEDs

// --- Estruturas e Enumerações para a Lógica de Cor ---
typedef enum {
    INDEFINIDO, PRETO, BRANCO, VERMELHO, AMARELO, VERDE, CIANO, AZUL, MAGENTA
} CorIdentificada;

typedef struct {
    uint8_t r;
    uint8_t g;
    uint8_t b;
} CorRGB;

// --- Variáveis Globais ---
ssd1306_t disp;
uint buzzer_slice_num;

// --- Protótipos de Funções ---
void gy33_init();
void gy33_read_color(uint16_t *r, uint16_t *g, uint16_t *b, uint16_t *c);
uint8_t map(long x, long in_min, long in_max, long out_min, long out_max);
void init_buzzer();
void play_alert_tone();
void RGBtoHSV(float r, float g, float b, float *h, float *s, float *v);
CorIdentificada identificar_cor_hsv(float h, float s, float v);
CorRGB obter_rgb_para_cor(CorIdentificada cor);
const char* obter_nome_para_cor(CorIdentificada cor);

void init_leds();
void gpio_irq_handler(uint gpio, uint32_t events);
void switch_led_color();

// --- Funções do Sensor e Periféricos ---
// (Sem alterações, continuam as mesmas das versões anteriores)
void gy33_write_register(uint8_t reg, uint8_t value) { uint8_t buffer[2] = {reg | GY33_COMMAND_BIT, value}; i2c_write_blocking(I2C_PORT_SENSORS, GY33_I2C_ADDR, buffer, 2, false); }
uint16_t gy33_read_register(uint8_t reg) { uint8_t buffer[2]; uint8_t val = reg | GY33_COMMAND_BIT; i2c_write_blocking(I2C_PORT_SENSORS, GY33_I2C_ADDR, &val, 1, true); i2c_read_blocking(I2C_PORT_SENSORS, GY33_I2C_ADDR, buffer, 2, false); return (buffer[1] << 8) | buffer[0]; }
void gy33_init() { gy33_write_register(ENABLE_REG, 0x01); sleep_ms(3); gy33_write_register(ENABLE_REG, 0x03); gy33_write_register(ATIME_REG, 0xD5); gy33_write_register(CONTROL_REG, 0x02); }
void gy33_read_color(uint16_t *r, uint16_t *g, uint16_t *b, uint16_t *c) { *c = gy33_read_register(CDATA_REG); *r = gy33_read_register(RDATA_REG); *g = gy33_read_register(GDATA_REG); *b = gy33_read_register(BDATA_REG); }
void init_buzzer() { gpio_set_function(BUZZER_PIN, GPIO_FUNC_PWM); buzzer_slice_num = pwm_gpio_to_slice_num(BUZZER_PIN); pwm_config config = pwm_get_default_config(); pwm_config_set_clkdiv(&config, 488.0f); pwm_config_set_wrap(&config, 255); pwm_init(buzzer_slice_num, &config, true); pwm_set_gpio_level(BUZZER_PIN, 0); }
void play_alert_tone(){ pwm_set_gpio_level(BUZZER_PIN, 128); sleep_ms(80); pwm_set_gpio_level(BUZZER_PIN, 0); sleep_ms(80); pwm_set_gpio_level(BUZZER_PIN, 128); sleep_ms(80); pwm_set_gpio_level(BUZZER_PIN, 0); }
uint8_t map(long x, long in_min, long in_max, long out_min, long out_max) { if (x < in_min) x = in_min; if (x > in_max) x = in_max; return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min; }

// ##### FUNÇÕES DE LÓGICA DE COR AVANÇADA (HSV) #####
void RGBtoHSV(float r, float g, float b, float *h, float *s, float *v) {
    r /= 255.0f; g /= 255.0f; b /= 255.0f; 
    float cmax = fmaxf(r, fmaxf(g, b));
    float cmin = fminf(r, fminf(g, b));
    float diff = cmax - cmin;
    *h = -1; *s = -1;
    if (cmax == cmin) *h = 0;
    else if (cmax == r) *h = fmodf(60 * ((g - b) / diff) + 360, 360);
    else if (cmax == g) *h = fmodf(60 * ((b - r) / diff) + 120, 360);
    else if (cmax == b) *h = fmodf(60 * ((r - g) / diff) + 240, 360);
    if (cmax == 0) *s = 0; else *s = (diff / cmax);
    *v = cmax;
}
CorIdentificada identificar_cor_hsv(float h, float s, float v) {
    if (v < 0.20f) return INDEFINIDO;
    if (s < 0.25f) {
        if (v > 0.9f) return BRANCO;
    }
    if ((h >= 0 && h < 15) || (h >= 345 && h <= 360)) return VERMELHO;
    if (h >= 40 && h < 75) return AMARELO;
    if (h >= 75 && h < 165) return VERDE;
    if (h >= 165 && h < 195) return CIANO;
    if (h >= 195 && h < 255) return AZUL;
    if (h >= 285 && h < 345) return MAGENTA;
    return INDEFINIDO;
}
CorRGB obter_rgb_para_cor(CorIdentificada cor) {
    switch (cor) {
        case VERMELHO:  return (CorRGB){255, 0, 0};
        case VERDE:     return (CorRGB){0, 255, 0};
        case AZUL:      return (CorRGB){0, 0, 255};
        case BRANCO:    return (CorRGB){255, 255, 255};
        case AMARELO:   return (CorRGB){255, 255, 0};
        case CIANO:     return (CorRGB){0, 255, 255};
        case MAGENTA:   return (CorRGB){255, 0, 255};
        default:        return (CorRGB){0, 0, 0};
    }
}
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

// --- Função Principal ---
int main() {
    stdio_init_all();
    sleep_ms(2000);

    // Inits...
    i2c_init(I2C_PORT_SENSORS, 100 * 1000);
    gpio_set_function(I2C_SDA_SENSORS, GPIO_FUNC_I2C); gpio_set_function(I2C_SCL_SENSORS, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA_SENSORS); gpio_pull_up(I2C_SCL_SENSORS);
    i2c_init(I2C_PORT_DISPLAY, 400 * 1000);
    gpio_set_function(I2C_SDA_DISPLAY, GPIO_FUNC_I2C); gpio_set_function(I2C_SCL_DISPLAY, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA_DISPLAY); gpio_pull_up(I2C_SCL_DISPLAY);
    init_buzzer();
    ssd1306_init(&disp, 128, 64, false, ADDRESS_DISPLAY, I2C_PORT_DISPLAY);
    ssd1306_config(&disp);
    ssd1306_draw_string(&disp, "Iniciando...", 0, 0);
    ssd1306_send_data(&disp);
    sleep_ms(1000);
    bh1750_power_on(I2C_PORT_SENSORS);
    gy33_init();
    np_init(WS2812_PIN);
    np_clear();

    bool led_pattern[LEDS_COUNT] = {0,0,0,0,0, 0,1,1,1,0, 0,1,1,1,0, 0,1,1,1,0, 0,0,0,0,0}; 
    char oled_buffer[40];
    uint16_t r, g, b, c;

    init_leds();

    gpio_set_irq_enabled_with_callback(BUTTON_A, GPIO_IRQ_EDGE_FALL, true, &gpio_irq_handler);
    gpio_set_irq_enabled_with_callback(BUTTON_B, GPIO_IRQ_EDGE_FALL, true, &gpio_irq_handler);
    


    while (1) {
        // --- Leitura e Processamento ---
        uint16_t lux = bh1750_read_measurement(I2C_PORT_SENSORS);
        gy33_read_color(&r, &g, &b, &c);
        uint8_t r_norm = map(r, 0, SENSOR_COLOR_MAX_VALUE, 0, 255);
        uint8_t g_norm = map(g, 0, SENSOR_COLOR_MAX_VALUE, 0, 255);
        uint8_t b_norm = map(b, 0, SENSOR_COLOR_MAX_VALUE, 0, 255);
        printf("Lux: %u, R: %u, G: %u, B: %u\n", lux, r_norm, g_norm, b_norm);
        float h, s, v;
        RGBtoHSV(r_norm, g_norm, b_norm, &h, &s, &v);
        CorIdentificada cor_atual = identificar_cor_hsv(h, s, v);

        // --- Lógica de Alertas ---
        bool low_light_alert = lux < LUMINOSITY_THRESHOLD;
        bool intense_red_alert = (cor_atual == VERMELHO && s > 0.7f && v > 0.7f);
        if (low_light_alert || intense_red_alert) {
            play_alert_tone();
        }

        // --- Atualização da Matriz de LED ---
        CorRGB cor_led_pura = obter_rgb_para_cor(cor_atual);
        uint8_t brilho = map(lux, 10, 80, 1, 255);
        uint8_t r_final = (cor_led_pura.r * brilho) / 255;
        uint8_t g_final = (cor_led_pura.g * brilho) / 255;
        uint8_t b_final = (cor_led_pura.b * brilho) / 255;
        np_set_leds(led_pattern, r_final, g_final, b_final);
        
        // ##### BLOCO DE EXIBIÇÃO NO DISPLAY OTIMIZADO #####
        ssd1306_fill(&disp, false);

        // Linha 1: Cor Identificada
        sprintf(oled_buffer, "Cor: %s", obter_nome_para_cor(cor_atual));
        ssd1306_draw_string(&disp, oled_buffer, 0, 0);

        // Linha 2: Dados HSV (formato compacto)
        sprintf(oled_buffer, "H:%3.0f S:%.2f V:%.2f", h, s, v);
        ssd1306_draw_string(&disp, oled_buffer, 0, 16);

        // Linha 3: Dados RGB (brutos)
        sprintf(oled_buffer, "R:%-5u G:%-5u", r, g);
        ssd1306_draw_string(&disp, oled_buffer, 0, 36);

        // Linha 4: Dados de B (bruto) e Luminosidade
        sprintf(oled_buffer, "B:%-5u Lux:%-4u", b, lux);
        ssd1306_draw_string(&disp, oled_buffer, 0, 52);

        ssd1306_send_data(&disp);
        // ################################################
        
        switch_led_color(); // Atualiza a cor do LED com base no estado
        sleep_ms(200);
    }

    return 0;
}

void init_leds() {
    gpio_init(LED_RED);
    gpio_init(LED_GREEN);
    gpio_init(LED_BLUE);

    gpio_set_dir(LED_RED, GPIO_OUT);
    gpio_set_dir(LED_GREEN, GPIO_OUT);
    gpio_set_dir(LED_BLUE, GPIO_OUT);

    gpio_put(LED_RED, 0);
    gpio_put(LED_GREEN, 0);
    gpio_put(LED_BLUE, 0);

    gpio_init(BUTTON_A);
    gpio_init(BUTTON_B);
    gpio_set_dir(BUTTON_A, GPIO_IN);
    gpio_set_dir(BUTTON_B, GPIO_IN);
    gpio_pull_up(BUTTON_A);
    gpio_pull_up(BUTTON_B);
}

void gpio_irq_handler(uint gpio, uint32_t events){
    // Pega o tempo atual em milissegundos
    uint32_t current_time = to_ms_since_boot(get_absolute_time());
    if (current_time - last_press_time > 250)
    {
        // Atualiza o tempo do último aperto válido
        last_press_time = current_time;

        if(gpio == BUTTON_A){
            // Permite que os LEDS possam funcionar
            led_enabled = !led_enabled; // Alterna o estado dos LEDs
        } else if(gpio == BUTTON_B){
            // Muda a cor do LED (alterna entre 0 e 7)
            led_state++;
            if (led_state > 6) {
                led_state = 0; // Reseta o estado se ultrapassar 6
            }
        }
    }
}

void switch_led_color() {
    if(led_enabled){
        switch (led_state) {
            case 0: gpio_put(LED_RED, 1); gpio_put(LED_GREEN, 0); gpio_put(LED_BLUE, 0); break; // Vermelho
            case 1: gpio_put(LED_RED, 0); gpio_put(LED_GREEN, 1); gpio_put(LED_BLUE, 0); break; // Verde
            case 2: gpio_put(LED_RED, 0); gpio_put(LED_GREEN, 0); gpio_put(LED_BLUE, 1); break; // Azul
            case 3: gpio_put(LED_RED, 1); gpio_put(LED_GREEN, 1); gpio_put(LED_BLUE, 0); break; // Amarelo
            case 4: gpio_put(LED_RED, 0); gpio_put(LED_GREEN, 1); gpio_put(LED_BLUE, 1); break; // Ciano
            case 5: gpio_put(LED_RED, 1); gpio_put(LED_GREEN, 0); gpio_put(LED_BLUE, 1); break; // Magenta
            case 6: gpio_put(LED_RED, 1); gpio_put(LED_GREEN, 1); gpio_put(LED_BLUE, 1); break; // Branco
            default: gpio_put(LED_RED, 0); gpio_put(LED_GREEN, 0); gpio_put(LED_BLUE, 0); break; // Desligado
        }
    } else {
        gpio_put(LED_RED, 0);
        gpio_put(LED_GREEN, 0);
        gpio_put(LED_BLUE, 0); // Desliga todos os LEDs
    }
}