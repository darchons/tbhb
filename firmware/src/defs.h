/*
 * defs.h
 *
 *  Created on: Mar 15, 2013
 *      Author: nchen
 */

#ifndef DEFS_H_
#define DEFS_H_

#define ALWAYS_INLINE   __attribute__((always_inline))

// Format: 0xGGRR
typedef uint16_t    pixel_t;
// Format: 0xGGGGRRRR
typedef uint32_t    gamma_pixel_t;

// Format: 0bXXXXXXXXXXXXXXXX
//  where each X corresponds to a driver channel
typedef uint16_t    line_t;

#define BITS        9
#define CHANNELS    2   // red and green
#define WIDTH       8
#define LINES       8
#define BUFFERS     3
#define SRC_BUFFERS 2   // Lines of source to buffer

ALWAYS_INLINE
static gamma_pixel_t
CorrectGamma(pixel_t pixel) {
    uintptr_t red = (((uintptr_t)pixel) & 0xff) + 1;
    uintptr_t green = (((uintptr_t)pixel) & 0xff00) + 0x100;
    return ((((uintptr_t)(red * red) - 1) >> (16 - BITS)) |
            (((uintptr_t)(green * green) - 0x10000) >> (16 - BITS))) &
            (((1 << BITS) - 1) | ((0x10000 << BITS) - 1));
}

// Size of a timer program for one line
#define PROGRAM_SIZE    (((BITS + 1) * BITS) + 1)

// Size of program to generate per received pixel, excluding first program
//  program entry and final received pixel in a line (see SetFrameData)
#define PROGRAM_STEP    (((PROGRAM_SIZE - 1) + (WIDTH - 2)) / (WIDTH - 1))

#if (BUFFERS & (BUFFERS - 1)) == 0
#define ROUND_BUFFER_INDEX(i)   ((i) & (BUFFERS - 1))
#else
#define ROUND_BUFFER_INDEX(i)   ((i) >= BUFFERS ? (i) - BUFFERS : (i))
#endif

static const intptr_t LINE_SEQUENCE[LINES + 1] = {4, 6, 3, 1, 0, 2, 7, 5, 4};

#define HOST_SPI_CLK    4000000
#define DRIVER_SPI_CLK  12000000
#define DRIVER_LINE_CLK 600000

#define NVIC_PRIO_DRIVER_TIMER  0
#define NVIC_PRIO_HOST_SSP      3

#define MAKE_PIO_(prefix, port, pin)    prefix ## PIO ## port ## _ ## pin
#define MAKE_PIO(prefix, port, pin)     MAKE_PIO_(prefix, port, pin)

// /EN      pin 14 (PIO1_24/CT32B0_MAT0)
#define SPI_EN_PORT 1
#define SPI_EN_PIN  24
#define SPI_EN_PIO  MAKE_PIO(, SPI_EN_PORT, SPI_EN_PIN)
// MOSI     pin 12 (PIO0_21/CT16B1_MAT0/MOSI1)
#define MOSI_PORT   0
#define MOSI_PIN    21
#define MOSI_PIO    MAKE_PIO(, MOSI_PORT, MOSI_PIN)
// SCLK     pin 28 (PIO1_15/DCD/CT16B0_MAT2/SCK1)
#define SCLK_PORT   1
#define SCLK_PIN    15
#define SCLK_PIO    MAKE_PIO(, SCLK_PORT, SCLK_PIN)
// /CS      pin 13 (PIO1_23/CT16B1_MAT1/SSEL1)
#define CS_PORT     1
#define CS_PIN      23
#define CS_PIO      MAKE_PIO(, CS_PORT, CS_PIN)

// MOSI     pin 18 (PIO0_9/MOSI0/CT16B0_MAT1)
#define LED_SIN_PORT    0
#define LED_SIN_PIN     9
#define LED_SIN_PIO     MAKE_PIO(, LED_SIN_PORT, LED_SIN_PIN)
// SCLK     pin 15 (PIO0_6/SCK0)
#define LED_SCLK_PORT   0
#define LED_SCLK_PIN    6
#define LED_SCLK_PIO    MAKE_PIO(, LED_SCLK_PORT, LED_SCLK_PIN)
// /CS      pin 8 (PIO0_2/SSEL0/CT16B0_CAP0)
#define LED_LATCH_PORT  0
#define LED_LATCH_PIN   2
#define LED_LATCH_PIO   MAKE_PIO(, LED_LATCH_PORT, LED_LATCH_PIN)

