#include "stm32f1xx.h"

#define LED_PORT GPIOC
#define LED_PIN  13

#define SCI_PORT GPIOB
#define SCI_PIN  4

static void delay(volatile uint32_t d) {
    while (d--) __asm__("nop");
}

int main(void) {
    /* Включаем тактирование портов B и C */
    RCC->APB2ENR |= RCC_APB2ENR_IOPBEN | RCC_APB2ENR_IOPCEN | RCC_APB2ENR_AFIOEN;

    /* Отключаем JTAG, оставляем SWD */
    AFIO->MAPR |= AFIO_MAPR_SWJ_CFG_JTAGDISABLE;

    /* Настраиваем PC13 как push-pull output (LED) */
    LED_PORT->CRH &= ~(0xF << ((LED_PIN - 8) * 4));
    LED_PORT->CRH |=  (0x1 << ((LED_PIN - 8) * 4)); // 0b0001 = output, max 10 MHz

    /* Настраиваем PB4 как push-pull output */
    SCI_PORT->CRL &= ~(0xF << (SCI_PIN * 4));
    SCI_PORT->CRL |=  (0x3 << (SCI_PIN * 4)); // 0b0011 = output, max 50 MHz

    /* Генерируем импульсы на PB4 */
    while (1) {
        SCI_PORT->BSRR = (1 << SCI_PIN);  // high
        delay(100);
        SCI_PORT->BRR  = (1 << SCI_PIN);  // low
        delay(100);

        /* Мигаем светодиодом медленно для наглядности */
        LED_PORT->BRR  = (1 << LED_PIN); 
        delay(50000);
        LED_PORT->BSRR = (1 << LED_PIN);
        delay(50000);
    }
}
