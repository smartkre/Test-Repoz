#include "stm32f1xx.h"
#include "stm32f103x6.h"
#include "system_stm32f1xx.h"

// Определение масок пинов через битовые смещения
#define RST_PIN    (1 << 0)    // PA0
#define SCI_PIN    (1 << 1)    // PA1  
#define SDO_PIN    (1 << 2)    // PA2
#define SII_PIN    (1 << 3)    // PA3
#define SDI_PIN    (1 << 4)    // PA4
#define VCC_PIN    (1 << 5)    // PA5
#define BUTTON_PIN (1 << 6)    // PA6
#define LED_PIN    (1 << 13)   // PC13

// Определения для фьюзов
#define HFUSE  0x747C
#define LFUSE  0x646C
#define EFUSE  0x666E
#define ATTINY13 0x9007

// Прототипы функций
void GPIO_Init(void);
void delay_ms(uint32_t ms);
void delay_us(uint32_t us);
void blinkLED(uint8_t times, uint16_t duration);
uint8_t shiftOut(uint8_t val1, uint8_t val2);
void writeFuse(uint16_t fuse, uint8_t val);
void readFuses(void);
uint16_t readSignature(void);
uint8_t checkButton(void);

// Функции для работы с GPIO
void GPIO_SetBit(GPIO_TypeDef* GPIOx, uint16_t GPIO_Pin) {
    GPIOx->BSRR = GPIO_Pin;
}

void GPIO_ResetBit(GPIO_TypeDef* GPIOx, uint16_t GPIO_Pin) {
    GPIOx->BRR = GPIO_Pin;
}

uint8_t GPIO_ReadInputDataBit(GPIO_TypeDef* GPIOx, uint16_t GPIO_Pin) {
    return (GPIOx->IDR & GPIO_Pin) ? 1 : 0;
}

int main(void) {
    // Инициализация системы
    SystemInit();
    GPIO_Init();
    
    // Начальная индикация - 2 моргания по 0.5 сек
    blinkLED(2, 500);
    
    // Основной цикл
    while(1) {
        // Мигание раз в секунду в режиме ожидания
        GPIO_SetBit(GPIOC, LED_PIN);
        delay_ms(500);
        GPIO_ResetBit(GPIOC, LED_PIN);
        delay_ms(500);
        
        // Проверка нажатия кнопки
        if(checkButton()) {
            // Начало процедуры программирования
            GPIO_ResetBit(GPIOA, SDI_PIN);
            GPIO_ResetBit(GPIOA, SII_PIN);
            
            // Установка SDO как выхода (PA2)
            GPIOA->CRL &= ~(0xF << (2 * 4)); // Очистка битов конфигурации
            GPIOA->CRL |= (0x1 << (2 * 4));  // Output, 10MHz, Push-pull
            
            GPIO_ResetBit(GPIOA, SDO_PIN);
            GPIO_SetBit(GPIOA, RST_PIN);   // 12V Off
            GPIO_SetBit(GPIOA, VCC_PIN);   // VCC On
            
            delay_us(20);
            GPIO_ResetBit(GPIOA, RST_PIN); // 12V On
            delay_us(10);
            
            // Возврат SDO как входа (PA2)
            GPIOA->CRL &= ~(0xF << (2 * 4)); // Очистка битов конфигурации
            GPIOA->CRL |= (0x4 << (2 * 4));  // Input, floating
            
            delay_us(300);
            
            // Чтение сигнатуры
            uint16_t signature = readSignature();
            
            // Программирование фьюзов для ATtiny13
            if(signature == ATTINY13) {
                writeFuse(LFUSE, 0x6A);
                writeFuse(HFUSE, 0xFF);
                
                // Проверка результата
                readFuses();
                blinkLED(3, 1000);  // Успех - 3 моргания за 1 сек
            } else {
                blinkLED(5, 1500);  // Ошибка - 5 морганий за 1.5 сек
            }
            
            // Завершение
            GPIO_ResetBit(GPIOA, SCI_PIN);
            GPIO_ResetBit(GPIOA, VCC_PIN);
            GPIO_SetBit(GPIOA, RST_PIN);
            
            delay_ms(1000);
        }
    }
}

