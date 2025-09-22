/* main.c
   HVSP for ATtiny13 on STM32F103C6T6
   Pin mapping (as requested):
     PB0  -> RST (level-shifter control, inverted: HIGH -> 12V off, LOW -> 12V on)
     PB4  -> SCI (serial clock input to target)
     PB1  -> SDO  (target data output) - input
     PA6  -> SII  (instruction input to target)
     PA7  -> SDI  (data input to target)
     PA9  -> BUTTON (active low, internal pull-up)
     PC13 -> LED (status)
   If you have hardware to switch VCC for target, enable VCC_CONTROL_PIN_USED=1 and set VCC_PIN_* appropriately.
*/

/* ========== Configuration ========== */
#define VCC_CONTROL_PIN_USED 0   /* 0 = VCC tied to 3.3V (not controlled); 1 = control VCC via GPIO */
#define VCC_PIN_PORT GPIOA
#define VCC_PIN       (1 << 0)   /* PA0 if you have a transistor to switch VCC */

/* Timing configuration */
#define SCI_FREQ_HZ 100000U      /* target SCI frequency ~100kHz */
#define SCI_HALF_PERIOD_US (1000000U / (2U * SCI_FREQ_HZ)) /* us for half period */

/* ATTiny signatures */
#define ATTINY13_SIG 0x9007U

/* Fuse addresses from Arduino code */
#define LFUSE 0x646CU
#define HFUSE 0x747CU
#define EFUSE 0x666EU

/* ========== Includes ========== */
#include "stm32f1xx.h"  /* CMSIS-SVD style defs (register names) */
#include <stdint.h>

/* ========== Pin definitions (bitmasks) ========== */
#define RST_PORT GPIOB
#define RST_PIN  (1 << 0)   /* PB0 */

#define SCI_PORT GPIOB
#define SCI_PIN  (1 << 4)   /* PB4 */

#define SDO_PORT GPIOB
#define SDO_PIN  (1 << 1)   /* PB1 */

#define SII_PORT GPIOA
#define SII_PIN  (1 << 6)   /* PA6 */

#define SDI_PORT GPIOA
#define SDI_PIN  (1 << 7)   /* PA7 */

#define BUTTON_PORT GPIOA
#define BUTTON_PIN  (1 << 9) /* PA9 */

#define LED_PORT GPIOC
#define LED_PIN  (1 << 13)  /* PC13 (onboard LED) */

/* ========== Forward declarations ========== */
static void System_Init(void);
static void GPIO_Init_All(void);
static void DWT_Delay_Init(void);
static void delay_us(uint32_t us);

static inline void pin_set(GPIO_TypeDef *port, uint32_t pin);
static inline void pin_clear(GPIO_TypeDef *port, uint32_t pin);
static inline uint8_t pin_read(GPIO_TypeDef *port, uint32_t pin);

static void led_blink_pattern_success(void);
static void led_blink_pattern_error(void);
static void led_blink_startup(void);
static void led_blink_waiting(void);

static uint8_t hvsp_shiftOut(uint8_t val1, uint8_t val2);
static void hvsp_writeFuse(uint16_t fuse, uint8_t val);
static uint8_t hvsp_readFuses(void); /* returns 0 on success (prints via LED), (void)val used where needed */
static uint16_t hvsp_readSignature(void);

/* ========== DWT microsecond delay ========== */
static void DWT_Delay_Init(void)
{
    /* Enable TRC */
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    /* Enable counter */
    DWT->CYCCNT = 0;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
}

static void delay_us(uint32_t us)
{
    uint32_t cycles = (SystemCoreClock / 1000000U) * us;
    uint32_t start = DWT->CYCCNT;
    while ((DWT->CYCCNT - start) < cycles) { __NOP(); }
}

/* ========== Utility pin ops using BSRR ========== */
static inline void pin_set(GPIO_TypeDef *port, uint32_t pin)
{
    port->BSRR = pin; /* set */
}
static inline void pin_clear(GPIO_TypeDef *port, uint32_t pin)
{
    port->BRR = pin;  /* reset */
}
static inline uint8_t pin_read(GPIO_TypeDef *port, uint32_t pin)
{
    return ( (port->IDR & pin) ? 1U : 0U );
}

