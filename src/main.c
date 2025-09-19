#include "stm32f103x6.h"  // Явное включение для STM32F103C6T6

// Pin definitions for STM32
#define VCC_PIN             GPIO_PIN_8
#define RST_PIN             GPIO_PIN_13  // Controls 12V via NPN transistor
#define SCI_PIN             GPIO_PIN_12
#define SDO_PIN             GPIO_PIN_11
#define SII_PIN             GPIO_PIN_10
#define SDI_PIN             GPIO_PIN_9

// Button pin for starting programming
#define BUTTON_PIN          GPIO_PIN_0  // Button to GND, internal pull-up
#define BUTTON_GPIO_PORT    GPIOA

// LED pin for indication (built-in on PC13 for Blue Pill-like boards)
#define LED_PIN             GPIO_PIN_13  // Active low
#define LED_GPIO_PORT       GPIOC

// Fuse addresses
#define HFUSE  0x747C
#define LFUSE  0x646C

// Expected fuse values after writing
#define EXPECTED_LFUSE      0x6A
#define EXPECTED_HFUSE      0xFF

// Function prototypes
void GPIO_Init(void);
uint8_t shiftOut(uint8_t val1, uint8_t val2);
void writeFuse(unsigned int fuse, uint8_t val);
uint8_t readLFuse(void);
uint8_t readHFuse(void);
void delayMicroseconds(uint32_t us);
void delay_ms(uint32_t ms);
void blinkLED(uint32_t times, uint32_t on_ms, uint32_t off_ms);

int main(void) {
    // SystemInit() called from startup_stm32f103xb.s, sets HSI 8MHz

    GPIO_Init();

    // Wait for button press (active low)
    while ((BUTTON_GPIO_PORT->IDR & BUTTON_PIN) != 0) {
        // Wait until button is pressed
    }

    // Debounce delay
    delay_ms(50);

    // Turn on LED to indicate programming start
    LED_GPIO_PORT->BRR = LED_PIN;  // LED on

    // Initialize pins to enter programming mode
    // Set SDO to output
    SDO_GPIO_PORT->CRH &= ~(GPIO_CRH_MODE11 | GPIO_CRH_CNF11);
    SDO_GPIO_PORT->CRH |= GPIO_CRH_MODE11_0;  // Output 10MHz

    // Set low levels
    SDI_GPIO_PORT->BRR = SDI_PIN;  // SDI low
    SII_GPIO_PORT->BRR = SII_PIN;  // SII low
    SDO_GPIO_PORT->BRR = SDO_PIN;  // SDO low
    RST_GPIO_PORT->BSRR = RST_PIN; // RST high (12V off, transistor off)

    // Enter High-voltage Serial programming mode
    VCC_GPIO_PORT->BSRR = VCC_PIN;  // VCC On
    delayMicroseconds(20);
    RST_GPIO_PORT->BRR = RST_PIN;   // RST low (12V on, transistor on)
    delayMicroseconds(10);

    // Set SDO to input
    SDO_GPIO_PORT->CRH &= ~(GPIO_CRH_MODE11 | GPIO_CRH_CNF11);
    SDO_GPIO_PORT->CRH |= GPIO_CRH_CNF11_0;  // Input floating

    delayMicroseconds(300);

    // Write fuses for ATtiny13
    writeFuse(LFUSE, EXPECTED_LFUSE);
    writeFuse(HFUSE, EXPECTED_HFUSE);

    // Verify by reading back
    uint8_t read_lfuse = readLFuse();
    uint8_t read_hfuse = readHFuse();

    // Exit programming mode
    SCI_GPIO_PORT->BRR = SCI_PIN;  // SCI low
    VCC_GPIO_PORT->BRR = VCC_PIN;  // VCC Off
    RST_GPIO_PORT->BSRR = RST_PIN; // RST high (12V Off)

    // Turn off LED after programming
    LED_GPIO_PORT->BSRR = LED_PIN;  // LED off

    // Indicate result
    if (read_lfuse == EXPECTED_LFUSE && read_hfuse == EXPECTED_HFUSE) {
        // Success: blink 5 times (100ms on, 100ms off)
        blinkLED(5, 100, 100);
    } else {
        // Failure: blink 3 times slowly (500ms on, 500ms off)
        blinkLED(3, 500, 500);
        while (1) {
            blinkLED(1, 1000, 1000);  // Continuous slow blink
        }
    }

    // Infinite loop after success
    while (1) {
        // Do nothing
    }
}

