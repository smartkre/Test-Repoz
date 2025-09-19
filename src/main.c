#include "stm32f1xx.h"

#define LED_PIN     (1U << 13)  // PC13
#define BUTTON_PIN  (1U << 0)   // PA0

extern uint32_t SystemCoreClock;  // частота ядра, задаётся в system_stm32f1xx.c

// --- Функции задержки ---
void delay_ms(uint32_t ms) {
    uint32_t cycles_per_ms = SystemCoreClock / 1000 / 4; // делим, чтобы цикл ≈1мс
    for (uint32_t ms_count = 0; ms_count < ms; ms_count++) {
        for (volatile uint32_t i = 0; i < cycles_per_ms; i++) {
            __asm__("nop");
        }
    }
}

void delay_us(uint32_t us) {
    uint32_t cycles_per_us = SystemCoreClock / 1000000 / 4;
    for (uint32_t us_count = 0; us_count < us; us_count++) {
        for (volatile uint32_t i = 0; i < cycles_per_us; i++) {
            __asm__("nop");
        }
    }
}

// --- Инициализация GPIO ---
void GPIO_Init(void) {
    // включаем тактирование GPIOA, GPIOB, GPIOC
    RCC->APB2ENR |= RCC_APB2ENR_IOPAEN | RCC_APB2ENR_IOPBEN | RCC_APB2ENR_IOPCEN;

    // Настройка PC13 как выход push-pull 10MHz
    GPIOC->CRH &= ~(0xF << ((13 - 8) * 4));
    GPIOC->CRH |=  (0x1 << ((13 - 8) * 4));

    // Настройка PA0 как вход (Floating input)
    GPIOA->CRL &= ~(0xF << (0 * 4));
    GPIOA->CRL |=  (0x4 << (0 * 4)); // 0100: floating input
}

int main(void) {
    GPIO_Init();

    // выключим LED (PC13 высокий = выкл)
    GPIOC->BSRR = LED_PIN;
    delay_ms(50);

    // --- Индикация старта (мигаем 3 раза) ---
    for (uint32_t cycle = 0; cycle < 3; cycle++) {
        GPIOC->BRR = LED_PIN;  // On
        delay_ms(300);
        GPIOC->BSRR = LED_PIN; // Off
        delay_ms(100);
    }

    // --- Ждём отпускание кнопки PA0 ---
    while ((GPIOA->IDR & BUTTON_PIN) != 0) {
        // мигаем быстро, пока кнопка нажата
        GPIOC->BRR = LED_PIN;
        delay_ms(100);
        GPIOC->BSRR = LED_PIN;
        delay_ms(100);
    }

    // --- Основной цикл ---
    while (1) {
        // моргание LED каждые 500 мс
        GPIOC->BRR = LED_PIN;   // On
        delay_ms(500);
        GPIOC->BSRR = LED_PIN;  // Off
        delay_ms(500);
    }
}
