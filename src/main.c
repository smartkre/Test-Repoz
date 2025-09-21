#include "stm32f1xx.h"

// --- Инициализация тактирования с внешним HSE (8 МГц) и отключением LSE ---
void System_Init(void) {
    // Включение HSI для стабильного старта
    RCC->CR |= RCC_CR_HSION;
    while (!(RCC->CR & RCC_CR_HSIRDY));

    // Включение Backup Domain для LSE
    RCC->APB1ENR |= RCC_APB1ENR_BKPEN | RCC_APB1ENR_PWREN;
    // Отключение LSE (32.768 кГц)
    RCC->BDCR &= ~RCC_BDCR_LSEON;
    while (RCC->BDCR & RCC_BDCR_LSERDY);

    // Включение HSE (внешний кварц 8 МГц)
    RCC->CR |= RCC_CR_HSEON;
    while (!(RCC->CR & RCC_CR_HSERDY));

    // Отключение PLL
    RCC->CR &= ~RCC_CR_PLLON;
    while (RCC->CR & RCC_CR_PLLRDY);

    // Переключение SYSCLK на HSE
    RCC->CFGR &= ~RCC_CFGR_SW;
    RCC->CFGR |= RCC_CFGR_SW_HSE;
    while ((RCC->CFGR & RCC_CFGR_SWS) != RCC_CFGR_SWS_HSE);

    // Настройка PLL (8 МГц * 9 = 72 МГц)
    RCC->CFGR &= ~RCC_CFGR_PLLMULL;
    RCC->CFGR |= RCC_CFGR_PLLMULL9;
    RCC->CFGR |= (1 << 16);  // Источник PLL — HSE
    RCC->CR |= RCC_CR_PLLON;
    while (!(RCC->CR & RCC_CR_PLLRDY));
    RCC->CFGR &= ~RCC_CFGR_SW;
    RCC->CFGR |= RCC_CFGR_SW_PLL;
    while ((RCC->CFGR & RCC_CFGR_SWS) != RCC_CFGR_SWS_PLL);

    // Сброс делителей
    RCC->CFGR &= ~(RCC_CFGR_HPRE | RCC_CFGR_PPRE1 | RCC_CFGR_PPRE2);

    // Настройка Flash Latency (2WS для 48-72 МГц)
    FLASH->ACR &= ~FLASH_ACR_LATENCY;
    FLASH->ACR |= FLASH_ACR_LATENCY_2;

    // Включаем тактирование GPIOA, AFIO и TIM2
    RCC->APB2ENR |= RCC_APB2ENR_IOPAEN | RCC_APB2ENR_AFIOEN;
    RCC->APB1ENR |= RCC_APB1ENR_TIM2EN;
}

// --- Настройка TIM2 для ШИМ на PA2 (TIM2_CH3, 10 МГц, 50%) ---
void TIM2_Init(void) {
    // Настройка PA2 как AF push-pull (для TIM2_CH3)
    GPIOA->CRL &= ~(0xF << (2 * 4));  // Очищаем PA2 (сдвиг 2)
    GPIOA->CRL |= (0xB << (2 * 4));   // AF push-pull 10MHz

    // Настройка TIM2
    // PCLK1 = 36 МГц (HCLK / 2 при 72 МГц)
    uint32_t pclk1 = 36000000;
    uint32_t target_freq = 10000000;  // Целевая частота 10 МГц
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
    TIM2->CCMR2 = (TIM2->CCMR2 & ~TIM_CCMR2_OC3M) | TIM_CCMR2_OC3M_1 | TIM_CCMR2_OC3M_2;  // PWM Mode 1
    TIM2->CCMR2 |= TIM_CCMR2_OC3PE;  // Включение предзагрузки
    TIM2->CCER |= TIM_CCER_CC3E;  // Включение канала 3
    TIM2->CR1 |= TIM_CR1_CEN;  // Включить таймер
}

int main(void) {
    System_Init();
    TIM2_Init();  // Генерация ШИМ на PA2

    while (1) {
        // Ничего не делать, ШИМ работает автоматически
    }
}