// GPIO Initialization
void GPIO_Init(void) {
    RCC->APB2ENR |= RCC_APB2ENR_IOPAEN | RCC_APB2ENR_IOPBEN | RCC_APB2ENR_IOPCEN;

    // VCC (PB8), RST (PB13), SCI (PB12), SII (PB10), SDI (PB9) as output
    GPIOB->CRH &= ~(GPIO_CRH_MODE8 | GPIO_CRH_CNF8);
    GPIOB->CRH |= GPIO_CRH_MODE8_0;  // PB8 output

    GPIOB->CRH &= ~(GPIO_CRH_MODE9 | GPIO_CRH_CNF9);
    GPIOB->CRH |= GPIO_CRH_MODE9_0;  // PB9 output

    GPIOB->CRH &= ~(GPIO_CRH_MODE10 | GPIO_CRH_CNF10);
    GPIOB->CRH |= GPIO_CRH_MODE10_0; // PB10 output

    GPIOB->CRH &= ~(GPIO_CRH_MODE11 | GPIO_CRH_CNF11);
    GPIOB->CRH |= GPIO_CRH_MODE11_0; // PB11 output (SDO)

    GPIOB->CRH &= ~(GPIO_CRH_MODE12 | GPIO_CRH_CNF12);
    GPIOB->CRH |= GPIO_CRH_MODE12_0; // PB12 output

    GPIOB->CRH &= ~(GPIO_CRH_MODE13 | GPIO_CRH_CNF13);
    GPIOB->CRH |= GPIO_CRH_MODE13_0; // PB13 output

    // Button PA0 input pull-up
    GPIOA->CRL &= ~(GPIO_CRL_MODE0 | GPIO_CRL_CNF0);
    GPIOA->CRL |= GPIO_CRL_CNF0_1;  // Input pull-up
    GPIOA->BSRR = BUTTON_PIN;

    // LED PC13 output
    GPIOC->CRH &= ~(GPIO_CRH_MODE13 | GPIO_CRH_CNF13);
    GPIOC->CRH |= GPIO_CRH_MODE13_0;  // Output
    GPIOC->BSRR = LED_PIN;            // LED off
}

// Delay functions (adjusted for 8MHz HSI from SystemInit)
void delayMicroseconds(uint32_t us) {
    for (volatile uint32_t i = 0; i < us * 2; i++) __NOP();  // Approx 1us per 4 cycles at 8MHz
}

void delay_ms(uint32_t ms) {
    for (volatile uint32_t i = 0; i < ms * 8000; i++) __NOP();  // Approx 1ms per 8000 cycles at 8MHz
}

// Blink LED
void blinkLED(uint32_t times, uint32_t on_ms, uint32_t off_ms) {
    for (uint32_t i = 0; i < times; i++) {
        LED_GPIO_PORT->BRR = LED_PIN;  // On
        delay_ms(on_ms);
        LED_GPIO_PORT->BSRR = LED_PIN; // Off
        delay_ms(off_ms);
    }
}

uint8_t shiftOut(uint8_t val1, uint8_t val2) {
    uint16_t inBits = 0;
    while (!(SDO_GPIO_PORT->IDR & SDO_PIN));
    unsigned int dout = (unsigned int)val1 << 2;
    unsigned int iout = (unsigned int)val2 << 2;
    for (int ii = 10; ii >= 0; ii--) {
        if (dout & (1 << ii)) SDI_GPIO_PORT->BSRR = SDI_PIN; else SDI_GPIO_PORT->BRR = SDI_PIN;
        if (iout & (1 << ii)) SII_GPIO_PORT->BSRR = SII_PIN; else SII_GPIO_PORT->BRR = SII_PIN;
        inBits <<= 1;
        if (SDO_GPIO_PORT->IDR & SDO_PIN) inBits |= 1;
        SCI_GPIO_PORT->BSRR = SCI_PIN;  // High
        SCI_GPIO_PORT->BRR = SCI_PIN;   // Low
    }
    return (uint8_t)(inBits >> 2);
}

void writeFuse(unsigned int fuse, uint8_t val) {
    shiftOut(0x40, 0x4C);
    shiftOut(val, 0x2C);
    shiftOut(0x00, (uint8_t)(fuse >> 8));
    shiftOut(0x00, (uint8_t)fuse);
}

uint8_t readLFuse(void) {
    shiftOut(0x04, 0x4C); shiftOut(0x00, 0x68);
    return shiftOut(0x00, 0x6C);
}

uint8_t readHFuse(void) {
    shiftOut(0x04, 0x4C); shiftOut(0x00, 0x7A);
    return shiftOut(0x00, 0x7E);
}
