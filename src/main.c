#include "stm32f1xx.h"

// Определение выводов согласно вашему переназначению
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

#define BUTTON_PORT GPIOA
#define BUTTON_PIN  (1 << 9)    // PA9

#define LED_PORT GPIOC
#define LED_PIN  (1 << 13)      // PC13

// Прототипы функций
void System_Init(void);
void GPIO_Init(void);
void Delay_ms(uint32_t ms);
void Delay_us(uint32_t us);
void Generate_Test_Signals(void);

int main(void) {
    System_Init();
    GPIO_Init();
    
    // Мигаем 3 раза для индикации старта
    for(int i = 0; i < 3; i++) {
        LED_PORT->BRR = LED_PIN; // LED ON
        Delay_ms(100);
        LED_PORT->BSRR = LED_PIN; // LED OFF
        Delay_ms(300);
    }
    
    // Бесконечный цикл генерации тестовых сигналов
    while(1) {
        Generate_Test_Signals();
        Delay_ms(1000); // Пауза 1 сек между сериями
    }
}

// Инициализация системы
void System_Init(void) {
    // Включение HSI
    RCC->CR |= RCC_CR_HSION;
    while ((RCC->CR & RCC_CR_HSIRDY) == 0);
    
    // Включение тактирования портов
    RCC->APB2ENR |= RCC_APB2ENR_IOPAEN | RCC_APB2ENR_IOPBEN | RCC_APB2ENR_IOPCEN;
    
    // Простая настройка на HSI 8MHz
    // Для тестовых сигналов точная настройка не критична
}

// Инициализация GPIO
void GPIO_Init(void) {
    // Настройка LED (PC13) - выход Open-Drain
    GPIOC->CRH &= ~(GPIO_CRH_MODE13 | GPIO_CRH_CNF13);
    GPIOC->CRH |= GPIO_CRH_MODE13_0; // Output mode, max speed 10 MHz
    GPIOC->CRH |= GPIO_CRH_CNF13_0;  // Open-drain
    
    // Настройка HVSP выводов - все на выход
    // RST (PB0)
    GPIOB->CRL &= ~(GPIO_CRL_MODE0 | GPIO_CRL_CNF0);
    GPIOB->CRL |= GPIO_CRL_MODE0;     // Output mode, max speed 50 MHz
    
    // SCI (PB4)
    GPIOB->CRL &= ~(GPIO_CRL_MODE4 | GPIO_CRL_CNF4);
    GPIOB->CRL |= GPIO_CRL_MODE4;     // Output mode, max speed 50 MHz
    
    // SII (PA6)
    GPIOA->CRL &= ~(GPIO_CRL_MODE6 | GPIO_CRL_CNF6);
    GPIOA->CRL |= GPIO_CRL_MODE6;     // Output mode, max speed 50 MHz
    
    // SDI (PA7)
    GPIOA->CRL &= ~(GPIO_CRL_MODE7 | GPIO_CRL_CNF7);
    GPIOA->CRL |= GPIO_CRL_MODE7;     // Output mode, max speed 50 MHz
    
    // SDO (PB1) - на выход
    GPIOB->CRL &= ~(GPIO_CRL_MODE1 | GPIO_CRL_CNF1);
    GPIOB->CRL |= GPIO_CRL_MODE1;     // Output mode, max speed 50 MHz
    
    // Изначально все линии в LOW
    HVSP_RST_PORT->BRR = HVSP_RST_PIN;
    HVSP_SCI_PORT->BRR = HVSP_SCI_PIN;
    HVSP_SII_PORT->BRR = HVSP_SII_PIN;
    HVSP_SDI_PORT->BRR = HVSP_SDI_PIN;
    HVSP_SDO_PORT->BRR = HVSP_SDO_PIN;
}

// Генерация тестовых сигналов
void Generate_Test_Signals(void) {
    // Серия 1: Тактовые импульсы (SCI) - 1 MHz
    for(int i = 0; i < 20; i++) {
        HVSP_SCI_PORT->BSRR = HVSP_SCI_PIN; // HIGH
        Delay_us(0.5); // 500 нс
        HVSP_SCI_PORT->BRR = HVSP_SCI_PIN;  // LOW
        Delay_us(0.5); // 500 нс
    }
    
    Delay_us(10);
    
    // Серия 2: Данные (SII/SDI) с тактом
    for(int i = 0; i < 8; i++) {
        // Устанавливаем данные
        HVSP_SII_PORT->BSRR = HVSP_SII_PIN; // SII = HIGH
        HVSP_SDI_PORT->BRR = HVSP_SDI_PIN;  // SDI = LOW
        
        // Тактовый импульс
        HVSP_SCI_PORT->BSRR = HVSP_SCI_PIN; // HIGH
        Delay_us(0.5);
        HVSP_SCI_PORT->BRR = HVSP_SCI_PIN;  // LOW
        Delay_us(0.5);
    }
    
    Delay_us(10);
    
    // Серия 3: Другой паттерн данных
    for(int i = 0; i < 8; i++) {
        // Устанавливаем данные
        HVSP_SII_PORT->BRR = HVSP_SII_PIN;  // SII = LOW
        HVSP_SDI_PORT->BSRR = HVSP_SDI_PIN; // SDI = HIGH
        
        // Тактовый импульс
        HVSP_SCI_PORT->BSRR = HVSP_SCI_PIN; // HIGH
        Delay_us(0.5);
        HVSP_SCI_PORT->BRR = HVSP_SCI_PIN;  // LOW
        Delay_us(0.5);
    }
    
    Delay_us(10);
    
    // Серия 4: RST импульс (имитация 12V)
    HVSP_RST_PORT->BSRR = HVSP_RST_PIN; // HIGH (12V)
    Delay_us(50);
    HVSP_RST_PORT->BRR = HVSP_RST_PIN;  // LOW
    Delay_us(10);
}

// Простые функции задержки
void Delay_ms(uint32_t ms) {
    for (uint32_t i = 0; i < ms * 8000; i++) {
        __NOP();
    }
}

void Delay_us(uint32_t us) {
    for (uint32_t i = 0; i < us * 8; i++) {
        __NOP();
    }
}
