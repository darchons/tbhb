/*
 ===============================================================================
 Name        : main.c
 Author      :
 Version     :
 Copyright   : Copyright (C)
 Description : main definition
 ===============================================================================
 */

//#define DEMO

#ifdef __USE_CMSIS
#include "LPC11Exx.h"
#endif

#include <stdbool.h>
#ifdef DEMO
#include <stdlib.h>
#endif
#include <cr_section_macros.h>
#include <NXP/crp.h>


// Variable to store CRP value in. Will be placed automatically
// by the linker when "Enable Code Read Protect" selected.
// See crp.h header for more information
__CRP const unsigned int CRP_WORD = CRP_NO_CRP;

#include "defs.h"
#include "conf.h"
#include "host.h"

static line_t g_buffers[BUFFERS][LINES][PROGRAM_SIZE];
static line_t (*g_frame)[LINES][PROGRAM_SIZE];
static line_t (*g_stage)[LINES][PROGRAM_SIZE];
static line_t *g_frame_line;
static line_t *g_stage_line;

static volatile bool g_switch_buffer;
static volatile size_t g_frame_index;
static size_t g_stage_index;

static uintptr_t g_program_interval[PROGRAM_SIZE];
static uintptr_t g_program_pos[PROGRAM_SIZE - 1];
static uintptr_t g_program_bit[PROGRAM_SIZE - 1];
static uintptr_t *g_frame_interval;

static uintptr_t g_program_csel[LINES];
static uintptr_t *g_frame_csel;

#define COMMAND_LENGTH_VARIABLE    UINTPTR_MAX

static enum HOST_COMMAND g_command;
static uintptr_t g_command_length;
static uintptr_t g_command_id;

static gamma_pixel_t g_src_buffers[SRC_BUFFERS][WIDTH];
static gamma_pixel_t (*g_src_program_line)[WIDTH];
static gamma_pixel_t *g_src_incoming_pixel;
static uintptr_t g_src_program_index;

static void
InitFrame(void)
{
    g_frame_index = 0;
    g_frame = &g_buffers[g_frame_index];
    g_frame_line = (*g_frame)[LINES];
    g_stage_index = 1;
    g_stage = &g_buffers[g_stage_index];
    g_stage_line = (*g_stage)[LINES];
    g_switch_buffer = false;
}

static void
InitSource(void)
{
    g_src_program_line = &g_src_buffers[SRC_BUFFERS - 1];
    g_src_incoming_pixel = *g_src_program_line;
    g_src_program_index = 0;
}

static void
InitProgram(void)
{
    intptr_t i, j;
    size_t program_index = 0;
    for (i = 1; i < BITS; ++i) {
        for (j = i - 1; j >= 0; --j) {
            g_program_interval[PROGRAM_SIZE - program_index - 2] =
                    ((j == 0) ? 1 : (1 << (j - 1))) * 2 - 1;
            g_program_pos[program_index] = CHANNELS * WIDTH - j - 1;
            g_program_bit[program_index] = (1 << i);
            program_index++;
        }
    }
    for (i = BITS - 1; i > 0; --i) {
        for (j = 0; j < i; ++j) {
            g_program_interval[PROGRAM_SIZE - program_index - 2] =
                    ((j + 1 == i) ? 1 : (1 << j)) * 2 - 1;
            g_program_pos[program_index] = CHANNELS * WIDTH - i - 1;
            g_program_bit[program_index] = (1 << j);
            program_index++;
        }
    }
    g_program_interval[PROGRAM_SIZE - 1] = 1;
    g_frame_interval = &g_program_interval[PROGRAM_SIZE];

    for (i = 0; i < LINES; ++i) {
#if LINES != 8
#error Change sequence below
#endif
        intptr_t xor = LINE_SEQUENCE[i] ^ LINE_SEQUENCE[i + 1];
#define MAKE_CSEL(n)    ((!!(xor & (1 << (n)))) << (CSEL ## n ## _PIN))
        g_program_csel[i] = MAKE_CSEL(0) | MAKE_CSEL(1) | MAKE_CSEL(2);
#undef MAKE_CSEL
    }
    for (i = 1; i < LINES; ++i) {
        if (LINE_SEQUENCE[i] == 0) {
            break;
        }
    }
    g_frame_csel = &g_program_csel[i];
}

