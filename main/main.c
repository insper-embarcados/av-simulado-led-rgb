/*
 * LED blink with FreeRTOS
 */
#include <FreeRTOS.h>
#include <task.h>
#include <semphr.h>
#include <queue.h>

#include "pico/stdlib.h"
#include <stdio.h>

#include "hardware/pwm.h"
#include "hardware/adc.h"

typedef struct rgb {
    int r;
    int g;
    int b;
} rgb_t;

QueueHandle_t xQueueADC;
QueueHandle_t xQueueRGB;

#define PWM_0_GP 10
#define PWM_1_GP 11
#define PWM_2_GP 12
#define ADC_GP 27
#define ADC_MUX_ID 1

void wheel(uint WheelPos, uint8_t *r, uint8_t *g, uint8_t *b);

bool timer_0_callback(repeating_timer_t *rt) {
    adc_select_input(ADC_MUX_ID);
    uint16_t result = adc_read();
    printf("%d\n", result);

    xQueueSendFromISR(xQueueADC, &result, 0);
    return true; // keep repeating
}

void adc_task(void *p) {
    adc_init();
    adc_gpio_init(ADC_GP);

    int timer_0_hz = 5;
    repeating_timer_t timer_0;
    if (!add_repeating_timer_us(1000000 / timer_0_hz,
                                timer_0_callback,
                                NULL,
                                &timer_0)) {
        printf("Failed to add timer\n");
    }

    uint16_t adc_value;
    printf("oi \n");
    while (true) {
        if (xQueueReceive(xQueueADC, &adc_value, pdMS_TO_TICKS(100))) {
            adc_value = adc_value * 256 / 4095;

            uint8_t r, g, b;
            wheel(adc_value, &r, &g, &b);
            rgb_t rgb = {r, g, b};
            xQueueSend(xQueueRGB, &rgb, 0);
        }
    }
}

void init_pwm(int pwm_pin_gp, uint resolution, uint *slice_num, uint *chan_num) {
    gpio_set_function(pwm_pin_gp, GPIO_FUNC_PWM);
    uint slice = pwm_gpio_to_slice_num(pwm_pin_gp);
    uint chan = pwm_gpio_to_channel(pwm_pin_gp);
    pwm_set_clkdiv(slice, 125); // pwm clock should now be running at 1MHz
    pwm_set_wrap(slice, resolution);
    pwm_set_chan_level(slice, PWM_CHAN_A, 0);
    pwm_set_enabled(slice, true);

    *slice_num = slice;
    *chan_num = chan;
}

void led_task(void *p) {
    int pwm_r_slice, pwm_r_chan;
    int pwm_g_slice, pwm_g_chan;
    int pwm_b_slice, pwm_b_chan;
    init_pwm(PWM_0_GP, 256, &pwm_r_slice, &pwm_r_chan);
    init_pwm(PWM_1_GP, 256, &pwm_g_slice, &pwm_g_chan);
    init_pwm(PWM_2_GP, 256, &pwm_b_slice, &pwm_b_chan);

    rgb_t rgb;
    while (true) {

        if (xQueueReceive(xQueueRGB, &rgb, pdMS_TO_TICKS(100))) {
            printf("r: %d, g: %d, b: %d\n", rgb.r, rgb.g, rgb.b);
            pwm_set_chan_level(pwm_r_slice, pwm_r_chan, rgb.r);
            pwm_set_chan_level(pwm_g_slice, pwm_g_chan, rgb.g);
            pwm_set_chan_level(pwm_b_slice, pwm_b_chan, rgb.b);
        }
    }
}

void wheel(uint WheelPos, uint8_t *r, uint8_t *g, uint8_t *b) {
    WheelPos = 255 - WheelPos;

    if (WheelPos < 85) {
        *r = 255 - WheelPos * 3;
        *g = 0;
        *b = WheelPos * 3;
    } else if (WheelPos < 170) {
        WheelPos -= 85;
        *r = 0;
        *g = WheelPos * 3;
        *b = 255 - WheelPos * 3;
    } else {
        WheelPos -= 170;
        *r = WheelPos * 3;
        *g = 255 - WheelPos * 3;
        *b = 0;
    }
}

int main() {
    stdio_init_all();
    printf("oi\n");
    xQueueADC = xQueueCreate(32, sizeof(uint16_t));
    xQueueRGB = xQueueCreate(32, sizeof(rgb_t));

    xTaskCreate(adc_task, "adc", 4095, NULL, 1, NULL);
    xTaskCreate(led_task, "led", 4095, NULL, 1, NULL);

    vTaskStartScheduler();

    while (true)
        ;
}
