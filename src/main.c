#define STM32F103x6
#include "stm32f1xx.h"

#define LED_PORT GPIOC
#define LED_PIN  (1 << 13)   // PC13

#define SCI_PORT GPIOB
#define SCI_PIN  (1 << 4)    // PB4

static void delay(volatile uint32_t d) {
    while (d--) __asm__("nop");
}

int main(void) {
    /* Включаем тактирование портов B и C */
    RCC->APB2ENR |= RCC_APB2ENR_IOPBEN | RCC_APB2ENR_IOPCEN;

    /* Настраиваем PC13 как push-pull output (LED) */
    GPIOC->CRH &= ~(0xF << ((13 - 8) * 4));
    GPIOC->CRH |=  (0x1 << ((13 - 8) * 4)); // 0b0001 = Output, max 10 MHz, push-pull

    /* Настраиваем PB4 как push-pull output */
    GPIOB->CRL &= ~(0xF << (4 * 4));
    GPIOB->CRL |=  (0x3 << (4 * 4));        // 0b0011 = Output, max 50 MHz, push-pull

    /* Генерируем 1000 импульсов на PB4 */
    for (int i = 0; i < 1000; i++) {
        GPIOB->BSRR = SCI_PIN;  // high
        delay(100);
        GPIOB->BRR  = SCI_PIN;  // low
        delay(100);
    }

    /* Основной цикл: мигаем светодиодом */
    while (1) {
        GPIOC->BRR  = LED_PIN;  // LED ON (PC13 низкий уровень)
        delay(500000);
        GPIOC->BSRR = LED_PIN;  // LED OFF (PC13 высокий уровень)
        delay(500000);
    }
}