static void
InitHostSPI(void)
{
    // Enable clocks for blocks used below
    LPC_SYSCON->SYSAHBCLKCTRL |= SYSAHBCLKCTRL_IOCON |
            SYSAHBCLKCTRL_GPIO | SYSAHBCLKCTRL_SSP1;

    setGPIO(SPI_EN_PORT, SPI_EN_PIN, GPIO_HI);
    setGPIODir(SPI_EN_PORT, SPI_EN_PIN, GPIO_OUTPUT);
    LPC_IOCON->SPI_EN_PIO = IOCon_Digital(IOCON_FUNC_0, IOCON_MODE_INACTIVE,
            IOCON_HYS_DISABLED, IOCON_INV_NORMAL, IOCON_OD_DISABLED);

    // Set SSP1 clock to run as fast as we can
    LPC_SYSCON->SSP1CLKDIV = 1;
    // De-assert SSP1 reset signal
    LPC_SYSCON->PRESETCTRL |= PRESETCTRL_SSP1_RST_N;

    // Set SSP1 configuration and start SSP1
    while (LPC_SSP1->SR & SSP_SR_RNE) {
        (void) LPC_SSP1->DR; // Clear out receive FIFO
    }
    LPC_SSP1->IMSC = SSP_IMSC_RTIM | SSP_IMSC_RXIM;
    LPC_SSP1->CR0 = SSP_CR0(16, SSP_FRF_SPI,
            SSP_CPOL_LO, SSP_CPHA_BACK_TO, 0);
    LPC_SSP1->CR1 = SSP_CR1(SSP_LBM_NORMAL, SSP_SSE_ENABLED,
            SSP_MS_SLAVE, SSP_SOD_OUTPUT_DISABLED);

    // MOSI
    LPC_IOCON->MOSI_PIO = IOCon_Digital(IOCON_FUNC_2, IOCON_MODE_INACTIVE,
            IOCON_HYS_DISABLED, IOCON_INV_NORMAL, IOCON_OD_DISABLED);
    // SCLK
    LPC_IOCON->SCLK_PIO = IOCon_Digital(IOCON_FUNC_3, IOCON_MODE_INACTIVE,
            IOCON_HYS_DISABLED, IOCON_INV_NORMAL, IOCON_OD_DISABLED);
    // /CS
    LPC_IOCON->CS_PIO = IOCon_Digital(IOCON_FUNC_2, IOCON_MODE_INACTIVE,
            IOCON_HYS_DISABLED, IOCON_INV_NORMAL, IOCON_OD_DISABLED);

    NVIC_SetPriority(SSP1_IRQn, NVIC_PRIO_HOST_SSP);
    NVIC_ClearPendingIRQ(SSP1_IRQn);
    NVIC_EnableIRQ(SSP1_IRQn);
}

static void
InitHostCommand(void) {
    g_command = HOST_NOP;
    g_command_id = 0;
}