// VREF0    pin 21 (TDI/PIO0_11/AD0/CT32B0_MAT3)
#define VREF0_PORT  0
#define VREF0_PIN   11
#define VREF0_PIO   MAKE_PIO(TDI_, VREF0_PORT, VREF0_PIN)
// VREF1    pin 22 (TMS/PIO0_12/AD1/CT32B1_CAP0)
#define VREF1_PORT  0
#define VREF1_PIN   12
#define VREF1_PIO   MAKE_PIO(TMS_, VREF1_PORT, VREF1_PIN)
// VREF2    pin 23 (TDO/PIO0_13/AD2/CT32B1_MAT0)
#define VREF2_PORT  0
#define VREF2_PIN   13
#define VREF2_PIO   MAKE_PIO(TDO_, VREF2_PORT, VREF2_PIN)
// VREF3    pin 24 (TRST/PIO0_14/AD3/CT32B1_MAT1)
#define VREF3_PORT  0
#define VREF3_PIN   14
#define VREF3_PIO   MAKE_PIO(TRST_, VREF3_PORT, VREF3_PIN)
// VREF4    pin 26 (PIO0_16/AD5/CT32B1_MAT3/WAKEUP)
#define VREF4_PORT  0
#define VREF4_PIN   16
#define VREF4_PIO   MAKE_PIO(, VREF4_PORT, VREF4_PIN)
// VREF5    pin 30 (PIO0_17/RTS/CT32B0_CAP0/SCLK)
#define VREF5_PORT  0
#define VREF5_PIN   17
#define VREF5_PIO   MAKE_PIO(, VREF5_PORT, VREF5_PIN)
// VREF6    pin 31 (PIO0_18/RXD/CT32B0_MAT0)
#define VREF6_PORT  0
#define VREF6_PIN   18
#define VREF6_PIO   MAKE_PIO(, VREF6_PORT, VREF6_PIN)

#define VREF_SIZE   7

static const uintptr_t IREF_DIR[] = { 0b11111111, 0b10001111, 0b10000111,
        0b10000011, 0b11111111, 0b10000001, 0b11011111, 0b10000000, 0b11001111,
        0b11000111, 0b11000011, 0b11000001, 0b11111111, 0b11000000, 0b11101111,
        0b11100111, 0b11100011, 0b11100001, 0b11100000, 0b11111111, 0b11110111,
        0b11110011, 0b11110001, 0b11110000, 0b11111111, 0b11111011, 0b11111001,
        0b11111000, 0b11111111, 0b11111101, 0b11111100, 0b11111111 };
static const uintptr_t IREF_OUT[] = { 0b10000000, 0b10000000, 0b10000000,
        0b10000000, 0b11000000, 0b10000000, 0b11000000, 0b10000000, 0b11000000,
        0b11000000, 0b11000000, 0b11000000, 0b11100000, 0b11000000, 0b11100000,
        0b11100000, 0b11100000, 0b11100000, 0b11100000, 0b11110000, 0b11110000,
        0b11110000, 0b11110000, 0b11110000, 0b11111000, 0b11111000, 0b11111000,
        0b11111000, 0b11111100, 0b11111100, 0b11111100, 0b11111110 };

// Dummy arrays to detect mismatch in size between IREF_DIR and IREF_OUT
static const uintptr_t IREF_DUMMY1[sizeof(IREF_DIR) - sizeof(IREF_OUT)];
static const uintptr_t IREF_DUMMY2[sizeof(IREF_OUT) - sizeof(IREF_DIR)];

#define IREF_LEVELS (sizeof(IREF_DIR) / sizeof(IREF_DIR[0]))

// CSEL0    pin 20 (PIO0_22/AD6/CT16B1_MAT1/MISO1)
#define CSEL0_PORT  0
#define CSEL0_PIN   22
#define CSEL0_PIO   MAKE_PIO(, CSEL0_PORT, CSEL0_PIN)
// CSEL1    pin 17 (PIO0_8/MISO0/CT16B0_MAT0)
#define CSEL1_PORT  0
#define CSEL1_PIN   8
#define CSEL1_PIO   MAKE_PIO(, CSEL1_PORT, CSEL1_PIN)
// CSEL2    pin 16 (PIO0_7/CTS)
#define CSEL2_PORT  0
#define CSEL2_PIN   7
#define CSEL2_PIO   MAKE_PIO(, CSEL2_PORT, CSEL2_PIN)

// BLANK    pin 1 (PIO1_19/DTR/SSEL1)
#define BLANK_PORT  1
#define BLANK_PIN   19
#define BLANK_PIO   MAKE_PIO(, BLANK_PORT, BLANK_PIN)

#endif /* DEFS_H_ */
