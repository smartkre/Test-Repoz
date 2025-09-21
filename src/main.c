#include "stm32f1xx.h"

#define TEST_PIN     (1U << 0)  // PB0
#define LED_PIN      (1U << 13)  // PC13

void System_Init(void) {
    RCC->CR |= RCC_CR_HSEON;
    while (!(RCC->CR & RCC_CR_HSERDY));
    RCC->BDCR &= ~RCC_BDCR_LSEON;
    while (RCC->BDCR & RCC_BDCR_LSERDY);
    RCC->CFGR &= ~RCC_CFGR_SW;
    RCC->CFGR |= RCC_CFGR_SW_HSE;
    while ((RCC->CFGR & RCC_CFGR_SWS) != RCC_CFGR_SWS_HSE);
    RCC->CFGR &= ~(RCC_CFGR_HPRE | RCC_CFGR_PPRE1 | RCC_CFGR_PPRE2);
    FLASH->ACR &= ~FLASH_ACR_LATENCY;
    FLASH->ACR |= FLASH_ACR_LATENCY_0;
    RCC->APB2ENR |= RCC_APB2ENR_IOPBEN | RCC_APB2ENR_IOPCEN;
    GPIOB->CRL &= ~(0xF << 0); GPIOB->CRL |= (0x1 << 0);
    GPIOC->CRH &= ~(0xF << ((13 - 8) * 4)); GPIOC->CRH |= (0x1 << ((13 - 8) * 4));
}

void delay_ms(uint32_t ms) {
    for (volatile uint32_t t = 0; t < ms; t++)
        for (volatile uint32_t i = 0; i < 8000; i++) __asm__("nop");
}

void delay_us(uint32_t us) {
    for (volatile uint32_t t = 0; t < us; t++)
        for (volatile uint32_t i = 0; i < 8; i++) __asm__("nop");
}

int main(void) {
    System_Init();
    GPIOC->BSRR = LED_PIN;
    while (1) {
        GPIOB->BSRR = TEST_PIN; delay_us(100);
        GPIOB->BRR = TEST_PIN; delay_us(100);
        GPIOC->BRR = LED_PIN; delay_ms(500);
        GPIOC->BSRR = LED_PIN; delay_ms(500);
    }
}
