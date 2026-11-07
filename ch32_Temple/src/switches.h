#ifndef SWITCHES_H
#define SWITCHES_H

#include <ch32v00x.h>

/*
 * Switch input abstraction.
 * All switches are active-low with internal pull-ups enabled, so a pressed switch reads as logic 0.
 */
typedef enum {
    SWITCH_1 = 0,
    SWITCH_2 = 1,
    SWITCH_3 = 2,
    SWITCH_4 = 3
} SwitchIndex;

bool switch_status[4] = {0};
bool switch_status_old[4] = {0};

/* Translate a logical switch index to the corresponding hardware pin. */
static inline uint16_t SwitchPin(SwitchIndex sw)
{
#if 1
    switch (sw) {
        case SWITCH_1: return GPIO_Pin_4;
        case SWITCH_2: return GPIO_Pin_0;
        case SWITCH_3: return GPIO_Pin_1;
        case SWITCH_4: return GPIO_Pin_2;
        default:       return 0;
    }
#endif
#if 0
        switch (sw) {
        case SWITCH_1: return GPIO_Pin_2;
        case SWITCH_2: return GPIO_Pin_4;
        case SWITCH_3: return GPIO_Pin_1;
        case SWITCH_4: return GPIO_Pin_0;
        default:       return 0;
    }
    #endif
}

/* Switch 1 sits on PD4, switch 2 on PC0, switches 3 and 4 on PA1 and PA2. */
static inline GPIO_TypeDef *SwitchPort(SwitchIndex sw)
{
    switch (sw) {
        case SWITCH_1: return GPIOD;
        case SWITCH_2: return GPIOC;
        default:       return GPIOA;
    }
}

/* Configure the switch GPIOs as pull-up inputs. */
static inline void Switches_Init(void)
{
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOD | RCC_APB2Periph_GPIOC | RCC_APB2Periph_GPIOA, ENABLE);

    GPIO_InitTypeDef gpio = {0};
    gpio.GPIO_Mode = GPIO_Mode_IPU;
    gpio.GPIO_Speed = GPIO_Speed_50MHz;

    gpio.GPIO_Pin = GPIO_Pin_4;
    GPIO_Init(GPIOD, &gpio);

    gpio.GPIO_Pin = GPIO_Pin_0;
    GPIO_Init(GPIOC, &gpio);

    gpio.GPIO_Pin = GPIO_Pin_1 | GPIO_Pin_2;
    GPIO_Init(GPIOA, &gpio);
}

/* Return 1 when the switch is pressed / logically ON. */
#if 0
static inline uint8_t Switch_IsOn(SwitchIndex sw)
{
    uint16_t pin = SwitchPin(sw);
    GPIO_TypeDef *port = SwitchPort(sw);

    if (pin == 0) {
        return 0;
    }

    return (GPIO_ReadInputDataBit(port, pin) == Bit_RESET) ? 1U : 0U;
}
    #endif


static inline bool Switch_IsOn(uint8_t sw)
{
    uint16_t pin = SwitchPin(sw);
    GPIO_TypeDef *port = SwitchPort(sw);

    if (pin == 0) {
        return 0;
    }

    return (GPIO_ReadInputDataBit(port, pin) == Bit_RESET) ? 1 : 0;
}

static inline void Get_Switch_status()
{
   for(uint8_t i = 0; i < 4; i++) {
       switch_status[i] = Switch_IsOn(i);
   }
}

#endif
