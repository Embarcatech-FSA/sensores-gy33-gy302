#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "hardware/i2c.h"
#include "hardware/gpio.h"
#include "hardware/pwm.h"

// Inclusão das bibliotecas dos periféricos
#include "bh1750.h"
#include "ssd1306.h"
#include "ws2812.h"
#include "gy33.h"
#include "mlp.h"

#include "config.h"
#include "color_utils.h"

// --- Variáveis Globais de Estado ---
volatile int led_state = 0;
volatile uint32_t last_press_time = 0;
volatile bool led_enabled = false;


// --- Variáveis Globais ---
ssd1306_t disp;
uint buzzer_slice_num;
bool screen = true;

// --- Definições das Funções ---

void init_buzzer();
void play_alert_tone();
void init_leds_buttons();
void gpio_irq_handler(uint gpio, uint32_t events);
void switch_led_color();
void init_i2c();
void trained_mlp_model(); 
int get_ambient_mode(); 

// -- Multilayer Perceptron
MLP mlp;
#define INPUT_LAYER_LEN 3
#define HIDDEN_LAYER_LEN 5
#define OUTPUT_LAYER_LEN 3

// Cores e luminosidade
uint8_t r_norm = 0.0;
uint8_t g_norm = 0.0;
uint8_t b_norm = 0.0;
uint16_t lux = 0.0;