static void
InitDriverSPI(void)
{
    // Enable clocks for blocks used below
    LPC_SYSCON->SYSAHBCLKCTRL |= SYSAHBCLKCTRL_IOCON |
            SYSAHBCLKCTRL_SSP0;

    // Set SSP0 clock
    LPC_SYSCON->SSP0CLKDIV = 1;
    // De-assert SSP0 reset signal
    LPC_SYSCON->PRESETCTRL |= PRESETCTRL_SSP0_RST_N;

    // Set SSP0 configuration and start SSP0
    LPC_SSP0->CPSR = SystemCoreClock / DRIVER_SPI_CLK;
    LPC_SSP0->CR0 = SSP_CR0(
            16, SSP_FRF_SPI, SSP_CPOL_LO, SSP_CPHA_AWAY_FROM, 0);
    LPC_SSP0->CR1 = SSP_CR1(
            SSP_LBM_NORMAL, SSP_SSE_ENABLED, SSP_MS_MASTER,
            SSP_SOD_NORMAL);

    // LED_SIN      pin 18 (PIO0_9/MOSI0/CT16B0_MAT1)
    LPC_IOCON->LED_SIN_PIO = IOCon_Digital(IOCON_FUNC_1, IOCON_MODE_INACTIVE,
            IOCON_HYS_DISABLED, IOCON_INV_NORMAL, IOCON_OD_DISABLED);
    // LED_SCLK     pin 15 (PIO0_6/SCK0)
    LPC_IOCON->LED_SCLK_PIO = IOCon_Digital(IOCON_FUNC_2, IOCON_MODE_INACTIVE,
            IOCON_HYS_DISABLED, IOCON_INV_NORMAL, IOCON_OD_DISABLED);
    // LED_LATCH    pin 8 (PIO0_2/SSEL0/CT16B0_CAP0)
    LPC_IOCON->LED_LATCH_PIO = IOCon_Digital(IOCON_FUNC_1, IOCON_MODE_INACTIVE,
            IOCON_HYS_DISABLED, IOCON_INV_NORMAL, IOCON_OD_DISABLED);
}

static void
InitDriverTimer(void)
{
    // Enable clocks for blocks used below
    LPC_SYSCON->SYSAHBCLKCTRL |= SYSAHBCLKCTRL_CT32B0;

    LPC_CT32B0->IR = CT32B0_IR_MR0INT | CT32B0_IR_MR1INT | CT32B0_IR_MR2INT |
            CT32B0_IR_MR3INT | CT32B0_IR_CR0INT | CT32B0_IR_CR1INT;
    NVIC_SetPriority(TIMER_32_0_IRQn, NVIC_PRIO_DRIVER_TIMER);
    NVIC_ClearPendingIRQ(TIMER_32_0_IRQn);
    NVIC_EnableIRQ(TIMER_32_0_IRQn);

    LPC_CT32B0->MCR = CT32B0_MCR_MR0I | CT32B0_MCR_MR0R | CT32B0_MCR_MR1R;
    LPC_CT32B0->MR0 = 1;
    LPC_CT32B0->MR1 = (1 << (BITS - 1)) * 2; // safeguard
    LPC_CT32B0->PC = 0;
    // Set timer to run at DRIVER_LINE_CLK
    LPC_CT32B0->PR = SystemCoreClock / DRIVER_LINE_CLK / 2 - 1;
    LPC_CT32B0->TC = 0;
    LPC_CT32B0->TCR = CT32B0_TCR(CT32B0_CEN_ENABLED, CT32B0_CRST_RESET);
    LPC_CT32B0->TCR = CT32B0_TCR(CT32B0_CEN_ENABLED, CT32B0_CRST_NORMAL);
}

