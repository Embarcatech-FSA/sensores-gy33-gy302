#include <stdio.h>
#include <string.h>
#include <stdlib.h> // Para a função abs()
#include "pico/stdlib.h"
#include "hardware/i2c.h"
#include "hardware/gpio.h"
#include "hardware/pwm.h"

// Inclusão das bibliotecas dos periféricos
#include "bh1750.h"
#include "ssd1306.h"
#include "ws2812.h"
#include "gy33.h"

#include "config.h"
#include "color_utils.h"

// --- Variáveis Globais de Estado ---
volatile int led_state = 0;
volatile uint32_t last_press_time = 0;
volatile bool led_enabled = false;


// --- Variáveis Globais ---
ssd1306_t disp;
uint buzzer_slice_num;

// --- Definições das Funções ---

void init_buzzer();
void play_alert_tone();
void init_leds();
void gpio_irq_handler(uint gpio, uint32_t events);
void switch_led_color();



// --- Função Principal ---
int main() {
    stdio_init_all();
    sleep_ms(2000);

    // Inicialização de todos os periféricos e interfaces
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
    init_leds();
    
    // Habilita as interrupções para os botões
    gpio_set_irq_enabled_with_callback(BUTTON_A, GPIO_IRQ_EDGE_FALL, true, &gpio_irq_handler);
    gpio_set_irq_enabled_with_callback(BUTTON_B, GPIO_IRQ_EDGE_FALL, true, &gpio_irq_handler);
    
    bool matriz[LEDS_COUNT] = {0,0,0,0,0, 0,1,1,1,0, 0,1,1,1,0, 0,1,1,1,0, 0,0,0,0,0}; 
    char oled_buffer[40];
    uint16_t r, g, b, c;

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
        uint8_t brilho = map(lux, 10, 80, 1, 255); // Ajuste do brilho baseado na luminosidade
        uint8_t r_final = (cor_led_pura.r * brilho) / 255;
        uint8_t g_final = (cor_led_pura.g * brilho) / 255;
        uint8_t b_final = (cor_led_pura.b * brilho) / 255;
        np_set_leds(matriz, r_final, g_final, b_final);
        
        // --- Exibição no Display OLED ---
        ssd1306_fill(&disp, false);
        sprintf(oled_buffer, "Cor: %s", obter_nome_para_cor(cor_atual));
        ssd1306_draw_string(&disp, oled_buffer, 0, 0);
        sprintf(oled_buffer, "H:%3.0f S:%.2f V:%.2f", h, s, v);
        ssd1306_draw_string(&disp, oled_buffer, 0, 16);
        sprintf(oled_buffer, "R:%-5u G:%-5u", r, g);
        ssd1306_draw_string(&disp, oled_buffer, 0, 36);
        sprintf(oled_buffer, "B:%-5u Lux:%-4u", b, lux);
        ssd1306_draw_string(&disp, oled_buffer, 0, 52);
        ssd1306_send_data(&disp);
        
        // --- Controle do LED RGB ---
        switch_led_color();
        sleep_ms(200);
    }

    return 0;
}


/**
 * @brief Inicializa o pino do buzzer para operar com PWM.
 */
void init_buzzer() {
    gpio_set_function(BUZZER_PIN, GPIO_FUNC_PWM);
    buzzer_slice_num = pwm_gpio_to_slice_num(BUZZER_PIN);
    pwm_config config = pwm_get_default_config();
    pwm_config_set_clkdiv(&config, 488.0f);
    pwm_config_set_wrap(&config, 255);
    pwm_init(buzzer_slice_num, &config, true);
    pwm_set_gpio_level(BUZZER_PIN, 0);
}

/**
 * @brief Toca um som de alerta curto (dois bipes) no buzzer.
 */
void play_alert_tone() {
    pwm_set_gpio_level(BUZZER_PIN, 128);
    sleep_ms(80);
    pwm_set_gpio_level(BUZZER_PIN, 0);
    sleep_ms(80);
    pwm_set_gpio_level(BUZZER_PIN, 128);
    sleep_ms(80);
    pwm_set_gpio_level(BUZZER_PIN, 0);
}


// --- Funções de Controle do LED RGB e Botões ---

/**
 * @brief Inicializa os pinos dos LEDs RGB como saídas e dos botões como entradas com pull-up.
 */
void init_leds() {
    // Configura os pinos dos LEDs
    gpio_init(LED_RED);
    gpio_init(LED_GREEN);
    gpio_init(LED_BLUE);
    gpio_set_dir(LED_RED, GPIO_OUT);
    gpio_set_dir(LED_GREEN, GPIO_OUT);
    gpio_set_dir(LED_BLUE, GPIO_OUT);
    gpio_put(LED_RED, 0);
    gpio_put(LED_GREEN, 0);
    gpio_put(LED_BLUE, 0);

    // Configura os pinos dos botões
    gpio_init(BUTTON_A);
    gpio_init(BUTTON_B);
    gpio_set_dir(BUTTON_A, GPIO_IN);
    gpio_set_dir(BUTTON_B, GPIO_IN);
    gpio_pull_up(BUTTON_A);
    gpio_pull_up(BUTTON_B);
}

/**
 * @brief Função de callback que trata as interrupções geradas pelos botões.
 * @param gpio O pino que gerou a interrupção.
 * @param events O tipo de evento (ex: borda de descida).
 */
void gpio_irq_handler(uint gpio, uint32_t events){
    uint32_t current_time = to_ms_since_boot(get_absolute_time());
    // Lógica de Debounce
    if (current_time - last_press_time > 250) {
        last_press_time = current_time;
        if(gpio == BUTTON_A){
            led_enabled = !led_enabled; // Alterna o estado do LED
        } else if(gpio == BUTTON_B){
            led_state++; // Avança para a próxima cor
            if (led_state > 6) {
                led_state = 0; // Volta ao início
            }
        }
    }
}

/**
 * @brief Atualiza o estado físico do LED RGB com base nas variáveis globais `led_enabled` e `led_state`.
 */
void switch_led_color() {
    if(led_enabled){
        switch (led_state) {
            case 0: gpio_put(LED_RED, 1); gpio_put(LED_GREEN, 0); gpio_put(LED_BLUE, 0); break;
            case 1: gpio_put(LED_RED, 0); gpio_put(LED_GREEN, 1); gpio_put(LED_BLUE, 0); break;
            case 2: gpio_put(LED_RED, 0); gpio_put(LED_GREEN, 0); gpio_put(LED_BLUE, 1); break;
            case 3: gpio_put(LED_RED, 1); gpio_put(LED_GREEN, 1); gpio_put(LED_BLUE, 0); break;
            case 4: gpio_put(LED_RED, 0); gpio_put(LED_GREEN, 1); gpio_put(LED_BLUE, 1); break;
            case 5: gpio_put(LED_RED, 1); gpio_put(LED_GREEN, 0); gpio_put(LED_BLUE, 1); break;
            case 6: gpio_put(LED_RED, 1); gpio_put(LED_GREEN, 1); gpio_put(LED_BLUE, 1); break;
            default: gpio_put(LED_RED, 0); gpio_put(LED_GREEN, 0); gpio_put(LED_BLUE, 0); break;
        }
    } else {
        gpio_put(LED_RED, 0);
        gpio_put(LED_GREEN, 0);
        gpio_put(LED_BLUE, 0);
    }
}