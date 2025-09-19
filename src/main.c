#include <stdint.h>

#define PERIPH_BASE       0x40000000U
#define APB2PERIPH_BASE   (PERIPH_BASE + 0x10000U)
#define AHBPERIPH_BASE    (PERIPH_BASE + 0x20000U)

#define RCC_BASE          (AHBPERIPH_BASE + 0x1000U)
#define GPIOA_BASE        (APB2PERIPH_BASE + 0x0800U)
#define GPIOB_BASE        (APB2PERIPH_BASE + 0x0C00U)
#define GPIOC_BASE        (APB2PERIPH_BASE + 0x1000U)

#define RCC_APB2ENR       (*(volatile uint32_t*)(RCC_BASE + 0x18U))
#define GPIOA_CRL         (*(volatile uint32_t*)(GPIOA_BASE + 0x00U))
#define GPIOA_BSRR        (*(volatile uint32_t*)(GPIOA_BASE + 0x10U))
#define GPIOA_IDR         (*(volatile uint32_t*)(GPIOA_BASE + 0x08U))
#define GPIOB_CRH         (*(volatile uint32_t*)(GPIOB_BASE + 0x04U))
#define GPIOB_BSRR        (*(volatile uint32_t*)(GPIOB_BASE + 0x10U))
#define GPIOB_BRR         (*(volatile uint32_t*)(GPIOB_BASE + 0x14U))
#define GPIOB_IDR         (*(volatile uint32_t*)(GPIOB_BASE + 0x08U))
#define GPIOC_CRH         (*(volatile uint32_t*)(GPIOC_BASE + 0x04U))
#define GPIOC_BSRR        (*(volatile uint32_t*)(GPIOC_BASE + 0x10U))
#define GPIOC_BRR         (*(volatile uint32_t*)(GPIOC_BASE + 0x14U))

// Pin definitions (bit positions)
#define BUTTON_PIN        (1 << 0)  // PA0
#define LED_PIN           (1 << 13) // PC13
#define VCC_PIN           (1 << 8)  // PB8
#define RST_PIN           (1 << 13) // PB13
#define SCI_PIN           (1 << 12) // PB12
#define SDO_PIN           (1 << 11) // PB11
#define SII_PIN           (1 << 10) // PB10
#define SDI_PIN           (1 << 9)  // PB9

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
    GPIO_Init();

    // Initial indication: 3 cycles of 2 blinks (300ms on, 1s pause)
    for (uint32_t cycle = 0; cycle < 3; cycle++) {
        GPIOC_BRR = LED_PIN;  // LED on
        delay_ms(300);
        GPIOC_BSRR = LED_PIN; // LED off
        delay_ms(100);        // Short pause between blinks
        GPIOC_BRR = LED_PIN;  // LED on
        delay_ms(300);
        GPIOC_BSRR = LED_PIN; // LED off
        delay_ms(1000 - 200); // 1s pause minus the 200ms already used
    }

    // Wait for button press (active low)
    while ((GPIOA_IDR & BUTTON_PIN) != 0) {
        // Wait until button is pressed
    }

    // Debounce delay
    delay_ms(50);

    // Turn on LED to indicate programming start
    GPIOC_BRR = LED_PIN;  // LED on

    // Initialize pins to enter programming mode
    // Set SDO to output
    GPIOB_CRH &= ~(0xF << (11 * 4));  // Clear SDO (PB11) mode
    GPIOB_CRH |= (0x1 << (11 * 4));   // Output 10MHz

    // Set low levels
    GPIOB_BRR = SDI_PIN;  // SDI low
    GPIOB_BRR = SII_PIN;  // SII low
    GPIOB_BRR = SDO_PIN;  // SDO low
    GPIOB_BSRR = RST_PIN; // RST high (12V off, transistor off)

    // Enter High-voltage Serial programming mode
    GPIOB_BSRR = VCC_PIN;  // VCC On
    delayMicroseconds(20);
    GPIOB_BRR = RST_PIN;   // RST low (12V on, transistor on)
    delayMicroseconds(10);

    // Set SDO to input
    GPIOB_CRH &= ~(0xF << (11 * 4));  // Clear SDO (PB11) mode
    GPIOB_CRH |= (0x4 << (11 * 4));   // Input floating

    delayMicroseconds(300);

    // Write fuses for ATtiny13
    writeFuse(LFUSE, EXPECTED_LFUSE);
    writeFuse(HFUSE, EXPECTED_HFUSE);

    // Verify by reading back
    uint8_t read_lfuse = readLFuse();
    uint8_t read_hfuse = readHFuse();

    // Exit programming mode
    GPIOB_BRR = SCI_PIN;  // SCI low
    GPIOB_BRR = VCC_PIN;  // VCC Off
    GPIOB_BSRR = RST_PIN; // RST high (12V Off)

    // Turn off LED after programming
    GPIOC_BSRR = LED_PIN;  // LED off

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

    while (1) {
        // Do nothing
    }
}