static void
InitDriverSignals(void)
{
    // Enable clocks for blocks used below
    LPC_SYSCON->SYSAHBCLKCTRL |= SYSAHBCLKCTRL_IOCON;

    // VREF0
    setGPIO(VREF0_PORT, VREF0_PIN, GPIO_LO);
    setGPIODir(VREF0_PORT, VREF0_PIN, GPIO_OUTPUT);
    LPC_IOCON->VREF0_PIO = IOCon_Digital(IOCON_FUNC_1, IOCON_MODE_INACTIVE,
            IOCON_HYS_DISABLED, IOCON_INV_NORMAL, IOCON_OD_DISABLED);
    // VREF1
    setGPIO(VREF1_PORT, VREF1_PIN, GPIO_LO);
    setGPIODir(VREF1_PORT, VREF1_PIN, GPIO_OUTPUT);
    LPC_IOCON->VREF1_PIO = IOCon_Digital(IOCON_FUNC_1, IOCON_MODE_INACTIVE,
            IOCON_HYS_DISABLED, IOCON_INV_NORMAL, IOCON_OD_DISABLED);
    // VREF2
    setGPIO(VREF2_PORT, VREF2_PIN, GPIO_LO);
    setGPIODir(VREF2_PORT, VREF2_PIN, GPIO_OUTPUT);
    LPC_IOCON->VREF2_PIO = IOCon_Digital(IOCON_FUNC_1, IOCON_MODE_INACTIVE,
            IOCON_HYS_DISABLED, IOCON_INV_NORMAL, IOCON_OD_DISABLED);
    // VREF3
    setGPIO(VREF3_PORT, VREF3_PIN, GPIO_LO);
    setGPIODir(VREF3_PORT, VREF3_PIN, GPIO_OUTPUT);
    LPC_IOCON->VREF3_PIO = IOCon_Digital(IOCON_FUNC_1, IOCON_MODE_INACTIVE,
            IOCON_HYS_DISABLED, IOCON_INV_NORMAL, IOCON_OD_DISABLED);
    // VREF4
    setGPIO(VREF4_PORT, VREF4_PIN, GPIO_LO);
    setGPIODir(VREF4_PORT, VREF4_PIN, GPIO_INPUT);
    LPC_IOCON->VREF4_PIO = IOCon_Digital(IOCON_FUNC_0, IOCON_MODE_INACTIVE,
            IOCON_HYS_DISABLED, IOCON_INV_NORMAL, IOCON_OD_DISABLED);
    // VREF5
    setGPIO(VREF5_PORT, VREF5_PIN, GPIO_LO);
    setGPIODir(VREF5_PORT, VREF5_PIN, GPIO_INPUT);
    LPC_IOCON->VREF5_PIO = IOCon_Digital(IOCON_FUNC_0, IOCON_MODE_INACTIVE,
            IOCON_HYS_DISABLED, IOCON_INV_NORMAL, IOCON_OD_DISABLED);
    // VREF6
    setGPIO(VREF6_PORT, VREF6_PIN, GPIO_LO);
    setGPIODir(VREF6_PORT, VREF6_PIN, GPIO_INPUT);
    LPC_IOCON->VREF6_PIO = IOCon_Digital(IOCON_FUNC_0, IOCON_MODE_INACTIVE,
            IOCON_HYS_DISABLED, IOCON_INV_NORMAL, IOCON_OD_DISABLED);

    // CSEL0
    setGPIO(CSEL0_PORT, CSEL0_PIN, GPIO_LO);
    setGPIODir(CSEL0_PORT, CSEL0_PIN, GPIO_OUTPUT);
    LPC_IOCON->CSEL0_PIO = IOCon_Digital(IOCON_FUNC_0, IOCON_MODE_INACTIVE,
            IOCON_HYS_DISABLED, IOCON_INV_NORMAL, IOCON_OD_DISABLED);
    // CSEL1
    setGPIO(CSEL1_PORT, CSEL1_PIN, GPIO_LO);
    setGPIODir(CSEL1_PORT, CSEL1_PIN, GPIO_OUTPUT);
    LPC_IOCON->CSEL1_PIO = IOCon_Digital(IOCON_FUNC_0, IOCON_MODE_INACTIVE,
            IOCON_HYS_DISABLED, IOCON_INV_NORMAL, IOCON_OD_DISABLED);
    // CSEL2
    setGPIO(CSEL2_PORT, CSEL2_PIN, GPIO_LO);
    setGPIODir(CSEL2_PORT, CSEL2_PIN, GPIO_OUTPUT);
    LPC_IOCON->CSEL2_PIO = IOCon_Digital(IOCON_FUNC_0, IOCON_MODE_INACTIVE,
            IOCON_HYS_DISABLED, IOCON_INV_NORMAL, IOCON_OD_DISABLED);

    // BLANK
    setGPIO(BLANK_PORT, BLANK_PIN, GPIO_HI);
    setGPIODir(BLANK_PORT, BLANK_PIN, GPIO_OUTPUT);
    LPC_IOCON->BLANK_PIO = IOCon_Digital(IOCON_FUNC_0, IOCON_MODE_INACTIVE,
            IOCON_HYS_DISABLED, IOCON_INV_NORMAL, IOCON_OD_DISABLED);
}

