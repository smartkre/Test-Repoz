#include "stm32f1xx.h"

#define TEST_PIN     (1U << 0)  // PB0
#define LED_PIN      (1U << 13)  // PC13

// --- Функции задержки с TIM2 ---
void TIM2_Init(void) {
    RCC->APB1ENR |= RCC_APB1ENR_TIM2EN;  // Включить тактирование TIM2
    TIM2->PSC = 399;                     // Предделитель: 8 МГц / (399 + 1) ≈ 20 кГц (50 мкс tick, корректировка на 6.5x)
    TIM2->ARR = 0xFFFF;                  // Максимальное значение авторелоада
    TIM2->CR1 = TIM_CR1_CEN;             // Включить таймер
}

void delay_us(uint32_t us) {
    TIM2->CNT = 0;                       // Сброс счетчика
    while (TIM2->CNT < (us / 50));       // Ждать, преобразуя us в тики (1 тик = 50 мкс)
}

void delay_ms(uint32_t ms) {
    for (uint32_t i = 0; i < ms; i++) {
        delay_us(1000);                  // 1 мс = 1000 мкс
    }
}

// --- Инициализация GPIO ---
void GPIO_Init(void) {
    // Включаем тактирование GPIOB и GPIOC
    RCC->APB2ENR |= RCC_APB2ENR_IOPBEN | RCC_APB2ENR_IOPCEN;

    // Настройка PB0 как выход push-pull 10MHz
    GPIOB->CRL &= ~(0xF << 0);           // Очищаем настройки PB0
    GPIOB->CRL |= (GPIO_CRL_MODE0_0 | GPIO_CRL_MODE0_1);  // 10MHz push-pull

    // Настройка PC13 как выход push-pull 10MHz
    GPIOC->CRH &= ~(0xF << ((13 - 8) * 4));  // Очищаем настройки PC13
    GPIOC->CRH |= (GPIO_CRH_MODE13_0 | GPIO_CRH_MODE13_1);  // 10MHz push-pull
}

int main(void) {
    GPIO_Init();
    TIM2_Init();  // Инициализация таймера после GPIO

    // Выключаем LED (PC13 высокий = выкл)
    GPIOC->BSRR = LED_PIN;

    // Бесконечный цикл с диагностикой
    while (1) {
        // Тест с таймером (PB0)
        GPIOB->BSRR = TEST_PIN;          // Устанавливаем PB0 в высокий уровень
        delay_us(100);                   // Задержка 100 мкс (2 тика по 50 мкс)
        GPIOB->BRR = TEST_PIN;           // Устанавливаем PB0 в низкий уровень
        delay_us(100);                   // Задержка 100 мкс

        // Тест без таймера (для проверки базового тактирования)
        GPIOB->BSRR = TEST_PIN;          // High
        for (volatile uint32_t i = 0; i < 10000; i++);  // Простая задержка
        GPIOB->BRR = TEST_PIN;           // Low
        for (volatile uint32_t i = 0; i < 10000; i++);  // Простая задержка

        // Индикация через PC13
        GPIOC->BRR = LED_PIN;            // Включаем LED
        delay_ms(500);                   // Задержка 500 мс
        GPIOC->BSRR = LED_PIN;           // Выключаем LED
        delay_ms(500);                   // Задержка 500 мс
    }
}
