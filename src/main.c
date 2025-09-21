#include "stm32f1xx.h"

#define LED_PORT GPIOC
#define LED_PIN  (1 << 13)

// Простейшая задержка
void Delay(uint32_t count) {
    for(volatile uint32_t i = 0; i < count; i++);
}

int main(void) {
    // Включение тактирования порта C
    RCC->APB2ENR |= RCC_APB2ENR_IOPCEN;
    
    // Настройка PC13 как выхода (Open-Drain)
    GPIOC->CRH &= ~(GPIO_CRH_MODE13 | GPIO_CRH_CNF13);
    GPIOC->CRH |= GPIO_CRH_MODE13_0; // Output mode, 10 MHz
    GPIOC->CRH |= GPIO_CRH_CNF13_0;  // Open-drain
    
    // Бесконечное мигание
    while(1) {
        LED_PORT->BSRR = LED_PIN; // LED OFF
        Delay(1000000);
        LED_PORT->BRR = LED_PIN;  // LED ON
        Delay(1000000);
    }
}
