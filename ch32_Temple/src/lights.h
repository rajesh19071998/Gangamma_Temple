#ifndef LIGHTS_H
#define LIGHTS_H

#include <ch32v00x.h>

/*
 * Light output abstraction for the four board loads.
 * Each lane maps to a GPIO on port C and can be driven ON or OFF.
 */
typedef enum {
    LIGHT_1 = 0,
    LIGHT_2 = 1,
    LIGHT_3 = 2,
    LIGHT_4 = 3
} LightIndex;

/* Return the hardware GPIO pin associated with a logical light index. */
static inline uint16_t LightPin(LightIndex light)
{
    switch (light) {
        case LIGHT_1: return GPIO_Pin_3;
        case LIGHT_2: return GPIO_Pin_4;
        case LIGHT_3: return GPIO_Pin_6;
        case LIGHT_4: return GPIO_Pin_7;
        default:      return 0;
    }
}

/* Configure all light output pins as push-pull GPIO and force all outputs OFF at startup. */
static inline void Lights_Init(void)
{
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOC, ENABLE);

    GPIO_InitTypeDef gpio = {0};
    gpio.GPIO_Pin = GPIO_Pin_3 | GPIO_Pin_4 | GPIO_Pin_6 | GPIO_Pin_7;
    gpio.GPIO_Mode = GPIO_Mode_Out_PP;
    gpio.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOC, &gpio);

    GPIO_ResetBits(GPIOC, gpio.GPIO_Pin);
}

#if 0
/* Apply the ON/OFF state to the selected light output. */
static inline void Light_Set(LightIndex light, uint8_t enabled)
{
    uint16_t pin = LightPin(light);
    if (pin != 0) {
        GPIO_WriteBit(GPIOC, pin, enabled ? Bit_SET : Bit_RESET);
    }
}
#endif

static inline void Light_control_Set(int light, bool enabled)
{
    uint16_t pin = LightPin(light);
    if (pin != 0) {
        GPIO_WriteBit(GPIOC, pin, enabled ? Bit_SET : Bit_RESET);
    }
}

#endif
