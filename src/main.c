/* main.c — объединённая версия:
   - индикация старта LED на PC13
   - ожидание кнопки PA0
   - HVSP (12V) sequence для восстановления фьюзов ATtiny13 (LFUSE, HFUSE)
   - задержки рассчитаны через SystemCoreClock
*/

#include <stdint.h>

/* --- Адреса регистров (как в твоём проекте) --- */
#define PERIPH_BASE       0x40000000U
#define APB2PERIPH_BASE   (PERIPH_BASE + 0x10000U)
#define AHBPERIPH_BASE    (PERIPH_BASE + 0x20000U)

#define RCC_BASE          (AHBPERIPH_BASE + 0x1000U)
#define GPIOA_BASE        (APB2PERIPH_BASE + 0x0800U)
#define GPIOB_BASE        (APB2PERIPH_BASE + 0x0C00U)
#define GPIOC_BASE        (APB2PERIPH_BASE + 0x1000U)

#define RCC_APB2ENR       (*(volatile uint32_t*)(RCC_BASE + 0x18U))

#define GPIOA_CRL         (*(volatile uint32_t*)(GPIOA_BASE + 0x00U))
#define GPIOA_IDR         (*(volatile uint32_t*)(GPIOA_BASE + 0x08U))
#define GPIOA_BSRR        (*(volatile uint32_t*)(GPIOA_BASE + 0x10U))

#define GPIOB_CRH         (*(volatile uint32_t*)(GPIOB_BASE + 0x04U))
#define GPIOB_BSRR        (*(volatile uint32_t*)(GPIOB_BASE + 0x10U))
#define GPIOB_BRR         (*(volatile uint32_t*)(GPIOB_BASE + 0x14U))
#define GPIOB_IDR         (*(volatile uint32_t*)(GPIOB_BASE + 0x08U))

#define GPIOC_CRH         (*(volatile uint32_t*)(GPIOC_BASE + 0x04U))
#define GPIOC_BSRR        (*(volatile uint32_t*)(GPIOC_BASE + 0x10U))
#define GPIOC_BRR         (*(volatile uint32_t*)(GPIOC_BASE + 0x14U))

/* --- Пины (согласно твоему апаратному подключению) --- */
#define LED_PIN     (1 << 13) /* PC13 */
#define BUTTON_PIN  (1 << 0)  /* PA0 */

#define VCC_PIN     (1 << 8)  /* PB8 - управляет 12V через транзистор/ключ */
#define RST_PIN     (1 << 13) /* PB13 - reset line (ключ для подачи 12V) */

#define SCI_PIN     (1 << 12) /* PB12 - SCLK */
#define SDO_PIN     (1 << 11) /* PB11 - MISO / slave output */
#define SII_PIN     (1 << 10) /* PB10 - second input */
#define SDI_PIN     (1 << 9)  /* PB9  - MOSI / data out */

/* --- Фьюзы и ожидаемые значения (как у тебя было) --- */
#define LFUSE  0x646C
#define HFUSE  0x747C

#define EXPECTED_LFUSE      0x6A
#define EXPECTED_HFUSE      0xFF

/* --- Прототипы --- */
void GPIO_Init(void);
void blinkLED(uint32_t times, uint32_t on_ms, uint32_t off_ms);
uint8_t shiftOut(uint8_t val1, uint8_t val2);
void writeFuse(unsigned int fuse, uint8_t val);
uint8_t readLFuse(void);
uint8_t readHFuse(void);

/* Объявляем SystemCoreClock (определён в system_stm32f1xx.c) */
extern uint32_t SystemCoreClock;

/* --- Задержки, зависящие от SystemCoreClock --- */
void delayMicroseconds(uint32_t us) {
    /* количество циклов NOP примерно = SystemCoreClock (Гц) * microsec / (NOP_cyc),
       мы используем делитель 4 как аппроксимацию (зависит от оптимизаций). */
    uint32_t cycles_per_us = (SystemCoreClock / 1000000U);
    uint32_t inner = cycles_per_us / 4U;
    if (inner < 1) inner = 1;
    for (uint32_t j = 0; j < us; ++j) {
        for (volatile uint32_t i = 0; i < inner; ++i) __asm__("nop");
    }
}