static void
SetDriverIRef(uintptr_t level)
{
    uintptr_t dir, out;
    if (level >= IREF_LEVELS) {
        level = IREF_LEVELS - 1;
    }
    dir = IREF_DIR[(size_t)level];
    out = IREF_OUT[(size_t)level];
#define SET_IREF(n) \
    do { \
        setGPIO(VREF ## n ## _PORT, VREF ## n ## _PIN, \
                !!(out & (1 << n))); \
        setGPIODir(VREF ## n ## _PORT, VREF ## n ## _PIN, \
                !!(dir & (1 << n))); \
    } while (false)
    SET_IREF(0);
    SET_IREF(1);
    SET_IREF(2);
    SET_IREF(3);
    SET_IREF(4);
    SET_IREF(5);
    SET_IREF(6);
#if VREF_SIZE != 7
#error Adjust code above
#endif
#undef SET_IREF
}

static void
SetFrameData(uintptr_t data)
{
    intptr_t i;
    uintptr_t line, shift;
    gamma_pixel_t pixel;
    gamma_pixel_t (*src_line)[WIDTH] = g_src_program_line;

    *(g_src_incoming_pixel++) = ~CorrectGamma((pixel_t)data);
    if (g_src_incoming_pixel > *src_line &&
            g_src_incoming_pixel <= *(src_line + 1)) {
        // First line
        if (g_src_incoming_pixel != *(++src_line)) {
            return;
        }
    }
    if (g_src_incoming_pixel == *src_line ||
            g_src_incoming_pixel == g_src_buffers[SRC_BUFFERS]) {
        if (*src_line == g_src_buffers[0]) {
            src_line = &g_src_buffers[SRC_BUFFERS];
        }
        g_src_program_line = --src_line;
        g_src_incoming_pixel = *(src_line - 1);
        if (*src_line == g_src_buffers[0]) {
            g_src_incoming_pixel = g_src_buffers[SRC_BUFFERS - 1];
        }
        // Calculate the first entry of the program
        line = 0;
        shift = 0;
        for (i = CHANNELS * WIDTH - 1; i >= 0; --i) {
            pixel = (*src_line)[i / 2];
            pixel = (i & 1) ? (pixel >> 16) : pixel;
            line |= ((!(pixel & (1 << shift))) << i);
            shift = (shift == (BITS - 1)) ? 0 : (shift + 1);
        }
        g_src_program_index = 0;
        *(--g_stage_line) = (line_t)line;
    } else {
        line = (uintptr_t)*g_stage_line;
        i = g_src_program_index;
        shift = PROGRAM_SIZE - i - 1;
        shift = shift > PROGRAM_STEP ? PROGRAM_STEP : shift;
        for (; shift; --shift, ++i) {
            intptr_t pos = g_program_pos[i];
            intptr_t bit = g_program_bit[i];
            for (; pos >= 0; pos -= BITS) {
                pixel = (*src_line)[pos / 2];
                pixel = (pos & 1) ? (pixel >> 16) : pixel;
                line = (line & ~(1 << pos)) | ((!(pixel & bit)) << pos);
            }
            *(--g_stage_line) = (line_t)line;
        }
        g_src_program_index = i;
    }
    if (g_stage_line == (*g_stage)[0]) {
        g_stage_line = (*g_stage)[LINES];
    }
}

static void
FillFrame(uintptr_t data)
{
    intptr_t i, j;
    for (j = LINES; j > 0; --j) {
        for (i = WIDTH; i > 0; --i) {
            SetFrameData(data);
        }
    }
}

static void
NextFrame(void)
{
    intptr_t i;
    for (i = WIDTH - 1; i > 0; --i) {
        SetFrameData(0); // Digest last line received
    }
    g_switch_buffer = true;
    g_stage_index = ROUND_BUFFER_INDEX(g_stage_index + 1);
    g_stage = &g_buffers[g_stage_index];
    g_stage_line = (*g_stage)[LINES];
    InitSource();
    while (g_stage_index == g_frame_index) {
        __WFI();
    }
}

