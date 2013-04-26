/*
 * conf.h
 *
 *  Created on: Mar 15, 2013
 *      Author: nchen
 */

#ifndef CONF_H_
#define CONF_H_

enum IOCON_FUNC {
    IOCON_FUNC_0 = 0,
    IOCON_FUNC_1 = 1,
    IOCON_FUNC_2 = 2,
    IOCON_FUNC_3 = 3
};

enum IOCON_MODE {
    IOCON_MODE_INACTIVE     = (0 << 3),
    IOCON_MODE_PULL_DOWN    = (1 << 3),
    IOCON_MODE_PULL_UP      = (2 << 3),
    IOCON_MODE_REPEATER     = (3 << 3)
};

enum IOCON_HYS {
    IOCON_HYS_DISABLED  = (0 << 5),
    IOCON_HYS_ENABLED   = (1 << 5)
};

enum IOCON_INV {
    IOCON_INV_NORMAL    = (0 << 6),
    IOCON_INV_INVERTED  = (1 << 6)
};

enum IOCON_ADMODE {
    IOCON_ADMODE_ANALOG     = (0 << 7),
    IOCON_ADMODE_DIGITAL    = (1 << 7)
};

enum IOCON_FILTR {
    IOCON_FILTR_ENABLED     = (0 << 8),
    IOCON_FILTR_DISABLED    = (1 << 8)
};

enum IOCON_OD {
    IOCON_OD_DISABLED   = (0 << 10),
    IOCON_OD_OPEN_DRAIN = (1 << 10)
};

ALWAYS_INLINE static uint32_t
IOCon_Digital(enum IOCON_FUNC func, enum IOCON_MODE mode,
                 enum IOCON_HYS hys, enum IOCON_INV inv, enum IOCON_OD od) {
    return func | mode | hys | inv | od | IOCON_ADMODE_DIGITAL;
}

enum GPIO_DIR {
    GPIO_INPUT  = 0,
    GPIO_OUTPUT = 1
};

enum GPIO_STATE {
    GPIO_LO = 0,
    GPIO_HI = 1
};

ALWAYS_INLINE static void
setGPIODir(intptr_t port, intptr_t pin, enum GPIO_DIR dir) {
    if (dir == GPIO_OUTPUT) {
        LPC_GPIO->DIR[port] |= (GPIO_OUTPUT << pin);
    } else {
        LPC_GPIO->DIR[port] &= ~(GPIO_OUTPUT << pin);
    }
}

ALWAYS_INLINE static enum GPIO_DIR
getGPIODir(intptr_t port, intptr_t pin) {
    return !!(LPC_GPIO->DIR[port] & (GPIO_OUTPUT << pin));
}

ALWAYS_INLINE static void
setGPIO(intptr_t port, intptr_t pin, enum GPIO_STATE state) {
    LPC_GPIO->W[port * sizeof(LPC_GPIO->W0) /
                sizeof(LPC_GPIO->W[0]) + pin] = state;
}

ALWAYS_INLINE static enum GPIO_STATE
getGPIO(intptr_t port, intptr_t pin) {
    return !!LPC_GPIO->W[port * sizeof(LPC_GPIO->W0) /
                         sizeof(LPC_GPIO->W[0]) + pin];
}

enum SYSAHBCLKCTRL {
    SYSAHBCLKCTRL_SYS = 1,
    SYSAHBCLKCTRL_ROM = (1 << 1),
    SYSAHBCLKCTRL_RAM0 = (1 << 2),
    SYSAHBCLKCTRL_FLASHREG = (1 << 3),
    SYSAHBCLKCTRL_FLASHARRAY = (1 << 4),
    SYSAHBCLKCTRL_I2C = (1 << 5),
    SYSAHBCLKCTRL_GPIO = (1 << 6),
    SYSAHBCLKCTRL_CT16B0 = (1 << 7),
    SYSAHBCLKCTRL_CT16B1 = (1 << 8),
    SYSAHBCLKCTRL_CT32B0 = (1 << 9),
    SYSAHBCLKCTRL_CT32B1 = (1 << 10),
    SYSAHBCLKCTRL_SSP0 = (1 << 11),
    SYSAHBCLKCTRL_USART = (1 << 12),
    SYSAHBCLKCTRL_ADC = (1 << 13),
    SYSAHBCLKCTRL_WWDT = (1 << 15),
    SYSAHBCLKCTRL_IOCON = (1 << 16),
    SYSAHBCLKCTRL_SSP1 = (1 << 18),
    SYSAHBCLKCTRL_PINT = (1 << 19),
    SYSAHBCLKCTRL_GROUP0INT = (1 << 23),
    SYSAHBCLKCTRL_GROUP1INT = (1 << 24),
    SYSAHBCLKCTRL_RAM1 = (1 << 25)
};

