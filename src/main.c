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

    // Сброс делителей
    RCC->CFGR &= ~(RCC_CFGR_HPRE | RCC_CFGR_PPRE1 | RCC_CFGR_PPRE2);

    // Настройка Flash Latency (0WS для 0-24 МГц)
    FLASH->ACR &= ~FLASH_ACR_LATENCY;
    FLASH->ACR |= FLASH_ACR_LATENCY_0;

    // Включаем тактирование GPIOA, AFIO и TIM2
    RCC->APB2ENR |= RCC_APB2ENR_IOPAEN | RCC_APB2ENR_AFIOEN;
    RCC->APB1ENR |= RCC_APB1ENR_TIM2EN;
}

// --- Функция задержки с учетом текущей частоты ---
void delay_ms(uint32_t ms, uint32_t hclk) {
    uint32_t cycles_per_ms = hclk / 8000;  // Адаптация под текущую частоту
    for (volatile uint32_t t = 0; t < ms; t++) {
        for (volatile uint32_t i = 0; i < cycles_per_ms; i++) {
            __asm__("nop");
        }
    }
}

// --- Настройка TIM2 для ШИМ на PA2 (TIM2_CH3) с динамической частотой ---
void TIM2_Init(uint32_t frequency, uint32_t hclk) {
    // Настройка PA2 как AF push-pull (для TIM2_CH3)
    GPIOA->CRL &= ~(0xF << (2 * 4));  // Очищаем PA2
    GPIOA->CRL |= (0xB << (2 * 4));   // AF push-pull 10MHz

    // Учет PCLK1 (APB1) — делится на 2, если HCLK > 36 МГц
    uint32_t pclk1 = (hclk > 36000000) ? (hclk / 2) : hclk;
    uint32_t prescaler = 0;
    uint32_t period = (pclk1 / frequency) - 1;

    if (period < 1) period = 1;  // Минимальный период = 1
    if (period > 0xFFFF) {  // Если период выходит за пределы 16 бит
        prescaler = (period / 0xFFFF) + 1;
        period = (pclk1 / (prescaler + 1) / frequency) - 1;
        if (period < 1) period = 1;
    }

    // Настройка PLL для частот выше 8 МГц
    if (frequency > 8000000 && hclk == 8000000) {
        uint32_t pll_mul = (frequency * 2 / 8000000);  // Примерное умножение
        if (pll_mul > 9) pll_mul = 9;  // Максимум 9x (72 МГц)
        RCC->CFGR &= ~RCC_CFGR_PLLMULL;
        RCC->CFGR |= (pll_mul - 2) << 18;  // Установка умножения (2-9)
        RCC->CFGR |= (1 << 16);            // Источник PLL — HSE
        RCC->CR |= RCC_CR_PLLON;
        while (!(RCC->CR & RCC_CR_PLLRDY));
        RCC->CFGR &= ~RCC_CFGR_SW;
        RCC->CFGR |= RCC_CFGR_SW_PLL;
        while ((RCC->CFGR & RCC_CFGR_SWS) != RCC_CFGR_SWS_PLL);
        hclk = 8000000 * pll_mul;
        if (hclk > 72000000) hclk = 72000000;
        // Обновление Flash Latency
        if (hclk > 48000000) FLASH->ACR |= FLASH_ACR_LATENCY_2;
        else if (hclk > 24000000) FLASH->ACR |= FLASH_ACR_LATENCY_1;
        else FLASH->ACR |= FLASH_ACR_LATENCY_0;
        pclk1 = (hclk > 36000000) ? (hclk / 2) : hclk;
    }

    // Пересчет для новой частоты с учетом PCLK1
    prescaler = 0;
    period = (pclk1 / frequency) - 1;
    if (period < 1) period = 1;
    if (period > 0xFFFF) {
        prescaler = (period / 0xFFFF) + 1;
        period = (pclk1 / (prescaler + 1) / frequency) - 1;
        if (period < 1) period = 1;
    }

    TIM2->PSC = prescaler;
    TIM2->ARR = period;
    TIM2->CCR3 = period / 2;  // Скважность 50%
    TIM2->CCMR2 = (TIM2->CCMR2 & ~TIM_CCMR2_OC3M) | TIM_CCMR2_OC3M_1 | TIM_CCMR2_OC3M_2;  // PWM Mode 1
    TIM2->CCMR2 |= TIM_CCMR2_OC3PE;  // Включение предзагрузки
    TIM2->CCER |= TIM_CCER_CC3E;  // Включение канала 3
    TIM2->CR1 |= TIM_CR1_CEN;  // Включить таймер
}

int main(void) {
    System_Init();

    // Увеличение частоты от 1 МГц до 70 МГц с шагом 1 МГц каждую секунду
    uint32_t hclk = 8000000;  // Начальная частота HCLK
    for (uint32_t freq = 1000000; freq <= 70000000; freq += 1000000) {
        TIM2_Init(freq, hclk);
        delay_ms(1000, hclk);  // Задержка 1 секунда с учетом HCLK
        // Обновляем hclk только если PLL активен
        if (freq > 8000000) {
            uint32_t pll_mul = (freq * 2 / 8000000);
            if (pll_mul > 9) pll_mul = 9;
            hclk = 8000000 * pll_mul;
            if (hclk > 72000000) hclk = 72000000;
        }
    }

    while (1) {
        // После достижения 70 МГц цикл останавливается
    }
}
