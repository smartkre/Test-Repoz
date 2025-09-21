#include "stm32f1xx.h"

#define TEST_PIN     (1U << 0)  // PB0 (PA0 для TIM2_CH1)

// --- Инициализация тактирования с HSE ---
void System_Init(void) {
    RCC->CR |= RCC_CR_HSEON;
    while (!(RCC->CR & RCC_CR_HSERDY));
    RCC->CFGR &= ~RCC_CFGR_SW;
    RCC->CFGR |= RCC_CFGR_SW_HSE;
    while ((RCC->CFGR & RCC_CFGR_SWS) != RCC_CFGR_SWS_HSE);
    RCC->CFGR &= ~(RCC_CFGR_HPRE | RCC_CFGR_PPRE1 | RCC_CFGR_PPRE2);
    FLASH->ACR |= FLASH_ACR_LATENCY_0;
    RCC->APB2ENR |= RCC_APB2ENR_IOPAEN | RCC_APB1ENR_TIM2EN;
    GPIOA->CRL &= ~(0xF << 0);  // PA0 как выход альтернативной функции
    GPIOA->CRL |= (0xB << 0);   // AF push-pull 10MHz
}

// --- Задержка ---
void delay_ms(uint32_t ms) {
    for (volatile uint32_t t = 0; t < ms; t++)
        for (volatile uint32_t i = 0; i < 8000; i++) __asm__("nop");
}

// --- Настройка TIM2 для ШИМ ---
void TIM2_Init(void) {
    TIM2->PSC = 9;           // Делитель 10
    TIM2->ARR = 159;         // Период для ~5 кГц
    TIM2->CCR1 = 80;         // Скважность ~50%
    TIM2->CCMR1 |= TIM_CCMR1_OC1PE | TIM_CCMR1_OC1M_1 | TIM_CCMR1_OC1M_2;  // PWM Mode 1
    TIM2->CCER |= TIM_CCER_CC1E;  // Включение канала 1
    TIM2->CR1 |= TIM_CR1_CEN;     // Включение таймера
}

int main(void) {
    System_Init();
    TIM2_Init();

    while (1) {
        // Динамическая смена скважности
        for (uint32_t i = 100; i <= 300; i += 100) {
            TIM2->CCR1 = i;
            delay_ms(5000);  // 5 секунд
        }
    }
}
