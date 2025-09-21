#include "stm32f1xx.h"

// Определение выводов
#define HVSP_SCI_PORT GPIOB
#define HVSP_SCI_PIN  (1 << 4)  // PB4 (TIM3_CH1)

#define HVSP_SDO_PORT GPIOB
#define HVSP_SDO_PIN  (1 << 1)  // PB1 (TIM3_CH4)

#define LED_PORT GPIOC
#define LED_PIN  (1 << 13)      // PC13

// --- Инициализация тактирования с внешним HSE (8 МГц) и отключением LSE ---
void System_Init(void) {
    // Включение HSI для стабильного старта
    RCC->CR |= RCC_CR_HSION;
    while (!(RCC->CR & RCC_CR_HSIRDY));

    // Включение Backup Domain для LSE
    RCC->APB1ENR |= RCC_APB1ENR_BKPEN | RCC_APB1ENR_PWREN;
    // Отключение LSE (32.768 кГц)
    RCC->BDCR &= ~RCC_BDCR_LSEON;
    while (RCC->BDCR & RCC_BDCR_LSERDY);

    // Включение HSE (внешний кварц 8 МГц)
    RCC->CR |= RCC_CR_HSEON;
    while (!(RCC->CR & RCC_CR_HSERDY));

    // Отключение PLL
    RCC->CR &= ~RCC_CR_PLLON;
    while (RCC->CR & RCC_CR_PLLRDY);

    // Переключение SYSCLK на HSE
    RCC->CFGR &= ~RCC_CFGR_SW;
    RCC->CFGR |= RCC_CFGR_SW_HSE;
    while ((RCC->CFGR & RCC_CFGR_SWS) != RCC_CFGR_SWS_HSE);

    // Сброс делителей
    RCC->CFGR &= ~(RCC_CFGR_HPRE | RCC_CFGR_PPRE1 | RCC_CFGR_PPRE2);

    // Настройка Flash Latency (0WS для 0-24 МГц)
    FLASH->ACR &= ~FLASH_ACR_LATENCY;
    FLASH->ACR |= FLASH_ACR_LATENCY_0;

    // Включаем тактирование GPIOA, GPIOB, GPIOC, AFIO и TIM3
    RCC->APB2ENR |= RCC_APB2ENR_IOPAEN | RCC_APB2ENR_IOPBEN | RCC_APB2ENR_IOPCEN | RCC_APB2ENR_AFIOEN;
    RCC->APB1ENR |= RCC_APB1ENR_TIM3EN;
}

void GPIO_Init(void) {
    // Настройка LED (PC13)
    GPIOC->CRH &= ~(GPIO_CRH_MODE13 | GPIO_CRH_CNF13);
    GPIOC->CRH |= GPIO_CRH_MODE13_0; // Output mode, 10 MHz
    GPIOC->CRH |= GPIO_CRH_CNF13_0;  // Open-drain
    
    // Настройка PB4 (TIM3_CH1) как Alternate Function Push-Pull
    GPIOB->CRL &= ~(GPIO_CRL_MODE4 | GPIO_CRL_CNF4);
    GPIOB->CRL |= (0xB << (4 * 4)); // AF push-pull 10MHz (аналогично PA2 из вашего кода)
    
    // Настройка PB1 (TIM3_CH4) как Alternate Function Push-Pull
    GPIOB->CRL &= ~(GPIO_CRL_MODE1 | GPIO_CRL_CNF1);
    GPIOB->CRL |= (0xB << (1 * 4)); // AF push-pull 10MHz
    
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
    TIM3->PSC = 0;          // Без деления (частота таймера = 8 МГц)
    TIM3->ARR = 15;         // Период = 16 / 8 МГц = 500 kHz (8MHz / 16 = 500kHz)
    TIM3->CCR1 = 8;         // Скважность 50%
    
    // Настройка канала 1 в PWM mode 1
    TIM3->CCMR1 &= ~TIM_CCMR1_OC1M;
    TIM3->CCMR1 |= TIM_CCMR1_OC1M_1 | TIM_CCMR1_OC1M_2; // PWM Mode 1
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
    TIM3->PSC = 0;          // Без деления (частота таймера = 8 МГц)
    TIM3->ARR = 39;         // Период = 40 / 8 МГц = 200 kHz (8MHz / 40 = 200kHz)
    TIM3->CCR4 = 20;        // Скважность 50%
    
    // Настройка канала 4 в PWM mode 1
    TIM3->CCMR2 &= ~TIM_CCMR2_OC4M;
    TIM3->CCMR2 |= TIM_CCMR2_OC4M_1 | TIM_CCMR2_OC4M_2; // PWM Mode 1
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