/* ========== System init (HSE=8MHz, no PLL) ========== */
static void System_Init(void)
{
    /* Enable HSI for safe start */
    RCC->CR |= RCC_CR_HSION;
    while (!(RCC->CR & RCC_CR_HSIRDY)) { }

    /* Disable LSE if present (backup domain) */
    RCC->APB1ENR |= RCC_APB1ENR_BKPEN | RCC_APB1ENR_PWREN;
    RCC->BDCR &= ~RCC_BDCR_LSEON;
    while (RCC->BDCR & RCC_BDCR_LSERDY) { }

    /* Enable HSE and wait */
    RCC->CR |= RCC_CR_HSEON;
    while (!(RCC->CR & RCC_CR_HSERDY)) { }

    /* Disable PLL */
    RCC->CR &= ~RCC_CR_PLLON;
    while (RCC->CR & RCC_CR_PLLRDY) { }

    /* Switch SYSCLK to HSE */
    RCC->CFGR &= ~RCC_CFGR_SW;
    RCC->CFGR |= RCC_CFGR_SW_HSE;
    while ((RCC->CFGR & RCC_CFGR_SWS) != RCC_CFGR_SWS_HSE) { }

    /* Reset dividers */
    RCC->CFGR &= ~(RCC_CFGR_HPRE | RCC_CFGR_PPRE1 | RCC_CFGR_PPRE2);

    /* Flash latency 0WS (0..24MHz) */
    FLASH->ACR &= ~FLASH_ACR_LATENCY;
    FLASH->ACR |= FLASH_ACR_LATENCY_0;

    /* Enable GPIOA, GPIOB, GPIOC clocks and AFIO */
    RCC->APB2ENR |= RCC_APB2ENR_IOPAEN | RCC_APB2ENR_IOPBEN | RCC_APB2ENR_IOPCEN | RCC_APB2ENR_AFIOEN;
}

/* ========== GPIO init ========== */
static void GPIO_Init_All(void)
{
    /* Configure GPIO modes via CRL/CRH
       We'll set outputs (push-pull) for RST, SCI, SII, SDI, LED, VCC(if used).
       SDO as input with pull-up (enable ODR bit).
       Button as input with pull-up (ODR bit).
    */

    /* --- GPIOA --- */
    /* Clear CRL/CRH for relevant pins, then set:
       PA6 (CRL bits for pin6): output push-pull 10MHz -> 0b1011 (0xB) per pin (CNF=00 OUT PP, MODE=10 for 2MHz; but we'll use 10MHz as 0x2)
       For simplicity use 10MHz push-pull: MODE=10(2MHz), CNF=00 -> 0x2. But earlier sample used AF push-pull 10MHz with 0xB for AF. We need plain push-pull for SII/SDI.
    */
    /* Reset PA6, PA7 bits in CRL */
    GPIOA->CRL &= ~(0xF << (6*4));
    GPIOA->CRL &= ~(0xF << (7*4));
    /* Set PA6, PA7 as General purpose push-pull output 10MHz: MODE=10 (0x2), CNF=00 -> 0x2 */
    GPIOA->CRL |= (0x2 << (6*4));
    GPIOA->CRL |= (0x2 << (7*4));

    /* PA9 (button) -> input with pull-up: in CRH for pin9 */
    GPIOA->CRH &= ~(0xF << ((9-8)*4));
    /* Input with pull-up/pull-down CNF=10, MODE=00 -> 0x8 */
    GPIOA->CRH |= (0x8 << ((9-8)*4));
    /* Enable pull-up by writing 1 to ODR bit */
    GPIOA->ODR |= BUTTON_PIN;

    /* If VCC control used, set PA0 as output push-pull */
    if (VCC_CONTROL_PIN_USED) {
        GPIOA->CRL &= ~(0xF << (0*4));
        GPIOA->CRL |= (0x2 << (0*4)); /* GP push-pull 10MHz */
        /* Ensure VCC off initially */
        pin_clear(VCC_PIN_PORT, VCC_PIN);
    }

    /* --- GPIOB --- */
    /* PB0 RST -> output push-pull 10MHz */
    GPIOB->CRL &= ~(0xF << (0*4));
    GPIOB->CRL |= (0x2 << (0*4));

    /* PB1 SDO -> input with pull-up */
    GPIOB->CRL &= ~(0xF << (1*4));
    GPIOB->CRL |= (0x8 << (1*4));
    GPIOB->ODR |= SDO_PIN; /* enable pull-up */

    /* PB4 SCI -> output push-pull 10MHz */
    GPIOB->CRL &= ~(0xF << (4*4));
    GPIOB->CRL |= (0x2 << (4*4));

    /* --- GPIOC --- */
    /* PC13 LED -> output push-pull (we'll use 2MHz to be conservative) -> MODE=10 CNF=00 -> 0x2 (pin13 in CRH: pin13 => (13-8)=5 index) */
    GPIOC->CRH &= ~(0xF << ((13-8)*4));
    GPIOC->CRH |= (0x2 << ((13-8)*4));
    /* Turn LED off initially (board LED often active-low) */
    pin_set(LED_PORT, LED_PIN); /* set high -> LED off on many boards */
}

