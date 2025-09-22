#include "stm32f1xx.h"

/* ======= Конфигурация ======= */
#define LED_PORT    GPIOC
#define LED_PIN     13

#define BUTTON_PORT GPIOA
#define BUTTON_PIN  9

#define VCC_CONTROL_PIN_USED 0   // =1 если хотите управлять VCC через транзистор
#define VCC_PORT    GPIOA
#define VCC_PIN     0

#define RST_PORT    GPIOB
#define RST_PIN     0   // управляет транзистором для 12В на RESET ATtiny

#define SCI_PORT    GPIOB
#define SCI_PIN     4   // Serial Clock Input (PB4, освободили от JTAG!)

#define SII_PORT    GPIOA
#define SII_PIN     6   // Serial Instruction Input

#define SDI_PORT    GPIOA
#define SDI_PIN     7   // Serial Data Input

#define SDO_PORT    GPIOB
#define SDO_PIN     1   // Serial Data Output (вход в STM32)

/* SCI частота */
#define SCI_FREQ_HZ         100000U
#define SCI_HALF_PERIOD_US  (1000000U / (2 * SCI_FREQ_HZ))

/* Макросы для GPIO */
#define pin_set(PORT,PIN)     ((PORT)->BSRR = (1U << (PIN)))
#define pin_clear(PORT,PIN)   ((PORT)->BRR  = (1U << (PIN)))
#define pin_read(PORT,PIN)    (((PORT)->IDR >> (PIN)) & 1U)

/* ======= Задержки ======= */
static inline void delay_us(uint32_t us) {
    uint32_t cycles = (SystemCoreClock / 1000000U) * us;
    uint32_t start = DWT->CYCCNT;
    while ((DWT->CYCCNT - start) < cycles) ;
}

static void DWT_Init(void) {
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CYCCNT = 0;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
}

/* ======= Инициализация GPIO ======= */
static void GPIO_Init_All(void) {
    /* Тактирование портов A, B, C и AFIO */
    RCC->APB2ENR |= RCC_APB2ENR_IOPAEN |
                    RCC_APB2ENR_IOPBEN |
                    RCC_APB2ENR_IOPCEN |
                    RCC_APB2ENR_AFIOEN;

    /* Отключаем JTAG, оставляем SWD — чтобы освободить PB4 */
    AFIO->MAPR |= AFIO_MAPR_SWJ_CFG_JTAGDISABLE;

    /* PC13 (LED) = выход push-pull */
    GPIOC->CRH &= ~(0xF << ((LED_PIN - 8) * 4));
    GPIOC->CRH |=  (0x1 << ((LED_PIN - 8) * 4));

    /* PA9 (кнопка) = вход с подтяжкой */
    GPIOA->CRH &= ~(0xF << ((BUTTON_PIN - 8) * 4));
    GPIOA->CRH |=  (0x8 << ((BUTTON_PIN - 8) * 4));  // input PU/PD
    pin_set(GPIOA, BUTTON_PIN); // подтяжка вверх

    /* PB0 (RST 12V control) = выход */
    GPIOB->CRL &= ~(0xF << (RST_PIN * 4));
    GPIOB->CRL |=  (0x3 << (RST_PIN * 4));

#if VCC_CONTROL_PIN_USED
    /* PA0 (VCC control) = выход */
    GPIOA->CRL &= ~(0xF << (VCC_PIN * 4));
    GPIOA->CRL |=  (0x3 << (VCC_PIN * 4));
#endif

    /* PB4 (SCI) = выход */
    GPIOB->CRL &= ~(0xF << (SCI_PIN * 4));
    GPIOB->CRL |=  (0x3 << (SCI_PIN * 4));

    /* PA6 (SII) = выход */
    GPIOA->CRL &= ~(0xF << (SII_PIN * 4));
    GPIOA->CRL |=  (0x3 << (SII_PIN * 4));

    /* PA7 (SDI) = выход */
    GPIOA->CRL &= ~(0xF << (SDI_PIN * 4));
    GPIOA->CRL |=  (0x3 << (SDI_PIN * 4));

    /* PB1 (SDO) = вход */
    GPIOB->CRL &= ~(0xF << (SDO_PIN * 4));
    GPIOB->CRL |=  (0x8 << (SDO_PIN * 4));  // input PU/PD
}

