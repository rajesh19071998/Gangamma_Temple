#ifndef DS3231_H
#define DS3231_H

#include <stddef.h>
#include <ch32v00x.h>
#include <debug.h>

/*
 * DS3231 real-time clock interface.
 * This module handles bit-banged I2C access to the RTC and converts between BCD and binary values.
 */

#define DS3231_I2C_ADDRESS 0x68
#define DS3231_I2C_HALF_PERIOD_US 5U

/* Bit-banged I2C; needs external pull-ups (DS3231 modules include them). */
#define DS3231_SCL_PIN GPIO_Pin_2
#define DS3231_SDA_PIN GPIO_Pin_1

#define DS3231_I2C_PORT GPIOC

#define DS3231_REG_SECONDS 0x00
#define DS3231_REG_MINUTES 0x01
#define DS3231_REG_HOURS 0x02
#define DS3231_REG_DAY 0x03
#define DS3231_REG_DATE 0x04
#define DS3231_REG_MONTH 0x05
#define DS3231_REG_YEAR 0x06

typedef struct {
    uint8_t seconds;
    uint8_t minutes;
    uint8_t hours;
    uint8_t day;
    uint8_t date;
    uint8_t month;
    uint8_t year;
} DS3231_DateTime;

/* Convert BCD-encoded RTC values into normal binary values. */
static inline uint8_t DS3231_BcdToBin(uint8_t value)
{
    return (uint8_t)((value & 0x0F) + ((value >> 4) * 10U));
}

/* Convert normal values back to the BCD format required by the DS3231 registers. */
static inline uint8_t DS3231_BinToBcd(uint8_t value)
{
    return (uint8_t)(((value / 10U) << 4) | (value % 10U));
}

static inline void DS3231_I2C_Delay(void)
{
    Delay_Us(DS3231_I2C_HALF_PERIOD_US);
}

static inline void DS3231_I2C_Sda(uint8_t level)
{
    GPIO_WriteBit(DS3231_I2C_PORT, DS3231_SDA_PIN, level ? Bit_SET : Bit_RESET);
}

static inline void DS3231_I2C_Scl(uint8_t level)
{
    GPIO_WriteBit(DS3231_I2C_PORT, DS3231_SCL_PIN, level ? Bit_SET : Bit_RESET);
}

static inline uint8_t DS3231_I2C_SdaRead(void)
{
    return GPIO_ReadInputDataBit(DS3231_I2C_PORT, DS3231_SDA_PIN) != 0 ? 1U : 0U;
}

static inline void DS3231_I2C_Start(void)
{
    DS3231_I2C_Sda(1U);
    DS3231_I2C_Scl(1U);
    DS3231_I2C_Delay();
    DS3231_I2C_Sda(0U);
    DS3231_I2C_Delay();
    DS3231_I2C_Scl(0U);
    DS3231_I2C_Delay();
}

static inline void DS3231_I2C_Stop(void)
{
    DS3231_I2C_Sda(0U);
    DS3231_I2C_Delay();
    DS3231_I2C_Scl(1U);
    DS3231_I2C_Delay();
    DS3231_I2C_Sda(1U);
    DS3231_I2C_Delay();
}

/* Clock out one byte. Returns 1 if the slave ACKed. */
static inline uint8_t DS3231_I2C_WriteByte(uint8_t data)
{
    uint8_t ack;

    for (uint8_t i = 0U; i < 8U; i++) {
        DS3231_I2C_Sda((data & 0x80U) != 0U);
        data = (uint8_t)(data << 1);
        DS3231_I2C_Delay();
        DS3231_I2C_Scl(1U);
        DS3231_I2C_Delay();
        DS3231_I2C_Scl(0U);
    }

    DS3231_I2C_Sda(1U);
    DS3231_I2C_Delay();
    DS3231_I2C_Scl(1U);
    DS3231_I2C_Delay();
    ack = DS3231_I2C_SdaRead() ? 0U : 1U;
    DS3231_I2C_Scl(0U);
    DS3231_I2C_Delay();

    return ack;
}

/* Clock in one byte, then send ACK (more bytes follow) or NACK (last byte). */
static inline uint8_t DS3231_I2C_ReadByte(uint8_t ack)
{
    uint8_t data = 0U;

    DS3231_I2C_Sda(1U);
    for (uint8_t i = 0U; i < 8U; i++) {
        DS3231_I2C_Delay();
        DS3231_I2C_Scl(1U);
        DS3231_I2C_Delay();
        data = (uint8_t)((data << 1) | DS3231_I2C_SdaRead());
        DS3231_I2C_Scl(0U);
    }

    DS3231_I2C_Sda(ack ? 0U : 1U);
    DS3231_I2C_Delay();
    DS3231_I2C_Scl(1U);
    DS3231_I2C_Delay();
    DS3231_I2C_Scl(0U);
    DS3231_I2C_Sda(1U);
    DS3231_I2C_Delay();

    return data;
}