/* ========== LED patterns ========== */
static void led_pulse(uint32_t times, uint32_t on_us, uint32_t off_us)
{
    for (uint32_t i = 0; i < times; ++i) {
        pin_clear(LED_PORT, LED_PIN); /* on (active low on many boards) */
        delay_us(on_us);
        pin_set(LED_PORT, LED_PIN);   /* off */
        delay_us(off_us);
    }
}

static void led_blink_startup(void)
{
    /* 3 flashes in 1.5s -> each cycle 0.25s on+off average */
    for (int i = 0; i < 3; ++i) {
        pin_clear(LED_PORT, LED_PIN);
        delay_us(150000U / 3U); /* approx 50ms? but to fit 1.5s -> simple shorter pulses */
        pin_set(LED_PORT, LED_PIN);
        delay_us(350000U / 3U);
    }
}

static void led_blink_waiting(void)
{
    /* Blink once per second: LED on 100ms, off 900ms (repeating) */
    pin_clear(LED_PORT, LED_PIN);
    delay_us(100000U);
    pin_set(LED_PORT, LED_PIN);
    delay_us(900000U);
}

static void led_blink_pattern_success(void)
{
    /* 5 flashes in 3s */
    for (int i = 0; i < 5; ++i) {
        pin_clear(LED_PORT, LED_PIN);
        delay_us(300000U / 5U);
        pin_set(LED_PORT, LED_PIN);
        delay_us(3000000U / 5U - (300000U / 5U));
    }
}

static void led_blink_pattern_error(void)
{
    /* 5 fast flashes in 1s */
    for (int i = 0; i < 5; ++i) {
        pin_clear(LED_PORT, LED_PIN);
        delay_us(80000U); /* 80ms */
        pin_set(LED_PORT, LED_PIN);
        delay_us(12000U); /* quick off */
    }
}

/* ========== HVSP low-level helpers ========== */

/* Set/clear SCI, SII, SDI, RST, VCC helpers */
static inline void SCI_set(void)  { pin_set(SCI_PORT, SCI_PIN); }
static inline void SCI_clear(void){ pin_clear(SCI_PORT, SCI_PIN); }

static inline void SII_set(void)  { pin_set(SII_PORT, SII_PIN); }
static inline void SII_clear(void){ pin_clear(SII_PORT, SII_PIN); }

static inline void SDI_set(void)  { pin_set(SDI_PORT, SDI_PIN); }
static inline void SDI_clear(void){ pin_clear(SDI_PORT, SDI_PIN); }

static inline void RST_set(void)  { pin_set(RST_PORT, RST_PIN); }  /* HIGH -> 12V off (inverting level shifter) */
static inline void RST_clear(void){ pin_clear(RST_PORT, RST_PIN); }/* LOW -> 12V on */

static inline void VCC_on(void) {
#if VCC_CONTROL_PIN_USED
    pin_set(VCC_PIN_PORT, VCC_PIN);
#else
    (void)0;
#endif
}
static inline void VCC_off(void) {
#if VCC_CONTROL_PIN_USED
    pin_clear(VCC_PIN_PORT, VCC_PIN);
#else
    (void)0;
#endif
}

/* hvsp_shiftOut: replicates algorithm from Arduino code:
   waits until SDO goes high, then shifts 11 bits (ii=10..0),
   for each step sets SDI and SII according to dout,iout, then pulses SCI,
   accumulating SDO reads.
   Returns (inBits >> 2) as in Arduino code.
*/
static uint8_t hvsp_shiftOut(uint8_t val1, uint8_t val2)
{
    uint16_t dout = ((uint16_t)val1) << 2;
    uint16_t iout = ((uint16_t)val2) << 2;
    uint16_t inBits = 0;

    /* Wait until SDO goes high */
    uint32_t timeout = 100000U;
    while (!pin_read(SDO_PORT, SDO_PIN)) {
        if (!--timeout) break;
        delay_us(1);
    }

    for (int ii = 10; ii >= 0; --ii) {
        /* set SDI */
        if (dout & (1U << ii)) SDI_set(); else SDI_clear();
        /* set SII */
        if (iout & (1U << ii)) SII_set(); else SII_clear();
        inBits <<= 1;
        /* read SDO into LSB */
        inBits |= (pin_read(SDO_PORT, SDO_PIN) ? 1U : 0U);
        /* Pulse SCI high then low */
        SCI_set();
        delay_us(SCI_HALF_PERIOD_US); /* half period high */
        SCI_clear();
        delay_us(SCI_HALF_PERIOD_US); /* half period low */
    }
    return (uint8_t)(inBits >> 2);
}

/* Write fuse using the sequence from Arduino: shiftOut(0x40, 0x4C); shiftOut(val,0x2C);
   shiftOut(0x00, fuse>>8); shiftOut(0x00, fuse);
*/
static void hvsp_writeFuse(uint16_t fuse, uint8_t val)
{
    (void)hvsp_shiftOut(0x40, 0x4C);
    (void)hvsp_shiftOut(val,   0x2C);
    (void)hvsp_shiftOut(0x00, (uint8_t)(fuse >> 8));
    (void)hvsp_shiftOut(0x00, (uint8_t)(fuse & 0xFF));
}

