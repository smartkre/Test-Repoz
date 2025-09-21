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

void System_Init(void) {
    // Включение HSI
    RCC->CR |= RCC_CR_HSION;
    while ((RCC->CR & RCC_CR_HSIRDY) == 0);
    
    // Включение тактирования портов, таймеров и AFIO
    RCC->APB2ENR |= RCC_APB2ENR_IOPAEN | RCC_APB2ENR_IOPBEN | RCC_APB2ENR_IOPCEN | RCC_APB2ENR_AFIOEN;
    RCC->APB1ENR |= RCC_APB1ENR_TIM3EN;
}

void GPIO_Init(void) {
    // Настройка LED (PC13)
    GPIOC->CRH &= ~(GPIO_CRH_MODE13 | GPIO_CRH_CNF13);
    GPIOC->CRH |= GPIO_CRH_MODE13_0; // Output mode, 10 MHz
    GPIOC->CRH |= GPIO_CRH_CNF13_0;  // Open-drain
    
    // Настройка SCI (PB4) как Alternate Function Push-Pull для TIM3_CH1
    GPIOB->CRL &= ~(GPIO_CRL_MODE4 | GPIO_CRL_CNF4);
    GPIOB->CRL |= GPIO_CRL_MODE4_0 | GPIO_CRL_MODE4_1; // Output mode, 50 MHz
    GPIOB->CRL |= GPIO_CRL_CNF4_1; // Alternate function output Push-pull
    
    // Настройка остальных HVSP выводов как обычных выходов
    // RST (PB0)
    GPIOB->CRL &= ~(GPIO_CRL_MODE0 | GPIO_CRL_CNF0);
    GPIOB->CRL |= GPIO_CRL_MODE0;     // Output mode, 50 MHz
    
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
    HVSP_SII_PORT->BRR = HVSP_SII_PIN;
    HVSP_SDI_PORT->BRR = HVSP_SDI_PIN;
    HVSP_SDO_PORT->BRR = HVSP_SDO_PIN;
}

void TIM3_Init(void) {
    // Остановка таймера
    TIM3->CR1 &= ~TIM_CR1_CEN;
    
    // Сброс таймера
    TIM3->CR1 = 0;
    TIM3->CR2 = 0;
    TIM3->PSC = 0;
    TIM3->ARR = 0;
    TIM3->CCR1 = 0;
    
    // Настройка таймера для генерации 1 MHz
    TIM3->PSC = 7;          // Делитель: 8 MHz / (7 + 1) = 1 MHz
    TIM3->ARR = 1;          // Период: 2 такта (1 MHz / 2 = 500 kHz)
    TIM3->CCR1 = 1;         // Скважность 50% (1/2)
    
    // Настройка канала 1 в PWM mode 1
    TIM3->CCMR1 &= ~TIM_CCMR1_OC1M;
    TIM3->CCMR1 |= (6 << 4); // PWM mode 1 (OC1M = 110)
    TIM3->CCMR1 |= TIM_CCMR1_OC1PE; // Enable preload
    
    // Включение канала 1
    TIM3->CCER |= TIM_CCER_CC1E;
    
    // Включение таймера
    TIM3->CR1 |= TIM_CR1_CEN;
}

// Простая задержка
void Delay(uint32_t count) {
    for(volatile uint32_t i = 0; i < count; i++);
}

int main(void) {
    System_Init();
    GPIO_Init();
    TIM3_Init();
    
    // Мигаем 3 раза для индикации старта
    for(int i = 0; i < 3; i++) {
        LED_PORT->BRR = LED_PIN; // LED ON
        Delay(500000);
        LED_PORT->BSRR = LED_PIN; // LED OFF
        Delay(500000);
    }
    
    // Простой тест - включаем все пины по очереди на 2 секунды
    while(1) {
        // Включаем RST (PB0) - постоянный HIGH
        HVSP_RST_PORT->BSRR = HVSP_RST_PIN;
        Delay(2000000);
        HVSP_RST_PORT->BRR = HVSP_RST_PIN;
        Delay(100000);
        
        // Включаем SII (PA6) - постоянный HIGH
        HVSP_SII_PORT->BSRR = HVSP_SII_PIN;
        Delay(2000000);
        HVSP_SII_PORT->BRR = HVSP_SII_PIN;
        Delay(100000);
        
        // Включаем SDI (PA7) - постоянный HIGH
        HVSP_SDI_PORT->BSRR = HVSP_SDI_PIN;
        Delay(2000000);
        HVSP_SDI_PORT->BRR = HVSP_SDI_PIN;
        Delay(100000);
        
        // Включаем SDO (PB1) - постоянный HIGH
        HVSP_SDO_PORT->BSRR = HVSP_SDO_PIN;
        Delay(2000000);
        HVSP_SDO_PORT->BRR = HVSP_SDO_PIN;
        Delay(100000);
        
        // Мигаем LED после цикла
        for(int i = 0; i < 3; i++) {
            LED_PORT->BRR = LED_PIN;
            Delay(100000);
            LED_PORT->BSRR = LED_PIN;
            Delay(100000);
        }
    }
}