enum PRESETCTRL {
    PRESETCTRL_SSP0_RST_N = 1,
    PRESETCTRL_I2C_RST_N = (1 << 1),
    PRESETCTRL_SSP1_RST_N = (1 << 2)
};

enum SSP_FRF {
    SSP_FRF_SPI = (0 << 4),
    SSP_FRF_TI = (1 << 4),
    SSP_FRF_MICROWIRE = (2 << 4)
};

enum SSP_CPOL {
    SSP_CPOL_LO = (0 << 6),
    SSP_CPOL_HI = (1 << 6)
};

enum SSP_CPHA {
    SSP_CPHA_AWAY_FROM = (0 << 7),
    SSP_CPHA_BACK_TO = (1 << 7)
};

ALWAYS_INLINE static uint32_t
SSP_CR0(int bits, enum SSP_FRF frf, enum SSP_CPOL cpol,
           enum SSP_CPHA cpha, int scr) {
    return (bits - 1) | frf | cpol | cpha | (scr << 8);
}

enum SSP_LBM {
    SSP_LBM_NORMAL = 0,
    SSP_LBM_LOOP_BACK = 1
};

enum SSP_SSE {
    SSP_SSE_DISABLED = (0 << 1),
    SSP_SSE_ENABLED = (1 << 1)
};

enum SSP_MS {
    SSP_MS_MASTER = (0 << 2),
    SSP_MS_SLAVE = (1 << 2)
};

enum SSP_SOD {
    SSP_SOD_NORMAL = (0 << 3),
    SSP_SOD_OUTPUT_DISABLED = (1 << 3)
};

ALWAYS_INLINE static uint32_t
SSP_CR1(enum SSP_LBM lbm, enum SSP_SSE sse,
           enum SSP_MS ms, enum SSP_SOD sod) {
    return lbm | sse | ms | sod;
}

enum SSP_IMSC {
    SSP_IMSC_RORIM = 1,
    SSP_IMSC_RTIM = (1 << 1),
    SSP_IMSC_RXIM = (1 << 2),
    SSP_IMSC_TXIM = (1 << 3)
};

enum SSP_SR {
    SSP_SR_TFE = 1,
    SSP_SR_TNF = (1 << 1),
    SSP_SR_RNE = (1 << 2),
    SSP_SR_RFF = (1 << 3),
    SSP_SR_BSY = (1 << 4)
};

enum CT32B0_IR {
    CT32B0_IR_MR0INT = 1,
    CT32B0_IR_MR1INT = (1 << 1),
    CT32B0_IR_MR2INT = (1 << 2),
    CT32B0_IR_MR3INT = (1 << 3),
    CT32B0_IR_CR0INT = (1 << 4),
    CT32B0_IR_CR1INT = (1 << 6),
};

enum CT32B0_CEN {
    CT32B0_CEN_DISABLED = 0,
    CT32B0_CEN_ENABLED = 1
};

enum CT32B0_CRST {
    CT32B0_CRST_NORMAL = 0,
    CT32B0_CRST_RESET = (1 << 1)
};

ALWAYS_INLINE static uint32_t
CT32B0_TCR(enum CT32B0_CEN cen, enum CT32B0_CRST crst) {
    return cen | crst;
}

enum CT32B0_MCR {
    CT32B0_MCR_MR0I = 1,
    CT32B0_MCR_MR0R = (1 << 1),
    CT32B0_MCR_MR0S = (1 << 2),
    CT32B0_MCR_MR1I = (1 << 3),
    CT32B0_MCR_MR1R = (1 << 4),
    CT32B0_MCR_MR1S = (1 << 5),
    CT32B0_MCR_MR2I = (1 << 6),
    CT32B0_MCR_MR2R = (1 << 7),
    CT32B0_MCR_MR2S = (1 << 8),
    CT32B0_MCR_MR3I = (1 << 9),
    CT32B0_MCR_MR3R = (1 << 10),
    CT32B0_MCR_MR3S = (1 << 11),
};

#endif /* CONF_H_ */
