#include "stm32f1xx.h"

// Определение выводов
#define HVSP_RST_PORT GPIOB
#define HVSP_RST_PIN  (1 << 0)  // PB0

#define HVSP_SCI_PORT GPIOB
#define HVSP_SCI_PIN  (1 << 4)  // PB4

#define HVSP_SDO_PORT GPIOB
#define HVSP_SDO_PIN  (1 << 1)  // PB1

#define HVSP_SII_PORT GPIOA
#define HVSP_SII_PIN  (1 << 6)  // PA6

#define HVSP_SDI_PORT GPIOA
#define HVSP_SDI_PIN  (1 << 7)  // PA7

#define LED_PORT GPIOC
#define LED_PIN  (1 << 13)      // PC13

void Delay(uint32_t count) {
    for(volatile uint32_t i = 0; i < count; i++);
}

void GPIO_Init(void) {
    // Включение тактирования портов
    RCC->APB2ENR |= RCC_APB2ENR_IOPAEN | RCC_APB2ENR_IOPBEN | RCC_APB2ENR_IOPCEN;
    Delay(1000);
    
    // Настройка LED (PC13)
    GPIOC->CRH &= ~(GPIO_CRH_MODE13 | GPIO_CRH_CNF13);
    GPIOC->CRH |= GPIO_CRH_MODE13_0; // Output mode, 10 MHz
    GPIOC->CRH |= GPIO_CRH_CNF13_0;  // Open-drain
    
    // Настройка HVSP выводов
    // RST (PB0)
    GPIOB->CRL &= ~(GPIO_CRL_MODE0 | GPIO_CRL_CNF0);
    GPIOB->CRL |= GPIO_CRL_MODE0;     // Output mode, 50 MHz
    
    // SCI (PB4)
    GPIOB->CRL &= ~(GPIO_CRL_MODE4 | GPIO_CRL_CNF4);
    GPIOB->CRL |= GPIO_CRL_MODE4;     // Output mode, 50 MHz
    
    // SDO (PB1)
    GPIOB->CRL &= ~(GPIO_CRL_MODE1 | GPIO_CRL_CNF1);
    GPIOB->CRL |= GPIO_CRL_MODE1;     // Output mode, 50 MHz
    
    // SII (PA6)
    GPIOA->CRL &= ~(GPIO_CRL_MODE6 | GPIO_CRL_CNF6);
    GPIOA->CRL |= GPIO_CRL_MODE6;     // Output mode, 50 MHz
    
    // SDI (PA7)
    GPIOA->CRL &= ~(GPIO_CRL_MODE7 | GPIO_CRL_CNF7);
    GPIOA->CRL |= GPIO_CRL_MODE7;     // Output mode, 50 MHz
    
    // Изначально все линии в LOW
    HVSP_RST_PORT->BRR = HVSP_RST_PIN;
    HVSP_SCI_PORT->BRR = HVSP_SCI_PIN;
    HVSP_SII_PORT->BRR = HVSP_SII_PIN;
    HVSP_SDI_PORT->BRR = HVSP_SDI_PIN;
    HVSP_SDO_PORT->BRR = HVSP_SDO_PIN;
}

int main(void) {
    GPIO_Init();
    
    // Мигаем 3 раза для индикации старта
    for(int i = 0; i < 3; i++) {
        LED_PORT->BRR = LED_PIN; // LED ON
        Delay(500000);
        LED_PORT->BSRR = LED_PIN; // LED OFF
        Delay(500000);
    }
    
    uint8_t test_phase = 0;
    
    while(1) {
        // Поочередно тестируем каждый пин
        switch(test_phase) {
            case 0: // Тест SCI (PB4) - 1 kHz
                HVSP_SCI_PORT->ODR ^= HVSP_SCI_PIN;
                Delay(500); // ~1 kHz
                break;
                
            case 1: // Тест SII (PA6) - 500 Hz
                HVSP_SII_PORT->ODR ^= HVSP_SII_PIN;
                Delay(1000);
                break;
                
            case 2: // Тест SDI (PA7) - 250 Hz  
                HVSP_SDI_PORT->ODR ^= HVSP_SDI_PIN;
                Delay(2000);
                break;
                
            case 3: // Тест RST (PB0) - 100 Hz
                HVSP_RST_PORT->ODR ^= HVSP_RST_PIN;
                Delay(5000);
                break;
                
            case 4: // Тест SDO (PB1) - 200 Hz
                HVSP_SDO_PORT->ODR ^= HVSP_SDO_PIN;
                Delay(2500);
                break;
        }
        
        // Переключаем фазу каждые 2 секунды
        static uint32_t counter = 0;
        if(counter++ > 2000000) {
            counter = 0;
            test_phase = (test_phase + 1) % 5;
            
            // Мигаем LED при смене фазы
            LED_PORT->BRR = LED_PIN;
            Delay(100000);
            LED_PORT->BSRR = LED_PIN;
        }
    }
}
