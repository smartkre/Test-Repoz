#include "stm32f1xx.h"

void delay_us(uint32_t us) {
    // Задержка в микросекундах при SYSCLK=8 МГц
    // 1 такт ~0.125 мкс → нужно 8 тактов на 1 мкс
    for(uint32_t i = 0; i < us * 8; i++) {
        __asm__("nop");
    }
}

int main(void) {
    // --- Настройка тактирования ---
    RCC->CR |= RCC_CR_HSION;                       // включаем HSI
    while ((RCC->CR & RCC_CR_HSIRDY) == 0);        // ждём готовности

    RCC->CFGR &= ~RCC_CFGR_SW;                     // сброс выбора источника
    RCC->CFGR |= RCC_CFGR_SW_HSI;                  // выбираем HSI = 8 МГц
    while ((RCC->CFGR & RCC_CFGR_SWS) != RCC_CFGR_SWS_HSI);

    RCC->CFGR &= ~RCC_CFGR_HPRE;                   // AHB = /1
    RCC->CFGR &= ~RCC_CFGR_PPRE1;                  // APB1 = /1
    RCC->CFGR &= ~RCC_CFGR_PPRE2;                  // APB2 = /1

    // --- Настройка портов ---
    RCC->APB2ENR |= RCC_APB2ENR_IOPCEN;            // тактирование GPIOC
    RCC->APB2ENR |= RCC_APB2ENR_IOPBEN;            // тактирование GPIOB

    // PC13 как выход push-pull
    GPIOC->CRH &= ~(GPIO_CRH_MODE13 | GPIO_CRH_CNF13);
    GPIOC->CRH |= GPIO_CRH_MODE13_0;               // выход 10 МГц, PP

    // PB0 как выход push-pull
    GPIOB->CRL &= ~(GPIO_CRL_MODE0 | GPIO_CRL_CNF0);
    GPIOB->CRL |= GPIO_CRL_MODE0_1;                // выход 2 МГц, PP

    // --- Основной цикл ---
    while (1) {
        // Генерация меандра 1 МГц на PB0
        GPIOB->BSRR = GPIO_BSRR_BS0;  // PB0 = 1
        __asm__("nop\nnop\nnop\nnop"); // задержка ~0.5 мкс
        GPIOB->BSRR = GPIO_BSRR_BR0;  // PB0 = 0
        __asm__("nop\nnop\nnop\nnop"); // задержка ~0.5 мкс

        // Моргание светодиода на PC13 с частотой 10 Гц
        static uint32_t counter = 0;
        counter++;
        if (counter >= 4000) {        // ~0.05 сек при 8 МГц
            GPIOC->ODR ^= (1 << 13);  // инверсия PC13
            counter = 0;
        }
    }
}
