#include "stm32f1xx.h"

#define LED_PIN   (1U << 13) // PC13
#define TEST_PIN  (1U << 0)  // PB0

void GPIO_Init(void) {
    // Включаем тактирование GPIOB и GPIOC, а также AFIO
    RCC->APB2ENR |= RCC_APB2ENR_IOPBEN | RCC_APB2ENR_IOPCEN | RCC_APB2ENR_AFIOEN;

    // PB0 -> альт.функция push-pull (TIM3_CH3)
    GPIOB->CRL &= ~(0xF << (0 * 4));
    GPIOB->CRL |= (0xB << (0 * 4));  // 0xB = 1011 (AF Push-Pull, 50 MHz)

    // PC13 -> выход push-pull 2 MHz
    GPIOC->CRH &= ~(0xF << ((13 - 8) * 4));
    GPIOC->CRH |= (0x2 << ((13 - 8) * 4));
}

void TIM3_Init(void) {
    // Включаем тактирование TIM3
    RCC->APB1ENR |= RCC_APB1ENR_TIM3EN;

    // Настраиваем таймер: делитель и период
    // Если частота APB1 = 36 MHz (типично для F103):
    // PSC = 35 -> счётчик тикает на 1 MHz
    TIM3->PSC = 35;
    TIM3->ARR = 1;   // период = 2 тика -> 500 кГц (меандр на выходе)
    TIM3->CCR3 = 1;  // 50% скважность

    // Режим PWM на CH3
    TIM3->CCMR2 &= ~TIM_CCMR2_OC3M;
    TIM3->CCMR2 |= (0x6 << TIM_CCMR2_OC3M_Pos); // Toggle mode
    TIM3->CCER |= TIM_CCER_CC3E;

    // Запуск таймера
    TIM3->CR1 |= TIM_CR1_CEN;
}

void delay_ms(uint32_t ms) {
    for (volatile uint32_t i = 0; i < ms * 8000; i++) {
        __asm__("nop");
    }
}

int main(void) {
    GPIO_Init();
    TIM3_Init();

    while (1) {
        // Мигаем LED на PC13 (10 Гц)
        GPIOC->BRR = LED_PIN;   // Вкл
        delay_ms(50);
        GPIOC->BSRR = LED_PIN;  // Выкл
        delay_ms(50);
    }
}