// --- Função Principal ---
int main() {
    stdio_init_all();
    sleep_ms(1000);

    // Inicialização de todos os periféricos e interfaces

    np_init(WS2812_PIN);
    np_clear();
    init_buzzer();
    init_leds_buttons();
    init_i2c();
    gy33_init();
    bh1750_power_on(I2C_PORT_SENSORS);

    ssd1306_init(&disp, 128, 64, false, ADDRESS_DISPLAY, I2C_PORT_DISPLAY);
    ssd1306_config(&disp);
    ssd1306_draw_string(&disp, "Iniciando...", 0, 0);
    ssd1306_send_data(&disp);
    
    sleep_ms(1000);
    
    // Habilita as interrupções para os botões
    gpio_set_irq_enabled_with_callback(BUTTON_A, GPIO_IRQ_EDGE_FALL, true, &gpio_irq_handler);
    gpio_set_irq_enabled_with_callback(BUTTON_B, GPIO_IRQ_EDGE_FALL, true, &gpio_irq_handler);
    gpio_set_irq_enabled_with_callback(BTN_JOYSTICK, GPIO_IRQ_EDGE_FALL, true, &gpio_irq_handler);
    
    bool matriz[LEDS_COUNT] = {0,0,0,0,0, 0,1,1,1,0, 0,1,1,1,0, 0,1,1,1,0, 0,0,0,0,0}; 
    char oled_buffer[128];
    uint16_t r, g, b, c;
    int mode;

    trained_mlp_model(); // Aplica o modelo treinado


    while (1) {
        // --- Leitura e Processamento ---
        lux = bh1750_read_measurement(I2C_PORT_SENSORS);
        gy33_read_color(&r, &g, &b, &c);
        r_norm = map(r, 0, SENSOR_COLOR_MAX_VALUE, 0, 255);
        g_norm = map(g, 0, SENSOR_COLOR_MAX_VALUE, 0, 255);
        b_norm = map(b, 0, SENSOR_COLOR_MAX_VALUE, 0, 255);
        printf("Lux: %u, R: %u, G: %u, B: %u\n", lux, r_norm, g_norm, b_norm);
        float h, s, v;
        RGBtoHSV(r_norm, g_norm, b_norm, &h, &s, &v);
        CorIdentificada cor_atual = identificar_cor_hsv(h, s, v);

        // --- Lógica de Alertas ---
        bool low_light_alert = lux < LUMINOSITY_THRESHOLD; // Se a luminosidade está abaixo do limiar, alerta de baixa luminosidade
        bool intense_red_alert = (cor_atual == VERMELHO && s > 0.6f && v > 0.7f); // Se a saturação e o valor são altos, indica vermelho intenso
        if (low_light_alert || intense_red_alert) {
            play_alert_tone();
        }

        // --- Atualização da Matriz de LED ---
        CorRGB cor_led_pura = obter_rgb_para_cor(cor_atual);
        uint8_t brilho = map(lux, LUMINOSITY_THRESHOLD, LUMINOSITY_MAX, 1, 255); // Ajuste do brilho baseado na luminosidade
        uint8_t r_final = (cor_led_pura.r * brilho) / 255;
        uint8_t g_final = (cor_led_pura.g * brilho) / 255;
        uint8_t b_final = (cor_led_pura.b * brilho) / 255;
        np_set_leds(matriz, r_final, g_final, b_final);

        mode = get_ambient_mode();
        
        // --- Exibição no Display OLED ---
        ssd1306_fill(&disp, false);
        sprintf(oled_buffer, "Cor: %s", obter_nome_para_cor(cor_atual));
        ssd1306_draw_string(&disp, oled_buffer, 0, 0);
        if(screen) {
            sprintf(oled_buffer, "H:%3.0f", h);
            ssd1306_draw_string(&disp, oled_buffer, 34, 16);
            sprintf(oled_buffer, "S:%.2f", s);
            ssd1306_draw_string(&disp, oled_buffer, 34, 26);
            sprintf(oled_buffer, "V:%.2f", v);
            ssd1306_draw_string(&disp, oled_buffer, 34, 36);
        } else {
            sprintf(oled_buffer, "R:%u", r_norm);
            ssd1306_draw_string(&disp, oled_buffer, 35, 16);
            sprintf(oled_buffer, "G:%u", g_norm);
            ssd1306_draw_string(&disp, oled_buffer, 35, 26);
            sprintf(oled_buffer, "B:%u", b_norm);
            ssd1306_draw_string(&disp, oled_buffer, 35, 36);
        }
        sprintf(oled_buffer, "Lux:%u", lux);
        ssd1306_draw_string(&disp, oled_buffer, 0, 52);
        sprintf(oled_buffer, (mode==0)?"Idle":(mode==1)?"Work":(mode==2)?"Fest":"????");
        ssd1306_draw_string(&disp, oled_buffer, 90, 52);
        ssd1306_send_data(&disp);
        
        // --- Controle do LED RGB ---
        switch_led_color();

        printf("\n\nModo do ambiente: %i\n\n", get_ambient_mode());
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
void init_leds_buttons() {
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
    gpio_init(BTN_JOYSTICK);
    gpio_set_dir(BUTTON_A, GPIO_IN);
    gpio_set_dir(BUTTON_B, GPIO_IN);
    gpio_set_dir(BTN_JOYSTICK, GPIO_IN);
    gpio_pull_up(BUTTON_A);
    gpio_pull_up(BUTTON_B);
    gpio_pull_up(BTN_JOYSTICK);
}

/**
 * @brief Inicializa o I2C para os sensores e o display.
 * Esta função configura os pinos SDA e SCL para os barramentos I2C dos sensores e do display.
 */
void init_i2c(){

    /* I2C dos sensores GY-33 e GY-302 */
    i2c_init(I2C_PORT_SENSORS, 100 * 1000);
    gpio_set_function(I2C_SDA_SENSORS, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL_SENSORS, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA_SENSORS); 
    gpio_pull_up(I2C_SCL_SENSORS);

    /* I2C do display SSD1306 */
    i2c_init(I2C_PORT_DISPLAY, 400 * 1000);
    gpio_set_function(I2C_SDA_DISPLAY, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL_DISPLAY, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA_DISPLAY);
    gpio_pull_up(I2C_SCL_DISPLAY);
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
        } else if(gpio == BTN_JOYSTICK) {
            screen = !screen;
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

int get_ambient_mode() {
    float xMin[3] = {0.0, 0.0, 0.0};
    float xMax[3] = {255.0, 255.0, 255.0};
    float yMin[3] = {0.0, 0.0, 0.0};
    float yMax[3] = {1.0, 1.0, 1.0};

    float X[3] = {r_norm, g_norm, b_norm};

    // Normalização
    for (int j = 0; j < 3; j++) {
        X[j] = (X[j] - xMin[j]) / (xMax[j] - xMin[j]);
    }

    // Forward MLP
    forward(&mlp, X);

    float *o = mlp.output_layer_outputs;
    float threshold_one = 0.95f;
    float threshold_zero = 0.05f;

    // Desnormaliza saída
    for (int k = 0; k < mlp.output_layer_length; k++) {
        o[k] = o[k] * (yMax[k] - yMin[k]) + yMin[k];
    }

    printf("\nMLP output: %.2f %.2f %.2f\n", o[0], o[1], o[2]);

    // Verifica saída “quase perfeita”
    for (int i = 0; i < mlp.output_layer_length; i++) {
        if (o[i] >= threshold_one) {
            int others_are_zero = 1;
            for (int j = 0; j < mlp.output_layer_length; j++) {
                if (j != i && o[j] > threshold_zero) {
                    others_are_zero = 0;
                    break;
                }
            }

            if (others_are_zero) {
                // Verificação extra com Lux
                if (i == 0 && (lux >= 10 && lux <= 300)) return 0; // Relax
                if (i == 1 && (lux >= 300 && lux <= 700)) return 1; // Work
                if (i == 2 && (lux >= 700 && lux <= 1000)) return 2; // Party

                return 3; // Lux fora da faixa → Outlier
            }
        }
    }

    return 3; // Incerto
}



void trained_mlp_model() {
    mlp.input_layer_length = INPUT_LAYER_LEN;
    mlp.hidden_layer_length = HIDDEN_LAYER_LEN;
    mlp.output_layer_length = OUTPUT_LAYER_LEN;

    float hidden_layer_weights[HIDDEN_LAYER_LEN][INPUT_LAYER_LEN+1] = {
        {2.661857, 6.408717, 1.197877, -5.405861, },
        {-2.044108, -5.768311, -0.194693, 4.042969, },
        {3.156306, -5.066918, -4.585429, 1.241510, },
        {-12.349979, -2.461361, 3.371196, 4.594296, },
        {4.260282, -4.847218, -4.883586, 0.496394, },
    };

    float output_layer_weights[OUTPUT_LAYER_LEN ][HIDDEN_LAYER_LEN+1] = {
        {-5.191444, 1.899328, -2.282097, 12.606183, -3.801378, -3.360971, },
        {8.362973, -6.287300, -4.975049, -11.016505, -4.422555, 2.839259, },
        {-6.135138, 2.244717, 5.509221, -5.429541, 6.808523, -3.378476, },
    };

    // Alocar pesos da hidden layer
    mlp.hidden_layer_weights = (float**) malloc(mlp.hidden_layer_length * sizeof(float*));
    for (int i = 0; i < mlp.hidden_layer_length; i++) {
        mlp.hidden_layer_weights[i] = (float*) malloc((mlp.input_layer_length + 1) * sizeof(float));
        for (int j = 0; j < (mlp.input_layer_length + 1); j++) {
            mlp.hidden_layer_weights[i][j] = hidden_layer_weights[i][j];
        }
    }

    // Alocar pesos da output layer
    mlp.output_layer_weights = (float**) malloc(mlp.output_layer_length * sizeof(float*));
    for (int i = 0; i < mlp.output_layer_length; i++) {
        mlp.output_layer_weights[i] = (float*) malloc((mlp.hidden_layer_length + 1) * sizeof(float));
        for (int j = 0; j < (mlp.hidden_layer_length + 1); j++) {
            mlp.output_layer_weights[i][j] = output_layer_weights[i][j];
        }
    }

    // Alocar saídas
    mlp.hidden_layer_outputs = (float*) malloc(mlp.hidden_layer_length * sizeof(float));
    mlp.output_layer_outputs = (float*) malloc(mlp.output_layer_length * sizeof(float));
}