/* Initialize the GPIO lines used by the RTC connection. */
static inline void DS3231_I2C_Init(void)
{
    //Delay_Init(); // rjv
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOC, ENABLE);

    DS3231_I2C_Sda(1U);
    DS3231_I2C_Scl(1U);

    GPIO_InitTypeDef gpio = {0};
    gpio.GPIO_Pin = DS3231_SCL_PIN | DS3231_SDA_PIN;
    gpio.GPIO_Mode = GPIO_Mode_Out_OD;
    gpio.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(DS3231_I2C_PORT, &gpio);
}

/* Check whether the DS3231 is present on the bus by polling for an ACK. */
static inline uint8_t DS3231_IsConnected(void)
{
    uint8_t ok;

    DS3231_I2C_Start();
    ok = DS3231_I2C_WriteByte((uint8_t)(DS3231_I2C_ADDRESS << 1));
    DS3231_I2C_Stop();

    return ok;
}

/* Write a single byte to a DS3231 register. If the RTC is missing, the call exits safely. */
static inline void DS3231_WriteReg(uint8_t reg, uint8_t value)
{
    DS3231_I2C_Start();
    (void)(DS3231_I2C_WriteByte((uint8_t)(DS3231_I2C_ADDRESS << 1)) &&
           DS3231_I2C_WriteByte(reg) &&
           DS3231_I2C_WriteByte(value));
    DS3231_I2C_Stop();
}

/* Read consecutive DS3231 registers. Returns 1 on success, 0 if the RTC does not respond. */
static inline uint8_t DS3231_ReadRegs(uint8_t reg, uint8_t *buf, uint8_t len)
{
    DS3231_I2C_Start();
    if (!DS3231_I2C_WriteByte((uint8_t)(DS3231_I2C_ADDRESS << 1)) || !DS3231_I2C_WriteByte(reg)) {
        DS3231_I2C_Stop();
        return 0U;
    }

    DS3231_I2C_Start();
    if (!DS3231_I2C_WriteByte((uint8_t)((DS3231_I2C_ADDRESS << 1) | 1U))) {
        DS3231_I2C_Stop();
        return 0U;
    }

    for (uint8_t i = 0U; i < len; i++) {
        buf[i] = DS3231_I2C_ReadByte((uint8_t)(i + 1U < len));
    }
    DS3231_I2C_Stop();

    return 1U;
}

/* Read a single byte from a DS3231 register. Returns 0 if the RTC is unavailable. */
static inline uint8_t DS3231_ReadReg(uint8_t reg)
{
    uint8_t value = 0U;

    if (!DS3231_ReadRegs(reg, &value, 1U)) {
        return 0U;
    }

    return value;
}

/* Gather the full date/time structure from the DS3231 RTC. Fields are 0 if the RTC is unavailable. */
static inline DS3231_DateTime DS3231_ReadDateTime(void)
{
    DS3231_DateTime dt = {0};
    uint8_t raw[7];

    if (!DS3231_ReadRegs(DS3231_REG_SECONDS, raw, sizeof(raw))) {
        return dt;
    }

    dt.seconds = DS3231_BcdToBin(raw[DS3231_REG_SECONDS] & 0x7FU);
    dt.minutes = DS3231_BcdToBin(raw[DS3231_REG_MINUTES] & 0x7FU);
    dt.hours = DS3231_BcdToBin(raw[DS3231_REG_HOURS] & 0x3FU);
    dt.day = DS3231_BcdToBin(raw[DS3231_REG_DAY] & 0x07U);
    dt.date = DS3231_BcdToBin(raw[DS3231_REG_DATE] & 0x3FU);
    dt.month = DS3231_BcdToBin(raw[DS3231_REG_MONTH] & 0x1FU);
    dt.year = DS3231_BcdToBin(raw[DS3231_REG_YEAR]);

    return dt;
}

/* Write an entire date/time structure to the DS3231 RTC. */
static inline void DS3231_WriteDateTime(const DS3231_DateTime *dt)
{
    if (dt == NULL) {
        return;
    }

    DS3231_WriteReg(DS3231_REG_SECONDS, DS3231_BinToBcd(dt->seconds));
    DS3231_WriteReg(DS3231_REG_MINUTES, DS3231_BinToBcd(dt->minutes));
    DS3231_WriteReg(DS3231_REG_HOURS, DS3231_BinToBcd(dt->hours));
    DS3231_WriteReg(DS3231_REG_DAY, DS3231_BinToBcd(dt->day));
    DS3231_WriteReg(DS3231_REG_DATE, DS3231_BinToBcd(dt->date));
    DS3231_WriteReg(DS3231_REG_MONTH, DS3231_BinToBcd(dt->month));
    DS3231_WriteReg(DS3231_REG_YEAR, DS3231_BinToBcd(dt->year));
}

#endif
