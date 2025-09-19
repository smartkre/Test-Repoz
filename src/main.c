#include "stm32f1xx.h"

#define LED_PIN     (1U << 13)  // PC13
#define BUTTON_PIN  (1U << 0)   // PA0

// --- Функции задержки с TIM2 ---
void TIM2_Init(void) {
    RCC->APB1ENR |= RCC_APB1ENR_TIM2EN;  // Включить тактирование TIM2
    TIM2->PSC = 7;                       // Предделитель: 8 МГц / (7 + 1) = 1 МГц (1 мкс tick)
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
    // Включаем тактирование GPIOA, GPIOB, GPIOC
    RCC->APB2ENR |= RCC_APB2ENR_IOPAEN | RCC_APB2ENR_IOPBEN | RCC_APB2ENR_IOPCEN;

    // Настройка PC13 как выход push-pull 10MHz
    GPIOC->CRH &= ~(0xF << ((13 - 8) * 4));
    GPIOC->CRH |= (0x1 << ((13 - 8) * 4));

    // Настройка PA0 как вход Floating
    GPIOA->CRL &= ~(0xF << (0 * 4));
    GPIOA->CRL |= (0x4 << (0 * 4)); // 0100: floating input
}

int main(void) {
    GPIO_Init();
    TIM2_Init();  // Инициализация таймера после GPIO

    // Выключим LED (PC13 высокий = выкл)
    GPIOC->BSRR = LED_PIN;
    delay_ms(50);

    // --- Индикация старта (3 мигания) ---
    for (uint32_t cycle = 0; cycle < 3; cycle++) {
        GPIOC->BRR = LED_PIN;  // On
        delay_ms(300);
        GPIOC->BSRR = LED_PIN; // Off
        delay_ms(200);
    }

    // --- Ждём отпускание кнопки PA0 ---
    while ((GPIOA->IDR & BUTTON_PIN) != 0) {
        // Мигаем быстро, пока кнопка нажата
        GPIOC->BRR = LED_PIN;
        delay_ms(100);
        GPIOC->BSRR = LED_PIN;
        delay_ms(100);
    }

    // --- Основной цикл ---
    while (1) {
        // Моргание LED каждые 500 мс
        GPIOC->BRR = LED_PIN;   // On
        delay_ms(500);
        GPIOC->BSRR = LED_PIN;  // Off
        delay_ms(500);
    }
}
