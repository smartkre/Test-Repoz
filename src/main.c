#include "stm32f1xx.h"

void clock_init(void) {
    // 1. Включаем HSI
    RCC->CR |= RCC_CR_HSION;
    while (!(RCC->CR & RCC_CR_HSIRDY));

    // 2. Отключаем PLL
    RCC->CR &= ~RCC_CR_PLLON;
    while (RCC->CR & RCC_CR_PLLRDY);

    // 3. Сбрасываем настройки делителей
    RCC->CFGR = 0x00000000;

    // 4. Выбираем HSI как источник системной частоты
    RCC->CFGR |= RCC_CFGR_SW_HSI;
    while ((RCC->CFGR & RCC_CFGR_SWS) != RCC_CFGR_SWS_HSI);
}

void delay_ms(uint32_t ms) {
    // При SYSCLK = 8 МГц одна итерация ~1 мкс (цикл + оптимизация)
    // Подогнано для более точного результата
    for (uint32_t i = 0; i < ms * 800; i++) {
        __asm__("nop");
    }
}

int main(void) {
    clock_init();

    // Включаем тактирование портов
    RCC->APB2ENR |= RCC_APB2ENR_IOPCEN | RCC_APB2ENR_IOPBEN;

    // PC13 как выход push-pull (2 МГц)
    GPIOC->CRH &= ~(GPIO_CRH_MODE13 | GPIO_CRH_CNF13);
    GPIOC->CRH |= GPIO_CRH_MODE13_1;

    // PB0 как выход push-pull (50 МГц)
    GPIOB->CRL &= ~(GPIO_CRL_MODE0 | GPIO_CRL_CNF0);
    GPIOB->CRL |= GPIO_CRL_MODE0_1 | GPIO_CRL_MODE0_0;

    while (1) {
        // ---- PB0: генерация ~1 МГц ----
        GPIOB->ODR ^= (1 << 0);   // toggle PB0
        for (volatile int i = 0; i < 4; i++); // минимальная задержка

        // ---- PC13: мигание 10 Гц ----
        static uint32_t counter = 0;
        counter++;
        if (counter >= 40000) {   // при SYSCLK=8 МГц примерно 100 мс
            GPIOC->ODR ^= (1 << 13);
            counter = 0;
        }
    }
}
