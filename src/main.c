#include "stm32f1xx.h"

// Определение выводов
#define HVSP_SCI_PORT GPIOB
#define HVSP_SCI_PIN  (1 << 4)  // PB4 (TIM3_CH1)

#define HVSP_SDO_PORT GPIOB
#define HVSP_SDO_PIN  (1 << 1)  // PB1 (TIM3_CH4)

#define LED_PORT GPIOC
#define LED_PIN  (1 << 13)      // PC13

void System_Init(void) {
    // Включение HSI
    RCC->CR |= RCC_CR_HSION;
    while ((RCC->CR & RCC_CR_HSIRDY) == 0);
    
    // Включение тактирования портов, таймеров и AFIO
    RCC->APB2ENR |= RCC_APB2ENR_IOPBEN | RCC_APB2ENR_IOPCEN | RCC_APB2ENR_AFIOEN;
    RCC->APB1ENR |= RCC_APB1ENR_TIM3EN;
}

void GPIO_Init(void) {
    // Настройка LED (PC13)
    GPIOC->CRH &= ~(GPIO_CRH_MODE13 | GPIO_CRH_CNF13);
    GPIOC->CRH |= GPIO_CRH_MODE13_0; // Output mode, 10 MHz
    GPIOC->CRH |= GPIO_CRH_CNF13_0;  // Open-drain
    
    // Настройка PB4 (TIM3_CH1) как Alternate Function Push-Pull
    GPIOB->CRL &= ~(GPIO_CRL_MODE4 | GPIO_CRL_CNF4);
    GPIOB->CRL |= GPIO_CRL_MODE4_0 | GPIO_CRL_MODE4_1; // Output mode, 50 MHz
    GPIOB->CRL |= GPIO_CRL_CNF4_1; // Alternate function output Push-pull
    
    // Настройка PB1 (TIM3_CH4) как Alternate Function Push-Pull
    GPIOB->CRL &= ~(GPIO_CRL_MODE1 | GPIO_CRL_CNF1);
    GPIOB->CRL |= GPIO_CRL_MODE1_0 | GPIO_CRL_MODE1_1; // Output mode, 50 MHz
    GPIOB->CRL |= GPIO_CRL_CNF1_1; // Alternate function output Push-pull
    
    // Переназначение TIM3_CH4 на PB1 (по умолчанию на PC9)
    AFIO->MAPR |= AFIO_MAPR_TIM3_REMAP_0; // Partial remap: TIM3_CH4 -> PB1
}

void TIM3_Init_500kHz(void) {
    // Остановка таймера
    TIM3->CR1 &= ~TIM_CR1_CEN;
    
    // Сброс настроек
    TIM3->CR1 = 0;
    TIM3->CR2 = 0;
    TIM3->PSC = 0;
    TIM3->ARR = 0;
    TIM3->CCR1 = 0;
    TIM3->CCR4 = 0;
    
    // Настройка таймера для генерации 500 kHz на CH1
    TIM3->PSC = 7;          // Делитель: 8 MHz / (7 + 1) = 1 MHz
    TIM3->ARR = 1;          // Период: 2 такта (1 MHz / 2 = 500 kHz)
    TIM3->CCR1 = 1;         // Скважность 50%
    
    // Настройка канала 1 в PWM mode 1
    TIM3->CCMR1 &= ~TIM_CCMR1_OC1M;
    TIM3->CCMR1 |= (6 << 4); // PWM mode 1 (OC1M = 110)
    TIM3->CCMR1 |= TIM_CCMR1_OC1PE; // Enable preload
    
    // Выключаем канал 4
    TIM3->CCER &= ~TIM_CCER_CC4E;
    
    // Включаем канал 1
    TIM3->CCER |= TIM_CCER_CC1E;
    
    // Включение таймера
    TIM3->CR1 |= TIM_CR1_CEN;
}

void TIM3_Init_200kHz(void) {
    // Остановка таймера
    TIM3->CR1 &= ~TIM_CR1_CEN;
    
    // Сброс настроек
    TIM3->CR1 = 0;
    TIM3->CR2 = 0;
    TIM3->PSC = 0;
    TIM3->ARR = 0;
    TIM3->CCR1 = 0;
    TIM3->CCR4 = 0;
    
    // Настройка таймера для генерации 200 kHz на CH4
    TIM3->PSC = 7;          // Делитель: 8 MHz / (7 + 1) = 1 MHz
    TIM3->ARR = 4;          // Период: 5 тактов (1 MHz / 5 = 200 kHz)
    TIM3->CCR4 = 2;         // Скважность 50%
    
    // Настройка канала 4 в PWM mode 1
    TIM3->CCMR2 &= ~TIM_CCMR2_OC4M;
    TIM3->CCMR2 |= (6 << 12); // PWM mode 1 (OC4M = 110)
    TIM3->CCMR2 |= TIM_CCMR2_OC4PE; // Enable preload
    
    // Выключаем канал 1
    TIM3->CCER &= ~TIM_CCER_CC1E;
    
    // Включаем канал 4
    TIM3->CCER |= TIM_CCER_CC4E;
    
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
    
    // Мигаем 3 раза для индикации старта
    for(int i = 0; i < 3; i++) {
        LED_PORT->BRR = LED_PIN; // LED ON
        Delay(500000);
        LED_PORT->BSRR = LED_PIN; // LED OFF
        Delay(500000);
    }
    
    uint32_t counter = 0;
    uint8_t phase = 0; // 0 = PB4 500kHz, 1 = PB1 200kHz
    
    while(1) {
        counter++;
        
        if (phase == 0) {
            // Фаза 1: Генерация 500 kHz на PB4
            TIM3_Init_500kHz();
            LED_PORT->BRR = LED_PIN; // LED ON - индикация фазы 1
            
            // Ждем 10 секунд
            if (counter > 10000000) {
                counter = 0;
                phase = 1;
                LED_PORT->BSRR = LED_PIN; // LED OFF
            }
        } else {
            // Фаза 2: Генерация 200 kHz на PB1
            TIM3_Init_200kHz();
            LED_PORT->BSRR = LED_PIN; // LED OFF - индикация фазы 2
            
            // Ждем 10 секунд
            if (counter > 10000000) {
                counter = 0;
                phase = 0;
                LED_PORT->BRR = LED_PIN; // LED ON
            }
        }
        
        Delay(1);
    }
}