void GPIO_Init(void) {
    RCC_APB2ENR |= (1 << 2) | (1 << 3) | (1 << 4);  // Enable GPIOA, GPIOB, GPIOC clocks

    // VCC (PB8), RST (PB13), SCI (PB12), SII (PB10), SDI (PB9) as output
    GPIOB_CRH &= ~(0xF << (8 * 4));   GPIOB_CRH |= (0x1 << (8 * 4));  // PB8 output
    GPIOB_CRH &= ~(0xF << (13 * 4));  GPIOB_CRH |= (0x1 << (13 * 4)); // PB13 output
    GPIOB_CRH &= ~(0xF << (12 * 4));  GPIOB_CRH |= (0x1 << (12 * 4)); // PB12 output
    GPIOB_CRH &= ~(0xF << (10 * 4));  GPIOB_CRH |= (0x1 << (10 * 4)); // PB10 output
    GPIOB_CRH &= ~(0xF << (9 * 4));   GPIOB_CRH |= (0x1 << (9 * 4));  // PB9 output

    // Button PA0 input pull-up
    GPIOA_CRL &= ~(0xF << (0 * 4));   GPIOA_CRL |= (0x8 << (0 * 4));  // Input pull-up
    GPIOA_BSRR = BUTTON_PIN;

    // LED PC13 output
    GPIOC_CRH &= ~(0xF << (13 * 4));  GPIOC_CRH |= (0x1 << (13 * 4)); // Output
    GPIOC_BSRR = LED_PIN;             // LED off
}

void delayMicroseconds(uint32_t us) {
    for (volatile uint32_t i = 0; i < us * 2; i++) __asm__("nop");  // Approx 1us per 4 cycles at 8MHz
}

void delay_ms(uint32_t ms) {
    for (volatile uint32_t i = 0; i < ms * 8000; i++) __asm__("nop");  // Approx 1ms per 8000 cycles at 8MHz
}

void blinkLED(uint32_t times, uint32_t on_ms, uint32_t off_ms) {
    for (uint32_t i = 0; i < times; i++) {
        GPIOC_BRR = LED_PIN;  // On
        delay_ms(on_ms);
        GPIOC_BSRR = LED_PIN; // Off
        delay_ms(off_ms);
    }
}

uint8_t shiftOut(uint8_t val1, uint8_t val2) {
    uint16_t inBits = 0;
    while (!(GPIOB_IDR & SDO_PIN));
    unsigned int dout = (unsigned int)val1 << 2;
    unsigned int iout = (unsigned int)val2 << 2;
    for (int ii = 10; ii >= 0; ii--) {
        if (dout & (1 << ii)) GPIOB_BSRR = SDI_PIN; else GPIOB_BRR = SDI_PIN;
        if (iout & (1 << ii)) GPIOB_BSRR = SII_PIN; else GPIOB_BRR = SII_PIN;
        inBits <<= 1;
        if (GPIOB_IDR & SDO_PIN) inBits |= 1;
        GPIOB_BSRR = SCI_PIN;  // High
        GPIOB_BRR = SCI_PIN;   // Low
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
