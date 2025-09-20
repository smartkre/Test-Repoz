#include "stm32f1xx.h"

// Простая задержка в цикле
static void delay_ms(uint32_t ms) {
    for (uint32_t i = 0; i < ms * 8000; i++) {
        __asm__("nop");
    }
}

int main(void) {
    // Включаем тактирование GPIOB и GPIOC
    RCC->APB2ENR |= RCC_APB2ENR_IOPBEN | RCC_APB2ENR_IOPCEN;

    // PB0 - выход push-pull, 50 МГц
    GPIOB->CRL &= ~(GPIO_CRL_MODE0 | GPIO_CRL_CNF0);
    GPIOB->CRL |= GPIO_CRL_MODE0_1 | GPIO_CRL_MODE0_0; // 50 МГц, general purpose push-pull

    // PC13 - выход push-pull, 2 МГц (светодиод)
    GPIOC->CRH &= ~(GPIO_CRH_MODE13 | GPIO_CRH_CNF13);
    GPIOC->CRH |= GPIO_CRH_MODE13_1; // 2 МГц, push-pull

    // Настроим SysTick для точного тайминга
    SysTick->LOAD = 8000000 / 1000 - 1; // 1 мс при 8 МГц
    SysTick->VAL = 0;
    SysTick->CTRL = SysTick_CTRL_CLKSOURCE_Msk | SysTick_CTRL_ENABLE_Msk;

    while (1) {
        // ====== Генерация тестового сигнала на PB0 ======
        // Просто делаем делитель вручную: 8 МГц / 8 = 1 МГц
        for (int i = 0; i < 4; i++) {
            GPIOB->BSRR = GPIO_BSRR_BS0;   // PB0 = 1
            __asm__("nop"); __asm__("nop"); __asm__("nop"); __asm__("nop");
            GPIOB->BSRR = GPIO_BSRR_BR0;   // PB0 = 0
            __asm__("nop"); __asm__("nop"); __asm__("nop"); __asm__("nop");
        }

        // ====== Мигание светодиодом PC13 ======
        GPIOC->BSRR = GPIO_BSRR_BR13; // LED ON
        delay_ms(100);                // 100 мс
        GPIOC->BSRR = GPIO_BSRR_BS13; // LED OFF
        delay_ms(100);                // 100 мс
    }
}