/* Read LFUSE, HFUSE, EFUSE and blink LED with values as feedback (we don't have UART).
   We follow Arduino readFuses sequence.
*/
static uint8_t hvsp_readFuses(void)
{
    uint8_t val;
    /* LFuse */
    (void)hvsp_shiftOut(0x04, 0x4C);
    (void)hvsp_shiftOut(0x00, 0x68);
    val = hvsp_shiftOut(0x00, 0x6C);
    (void)val; /* silence unused var warning as requested */

    /* HFuse */
    (void)hvsp_shiftOut(0x04, 0x4C);
    (void)hvsp_shiftOut(0x00, 0x7A);
    val = hvsp_shiftOut(0x00, 0x7E);
    (void)val;

    /* EFuse */
    (void)hvsp_shiftOut(0x04, 0x4C);
    (void)hvsp_shiftOut(0x00, 0x6A);
    val = hvsp_shiftOut(0x00, 0x6E);
    (void)val;

    return 0;
}

/* readSignature: loops ii=1..2 and composes two bytes into 16-bit signature */
static uint16_t hvsp_readSignature(void)
{
    uint16_t sig = 0;
    uint8_t val;
    for (int ii = 1; ii < 3; ++ii) {
        (void)hvsp_shiftOut(0x08, 0x4C);
        (void)hvsp_shiftOut((uint8_t)ii, 0x0C);
        (void)hvsp_shiftOut(0x00, 0x68);
        val = hvsp_shiftOut(0x00, 0x6C);
        sig = (uint16_t)((sig << 8) + val);
    }
    return sig;
}

/* ========== Main ========== */
int main(void)
{
    System_Init();
    DWT_Delay_Init();
    GPIO_Init_All();

    /* Small startup blink pattern */
    led_blink_startup();

    /* Wait 1s then enter waiting loop (blinking once/sec) */
    for (int i = 0; i < 3; ++i) { led_blink_waiting(); } /* short sequence */

    while (1) {
        /* Idle: blink once per second while waiting for button press */
        if (!pin_read(BUTTON_PORT, BUTTON_PIN)) { /* button pressed (active low) */
            /* Debounce simple */
            delay_us(50000U);
            if (!pin_read(BUTTON_PORT, BUTTON_PIN)) {
                /* Start HVSP sequence */
                /* Prepare pins: set SDO as output then low, SII/SDI low */
                /* Here SDO is input by default, but to follow Arduino we may temporarily drive it - but we can't if hardware tied.
                   We'll assume SDO pin is tri-stated when configured as input and we can configure as output if necessary.
                */

                /* Configure SDO as output: modify CRL */
                GPIOB->CRL &= ~(0xF << (1*4));
                GPIOB->CRL |= (0x2 << (1*4)); /* push-pull output 10MHz */
                /* Set SDI/SII low */
                SDI_clear();
                SII_clear();
                /* Set SDO low */
                pin_clear(SDO_PORT, SDO_PIN);

                /* RST HIGH -> 12V off (level shifter inverting) */
                RST_set();
                /* VCC on */
                VCC_on();
                delay_us(20U);
                /* RST LOW -> 12V on */
                RST_clear();
                delay_us(10U);
                /* Configure SDO as input with pull-up */
                GPIOB->CRL &= ~(0xF << (1*4));
                GPIOB->CRL |= (0x8 << (1*4));
                GPIOB->ODR |= SDO_PIN;
                delay_us(300U);

                /* read signature */
                uint16_t sig = hvsp_readSignature();

                if (sig == ATTINY13_SIG) {
                    /* Restore fuses for ATtiny13 */
                    hvsp_readFuses();
                    hvsp_writeFuse(LFUSE, 0x6A);
                    hvsp_writeFuse(HFUSE, 0xFF);
                    hvsp_readFuses();
                    /* Indicate success */
                    led_blink_pattern_success();
                } else {
                    /* Not recognized or other devices: indicate error */
                    led_blink_pattern_error();
                }

                /* cleanup: set SCI low, VCC off, RST high (12V off) */
                SCI_clear();
                VCC_off();
                RST_set();

                /* Reconfigure SDO as input (already) */
                GPIOB->CRL &= ~(0xF << (1*4));
                GPIOB->CRL |= (0x8 << (1*4));
                GPIOB->ODR |= SDO_PIN;

                /* small delay before next waiting */
                for (int w = 0; w < 4; ++w) led_blink_waiting();
            }
        } else {
            /* normal waiting blink */
            led_blink_waiting();
        }
    }

    /* not reached */
    return 0;
}
