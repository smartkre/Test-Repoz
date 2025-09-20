#include "stm32f1xx.h"

#define TEST_PIN     (1U << 0)  // PB0
#define LED_PIN      (1U << 13)  // PC13

// --- Функции задержки с TIM2 ---
void TIM2_Init(void) {
    RCC->APB1ENR |= RCC_APB1ENR_TIM2EN;  // Включить тактирование TIM2
    TIM2->PSC = 293;                     // Предделитель: 8 МГц / (293 + 1) ≈ 1 МГц (1 мкс tick, корректировка на 6x)
    TIM2->ARR = 0xFFFF;                  // Максимальное значение авторелоада
    TIM2->CR1 = TIM_CR1_CEN;             // Включить таймер
}

void delay_us(uint32_t us) {
    TIM2->CNT = 0;                       // Сброс счетчика
    while (TIM2->CNT < us);              // Ждать указанное количество микросекунд
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
    GPIOB->CRL |= (0x1 << 0);            // Настраиваем PB0 как выход 10MHz push-pull

    // Настройка PC13 как выход push-pull 10MHz
    GPIOC->CRH &= ~(0xF << ((13 - 8) * 4));  // Очищаем настройки PC13
    GPIOC->CRH |= (0x1 << ((13 - 8) * 4));   // Настраиваем PC13 как выход 10MHz push-pull
}

int main(void) {
    GPIO_Init();
    TIM2_Init();  // Инициализация таймера после GPIO

    // Выключаем LED (PC13 высокий = выкл)
    GPIOC->BSRR = LED_PIN;

    // Бесконечный цикл с диагностикой
    while (1) {
        // Переключаем PB0 для измерения частоты
        GPIOB->BSRR = TEST_PIN;          // Устанавливаем PB0 в высокий уровень
        delay_us(100);                   // Задержка 100 микросекунд
        GPIOB->BRR = TEST_PIN;           // Устанавливаем PB0 в низкий уровень
        delay_us(100);                   // Задержка 100 микросекунд

        // Простая индикация через PC13 (мигание каждые 500 мс)
        GPIOC->BRR = LED_PIN;            // Включаем LED
        delay_ms(500);
        GPIOC->BSRR = LED_PIN;           // Выключаем LED
        delay_ms(500);
    }
}