/* ======= Утилиты ======= */
static void led_blink_pattern(uint8_t times, uint32_t delay_ms) {
    for (uint8_t i = 0; i < times; i++) {
        pin_clear(LED_PORT, LED_PIN);
        delay_us(delay_ms * 1000U);
        pin_set(LED_PORT, LED_PIN);
        delay_us(delay_ms * 1000U);
    }
}

/* ======= HVSP Core ======= */
static void hvsp_shiftOut(uint16_t data) {
    for (int i = 10; i >= 0; i--) {
        if (data & (1 << i))
            pin_set(SII_PORT, SII_PIN);
        else
            pin_clear(SII_PORT, SII_PIN);

        /* SDI — бит i */
        if (data & (1 << i))
            pin_set(SDI_PORT, SDI_PIN);
        else
            pin_clear(SDI_PORT, SDI_PIN);

        /* SCI такт */
        pin_set(SCI_PORT, SCI_PIN);
        delay_us(SCI_HALF_PERIOD_US);
        pin_clear(SCI_PORT, SCI_PIN);
        delay_us(SCI_HALF_PERIOD_US);
    }
}

static uint16_t hvsp_read(uint16_t instr) {
    uint16_t result = 0;
    for (int i = 10; i >= 0; i--) {
        /* выставляем SII */
        if (instr & (1 << i))
            pin_set(SII_PORT, SII_PIN);
        else
            pin_clear(SII_PORT, SII_PIN);

        pin_set(SCI_PORT, SCI_PIN);
        delay_us(SCI_HALF_PERIOD_US);

        /* читаем SDO на середине такта */
        if (pin_read(SDO_PORT, SDO_PIN))
            result |= (1 << i);

        pin_clear(SCI_PORT, SCI_PIN);
        delay_us(SCI_HALF_PERIOD_US);
    }
    return result;
}

static uint16_t hvsp_readSignature(void) {
    return hvsp_read(0x080);  // команда чтения сигнатуры
}

static void hvsp_writeFuse(uint16_t cmd) {
    hvsp_shiftOut(cmd);
}

/* ======= MAIN ======= */
int main(void) {
    SystemCoreClock = 8000000U; // если используете HSE=8MHz без PLL
    DWT_Init();
    GPIO_Init_All();

    /* LED моргает 3 раза при старте */
    led_blink_pattern(3, 200);

    while (1) {
        if (!pin_read(BUTTON_PORT, BUTTON_PIN)) {
            delay_us(50000U); // debounce 50ms
            if (!pin_read(BUTTON_PORT, BUTTON_PIN)) {

#if VCC_CONTROL_PIN_USED
                pin_set(VCC_PORT, VCC_PIN);
                delay_us(20);
#endif
                /* Подать 12 В на RESET */
                pin_clear(RST_PORT, RST_PIN);
                delay_us(10);

                /* Ждём, пока ATtiny выйдет в HVSP */
                delay_us(1000);

                /* Читаем сигнатуру */
                uint16_t sig = hvsp_readSignature();

                if ((sig & 0xFF) == 0x0B) { // ATtiny13 signature
                    hvsp_writeFuse(0x040); // пример: запись LFUSE
                    hvsp_writeFuse(0x142); // пример: запись HFUSE
                    led_blink_pattern(5, 100); // успех
                } else {
                    led_blink_pattern(2, 500); // ошибка
                }

                /* Отключить RESET */
                pin_set(RST_PORT, RST_PIN);

#if VCC_CONTROL_PIN_USED
                pin_clear(VCC_PORT, VCC_PIN);
#endif
            }
        }
    }
}