void delay_ms(uint32_t ms) {
    uint32_t cycles_per_ms = (SystemCoreClock / 1000U);
    uint32_t inner = cycles_per_ms / 4U;
    if (inner < 1) inner = 1;
    for (uint32_t m = 0; m < ms; ++m) {
        for (volatile uint32_t i = 0; i < inner; ++i) __asm__("nop");
    }
}

/* --- Простая подсветка LED --- */
void blinkLED(uint32_t times, uint32_t on_ms, uint32_t off_ms) {
    for (uint32_t i = 0; i < times; i++) {
        GPIOC_BRR = LED_PIN;  // LED on (PC13 low)
        delay_ms(on_ms);
        GPIOC_BSRR = LED_PIN; // LED off (PC13 high)
        delay_ms(off_ms);
    }
}

/* --- Инициализация GPIO (как у тебя) --- */
void GPIO_Init(void) {
    /* включаем тактирование GPIOA, GPIOB, GPIOC */
    RCC_APB2ENR |= (1 << 2) | (1 << 3) | (1 << 4);

    /* PC13 как выход (CRH, pin 13) */
    GPIOC_CRH &= ~(0xF << (13 * 4));
    GPIOC_CRH |=  (0x1 << (13 * 4)); /* output push-pull 10MHz */
    GPIOC_BSRR = LED_PIN; /* LED off */

    /* PB8 (VCC control), PB13 (RST / 12V control), PB12 (SCI), PB11 (SDO), PB10 (SII), PB9 (SDI) */
    GPIOB_CRH &= ~(0xF << (8 * 4));   GPIOB_CRH |= (0x1 << (8 * 4));  /* PB8 output */
    GPIOB_CRH &= ~(0xF << (13 * 4));  GPIOB_CRH |= (0x1 << (13 * 4)); /* PB13 output */
    GPIOB_CRH &= ~(0xF << (12 * 4));  GPIOB_CRH |= (0x1 << (12 * 4)); /* PB12 output */
    GPIOB_CRH &= ~(0xF << (11 * 4));  GPIOB_CRH |= (0x4 << (11 * 4)); /* PB11 input (floating) -> 0100 */
    GPIOB_CRH &= ~(0xF << (10 * 4));  GPIOB_CRH |= (0x1 << (10 * 4)); /* PB10 output */
    GPIOB_CRH &= ~(0xF << (9 * 4));   GPIOB_CRH |= (0x1 << (9 * 4));  /* PB9 output */

    /* PA0 (button) as input floating */
    GPIOA_CRL &= ~(0xF << (0 * 4));
    GPIOA_CRL |=  (0x4 << (0 * 4)); /* floating input */
}

/* --- SPI-like shift function used for HVSP sequence --- */
uint8_t shiftOut(uint8_t val1, uint8_t val2) {
    uint16_t inBits = 0;

    /* wait until SDO (PB11) goes high (device ready) */
    while (!(GPIOB_IDR & SDO_PIN));

    unsigned int dout = (unsigned int)val1 << 2;
    unsigned int iout = (unsigned int)val2 << 2;

    for (int ii = 10; ii >= 0; ii--) {
        if (dout & (1 << ii)) GPIOB_BSRR = SDI_PIN; else GPIOB_BRR = SDI_PIN;
        if (iout & (1 << ii)) GPIOB_BSRR = SII_PIN; else GPIOB_BRR = SII_PIN;

        inBits <<= 1;
        if (GPIOB_IDR & SDO_PIN) inBits |= 1;

        /* clock pulse on SCI (PB12) */
        GPIOB_BSRR = SCI_PIN;  // High
        delayMicroseconds(1);
        GPIOB_BRR = SCI_PIN;   // Low
        delayMicroseconds(1);
    }
    return (uint8_t)(inBits >> 2);
}