void
TIMER32_0_IRQHandler(void)
{
    LPC_SSP0->DR = (uint32_t)(*(--g_frame_line));
    LPC_CT32B0->IR = CT32B0_IR_MR0INT;
    LPC_CT32B0->TC = (uintptr_t)(-1);
    LPC_CT32B0->MR0 = (uint32_t)(*(--g_frame_interval));

    if (g_frame_interval != g_program_interval) {
        return;
    }
    g_frame_interval = &g_program_interval[PROGRAM_SIZE];

#if !(CSEL0_PORT == CSEL1_PORT && CSEL1_PORT == CSEL2_PORT)
#error CSEL pins must be in same port
#endif
    LPC_GPIO->NOT[CSEL0_PORT] = (uint32_t)*(--g_frame_csel);
    if (g_frame_csel != g_program_csel) {
        return;
    }
    g_frame_csel = &g_program_csel[LINES];

    if (g_switch_buffer) {
        size_t next_index;
        g_switch_buffer = false;
        next_index = ROUND_BUFFER_INDEX(g_frame_index + 1);
        g_frame_index = next_index;
        g_frame = &g_buffers[next_index];
    }
    g_frame_line = (*g_frame)[LINES];
}

void
SSP1_IRQHandler(void)
{
    uintptr_t data = (HOST_DATA)LPC_SSP1->DR;
    enum HOST_COMMAND cmd = g_command;
    uintptr_t length = g_command_length;

    if (!length) {
        g_command = cmd = (enum HOST_COMMAND) data;
        if ((cmd & HOST_COMMAND_LENGTH_MASK) == HOST_COMMAND_VARIABLE) {
            g_command_length = COMMAND_LENGTH_VARIABLE;
            return;
        } else {
            g_command_length = (((uintptr_t)cmd) >> HOST_COMMAND_LENGTH_SHIFT);
            if (g_command_length) {
                return;
            }
        }
    } else if (length == COMMAND_LENGTH_VARIABLE) {
        g_command_length = (uintptr_t) data;
        return;
    } else {
        g_command_length = length -= 1;
    }
    if ((cmd & HOST_ID_MASK) && g_command_id &&
            (cmd & HOST_ID_MASK) != g_command_id) {
        return;
    }
    switch (cmd) {
    case HOST_NOP:
        break;
    case HOST_ID:
        g_command_id = cmd & HOST_ID_MASK;
        setGPIO(SPI_EN_PORT, SPI_EN_PIN, !g_command_id);
        break;
    case HOST_FLIP:
        NextFrame();
        break;
    case HOST_BLANK:
        setGPIO(BLANK_PORT, BLANK_PIN, !!data);
        break;
    case HOST_IREF:
        SetDriverIRef(data);
        break;
    case HOST_FILL:
        FillFrame(data);
        break;
    case HOST_FRAME:
        SetFrameData(data);
        break;
    }
}

int
main(void)
{
#ifdef DEMO
    uintptr_t data = 0xa5a5;
#endif

    __disable_irq();
    SCB->SCR |= SCB_SCR_SLEEPONEXIT_Msk;
    InitFrame();
    InitSource();
    InitProgram();
    InitHostSPI();
    InitHostCommand();
    InitDriverSPI();
    InitDriverTimer();
    InitDriverSignals();

#ifndef DEMO
    __enable_irq();
#else
    setGPIO(BLANK_PORT, BLANK_PIN, GPIO_LO);
    g_frame_csel = &g_program_csel[LINES];
#endif

    while (true) {
#ifndef DEMO
        __WFI();
#else
        uintptr_t i;
#define DELAY do for (i = SystemCoreClock / 10000; i; --i) { __NOP(); } while(0)
        DELAY;
        LPC_SSP0->DR = data; //rand() & 0x0000FFFF;
        LPC_GPIO->NOT[CSEL0_PORT] = (uint32_t)*(--g_frame_csel);
        if (g_frame_csel == g_program_csel) {
            g_frame_csel = &g_program_csel[LINES];
        }
        if (LPC_SSP1->MIS & (SSP_IMSC_RTIM | SSP_IMSC_RXIM)) {
            data = LPC_SSP1->DR;
        }
#undef DELAY
#endif
    }
    return 0;
}
