#include "stm32f1xx.h"

// --- Инициализация тактирования с внешним HSE (8 МГц) и отключением LSE ---
void System_Init(void) {
    RCC->CR |= RCC_CR_HSION;
    while (!(RCC->CR & RCC_CR_HSIRDY));

    RCC->APB1ENR |= RCC_APB1ENR_BKPEN | RCC_APB1ENR_PWREN;
    RCC->BDCR &= ~RCC_BDCR_LSEON;
    while (RCC->BDCR & RCC_BDCR_LSERDY);

    RCC->CR |= RCC_CR_HSEON;
    while (!(RCC->CR & RCC_CR_HSERDY));

    RCC->CR &= ~RCC_CR_PLLON;
    while (RCC->CR & RCC_CR_PLLRDY);

    RCC->CFGR &= ~RCC_CFGR_SW;
    RCC->CFGR |= RCC_CFGR_SW_HSE;
    while ((RCC->CFGR & RCC_CFGR_SWS) != RCC_CFGR_SWS_HSE);

    RCC->CFGR &= ~RCC_CFGR_PLLMULL;
    RCC->CFGR |= RCC_CFGR_PLLMULL9;  // 8 МГц * 9 = 72 МГц
    RCC->CFGR |= (1 << 16);          // Источник PLL — HSE
    RCC->CR |= RCC_CR_PLLON;
    while (!(RCC->CR & RCC_CR_PLLRDY));
    RCC->CFGR &= ~RCC_CFGR_SW;
    RCC->CFGR |= RCC_CFGR_SW_PLL;
    while ((RCC->CFGR & RCC_CFGR_SWS) != RCC_CFGR_SWS_PLL);

    RCC->CFGR &= ~(RCC_CFGR_HPRE | RCC_CFGR_PPRE1 | RCC_CFGR_PPRE2);

    FLASH->ACR &= ~FLASH_ACR_LATENCY;
    FLASH->ACR |= FLASH_ACR_LATENCY_2;

    RCC->APB2ENR |= RCC_APB2ENR_IOPAEN | RCC_APB2ENR_AFIOEN;
    RCC->APB1ENR |= RCC_APB1ENR_TIM2EN;
}

// --- Настройка TIM2 для ШИМ на PA2 (TIM2_CH3) с фиксированной частотой ~18 МГц ---
void TIM2_Init(void) {
    GPIOA->CRL &= ~(0xF << (2 * 4));  // Очищаем PA2
    GPIOA->CRL |= (0xB << (2 * 4));   // AF push-pull 10MHz

    uint32_t pclk1 = 36000000;  // PCLK1 = 36 МГц (HCLK / 2)
    uint32_t target_freq = 18000000;  // Целевая частота 18 МГц
    uint32_t prescaler = 0;
    uint32_t period = (pclk1 / target_freq) - 1;

    if (period < 1) period = 1;  // Минимальный период = 1
    if (period > 0xFFFF) {
        prescaler = (period / 0xFFFF) + 1;
        period = (pclk1 / (prescaler + 1) / target_freq) - 1;
        if (period < 1) period = 1;
    }

    TIM2->PSC = prescaler;
    TIM2->ARR = period;
    TIM2->CCR3 = period / 2;  // 50% скважность
    TIM2->CCMR2 = (TIM2->CCMR2 & ~TIM_CCMR2_OC3M) | TIM_CCMR2_OC3M_1 | TIM_CCMR2_OC3M_2;
    TIM2->CCMR2 |= TIM_CCMR2_OC3PE;
    TIM2->CCER |= TIM_CCER_CC3E;
    TIM2->CR1 |= TIM_CR1_CEN;
}

int main(void) {
    System_Init();
    TIM2_Init();  // Установка фиксированной частоты ~18 МГц на PA2

    while (1) {
        // Бесконечный цикл, ШИМ работает статично
    }
}