/* --- ТОП-уровень записи фьюза (адрес — fuse) --- */
void writeFuse(unsigned int fuse, uint8_t val) {
    shiftOut(0x40, 0x4C);
    shiftOut(val, 0x2C);
    shiftOut(0x00, (uint8_t)(fuse >> 8));
    shiftOut(0x00, (uint8_t)fuse);
}

/* --- чтение LFUSE, HFUSE --- */
uint8_t readLFuse(void) {
    shiftOut(0x04, 0x4C); shiftOut(0x00, 0x68);
    return shiftOut(0x00, 0x6C);
}
uint8_t readHFuse(void) {
    shiftOut(0x04, 0x4C); shiftOut(0x00, 0x7A);
    return shiftOut(0x00, 0x7E);
}

/* --- main: стартовая индикация -> ожидание кнопки -> HVSP -> проверка --- */
int main(void) {
    GPIO_Init();

    /* сначала убедимся, что LED в известном состоянии (выключен) */
    GPIOC_BSRR = LED_PIN;
    delay_ms(50);

    /* индикация старта: 3 двойных мигания */
    for (uint32_t cycle = 0; cycle < 3; cycle++) {
        GPIOC_BRR = LED_PIN;  // On
        delay_ms(300);
        GPIOC_BSRR = LED_PIN; // Off
        delay_ms(100);

        GPIOC_BRR = LED_PIN;  // On
        delay_ms(300);
        GPIOC_BSRR = LED_PIN; // Off
        delay_ms(300);
    }

    /* Ждём нажатия кнопки (PA0 active low) */
    while ((GPIOA_IDR & BUTTON_PIN) != 0) {
        /* просто ждем */
    }
    delay_ms(50); /* debounce */

    /* Индикатор начала программирования */
    GPIOC_BRR = LED_PIN; // LED on
    delay_ms(50);

    /* --- Подготовка линий перед HVSP --- */
    /* Сначала установим SDI/SII/SCI низкими, RST высокий (12V OFF) */
    GPIOB_BRR = SDI_PIN;  /* SDI low */
    GPIOB_BRR = SII_PIN;  /* SII low */
    GPIOB_BRR = SDO_PIN;  /* SDO low - but SDO is input, so earlier we set it as input */
    GPIOB_BSRR = RST_PIN; /* RST high (12V off) */

    /* Включаем VCC (через PB8) и потом опускаем RST чтобы подать 12V на RST через транзистор */
    GPIOB_BSRR = VCC_PIN;  /* VCC ON */
    delayMicroseconds(20);
    GPIOB_BRR = RST_PIN;   /* RST low => 12V applied to target reset */
    delayMicroseconds(10);

    /* Теперь SDO должен быть входом; ensure input mode done in GPIO_Init */

    /* Посылаем команды HVSP: прочитать и (при необходимости) записать фьюзы */
    uint8_t read_lfuse = readLFuse();
    uint8_t read_hfuse = readHFuse();

    /* Если нужные фьюзы не соответствуют ожидаемым — запишем */
    if (read_lfuse != EXPECTED_LFUSE) {
        writeFuse(LFUSE, EXPECTED_LFUSE);
    }
    if (read_hfuse != EXPECTED_HFUSE) {
        writeFuse(HFUSE, EXPECTED_HFUSE);
    }

    /* Завершение HVSP: отключаем 12V и VCC */
    GPIOB_BRR = VCC_PIN;  /* VCC OFF */
    GPIOB_BSRR = RST_PIN; /* RST high (12V OFF) */
    delay_ms(10);

    /* Проверим заново */
    read_lfuse = readLFuse();
    read_hfuse = readHFuse();

    /* Индикация результата */
    if (read_lfuse == EXPECTED_LFUSE && read_hfuse == EXPECTED_HFUSE) {
        /* Успех: быстрые 5 миганий */
        blinkLED(5, 100, 100);
    } else {
        /* Провал: 3 медленных мигания, затем вечный цикл медленного мигания */
        blinkLED(3, 500, 500);
        while (1) {
            blinkLED(1, 1000, 1000);
        }
    }

    while (1) {
        /* основная программа ничего не делает дальше */
    }

    /* никогда сюда не попадём */
    return 0;
}
