#include "stm32f1xx.h"

#define TEST_PIN     (1U << 0)  // PB0
#define LED_PIN      (1U << 13)  // PC13

// --- Функции задержки на loop (с корректировкой на ускорение) ---
void delay_ms(uint32_t ms) {
    for (volatile uint32_t t = 0; t < ms; t++) {
        for (volatile uint32_t i = 0; i < 1333; i++) {  // Уменьшено с 8000 до 1333 (корректировка на 6x ускорение)
            __asm__("nop");
        }
    }
}

void delay_us(uint32_t us) {
    for (volatile uint32_t t = 0; t < us; t++) {
        for (volatile uint32_t i = 0; i < 1; i++) {  // Уменьшено для корректировки
            __asm__("nop");
        }
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