// Инициализация GPIO
void GPIO_Init(void) {
    // Включение тактирования портов A и C
    RCC->APB2ENR |= RCC_APB2ENR_IOPAEN | RCC_APB2ENR_IOPCEN;
    
    // Настройка выходов PA0-PA5
    for(int i = 0; i <= 5; i++) {
        GPIOA->CRL &= ~(0xF << (i * 4));  // Очистка
        GPIOA->CRL |= (0x1 << (i * 4));   // Output, 10MHz, Push-pull
    }
    
    // Настройка светодиода PC13
    GPIOC->CRH &= ~(0xF << ((13 - 8) * 4));
    GPIOC->CRH |= (0x1 << ((13 - 8) * 4));
    
    // Настройка кнопки PA6 с подтяжкой к VCC
    GPIOA->CRL &= ~(0xF << (6 * 4));
    GPIOA->CRL |= (0x8 << (6 * 4));  // Input with pull-up/pull-down
    GPIOA->ODR |= BUTTON_PIN;        // Pull-up
    
    // Настройка SDO как входа по умолчанию (PA2)
    GPIOA->CRL &= ~(0xF << (2 * 4));
    GPIOA->CRL |= (0x4 << (2 * 4));  // Input, floating
    
    // Установка начальных состояний
    GPIO_SetBit(GPIOA, RST_PIN);
    GPIO_ResetBit(GPIOA, VCC_PIN);
}

// Функция моргания светодиодом
void blinkLED(uint8_t times, uint16_t duration) {
    uint16_t delay_time = duration / (times * 2);
    
    for(uint8_t i = 0; i < times; i++) {
        GPIO_SetBit(GPIOC, LED_PIN);
        delay_ms(delay_time);
        GPIO_ResetBit(GPIOC, LED_PIN);
        delay_ms(delay_time);
    }
}

// Проверка нажатия кнопки с антидребезгом
uint8_t checkButton(void) {
    if(GPIO_ReadInputDataBit(GPIOA, BUTTON_PIN) == 0) {
        delay_ms(50);  // Антидребезг
        if(GPIO_ReadInputDataBit(GPIOA, BUTTON_PIN) == 0) {
            while(GPIO_ReadInputDataBit(GPIOA, BUTTON_PIN) == 0); // Ожидание отпускания
            return 1;
        }
    }
    return 0;
}

// Функции HVSP (аналогичные Arduino коду)
uint8_t shiftOut(uint8_t val1, uint8_t val2) {
    uint16_t inBits = 0;
    
    // Ожидание готовности SDO
    while(GPIO_ReadInputDataBit(GPIOA, SDO_PIN) == 0);
    
    uint16_t dout = (uint16_t)val1 << 2;
    uint16_t iout = (uint16_t)val2 << 2;
    
    for(int8_t ii = 10; ii >= 0; ii--) {
        if(dout & (1 << ii)) {
            GPIO_SetBit(GPIOA, SDI_PIN);
        } else {
            GPIO_ResetBit(GPIOA, SDI_PIN);
        }
        
        if(iout & (1 << ii)) {
            GPIO_SetBit(GPIOA, SII_PIN);
        } else {
            GPIO_ResetBit(GPIOA, SII_PIN);
        }
        
        inBits <<= 1;
        inBits |= GPIO_ReadInputDataBit(GPIOA, SDO_PIN);
        
        GPIO_SetBit(GPIOA, SCI_PIN);
        GPIO_ResetBit(GPIOA, SCI_PIN);
    }
    
    return inBits >> 2;
}

void writeFuse(uint16_t fuse, uint8_t val) {
    shiftOut(0x40, 0x4C);
    shiftOut(val, 0x2C);
    shiftOut(0x00, (uint8_t)(fuse >> 8));
    shiftOut(0x00, (uint8_t)fuse);
}

void readFuses(void) {
    // Чтение фьюзов
    shiftOut(0x04, 0x4C);  // LFuse
    shiftOut(0x00, 0x68);
    shiftOut(0x00, 0x6C);
    
    shiftOut(0x04, 0x4C);  // HFuse
    shiftOut(0x00, 0x7A);
    shiftOut(0x00, 0x7E);
    
    shiftOut(0x04, 0x4C);  // EFuse
    shiftOut(0x00, 0x6A);
    shiftOut(0x00, 0x6E);
}

uint16_t readSignature(void) {
    uint16_t sig = 0;
    uint8_t val;
    
    for(uint8_t ii = 1; ii < 3; ii++) {
        shiftOut(0x08, 0x4C);
        shiftOut(ii, 0x0C);
        shiftOut(0x00, 0x68);
        val = shiftOut(0x00, 0x6C);
        sig = (sig << 8) + val;
    }
    
    return sig;
}

// Простые функции задержки
void delay_ms(uint32_t ms) {
    for(uint32_t i = 0; i < ms; i++) {
        for(uint32_t j = 0; j < 7200; j++);  // Примерно 1ms при 72MHz
    }
}

void delay_us(uint32_t us) {
    for(uint32_t i = 0; i < us; i++) {
        for(uint32_t j = 0; j < 7; j++);  // Примерно 1us при 72MHz
    }
}