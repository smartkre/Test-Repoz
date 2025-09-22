#include "stm32f1xx.h"
#include "stm32f103x6.h"
#include "system_stm32f1xx.h"

// Определение пинов через битовые маски
#define RST_PIN    0    // PA0
#define SCI_PIN    1    // PA1  
#define SDO_PIN    2    // PA2
#define SII_PIN    3    // PA3
#define SDI_PIN    4    // PA4
#define VCC_PIN    5    // PA5
#define BUTTON_PIN 6    // PA6
#define LED_PIN    13   // PC13

// Базовые адреса портов
#define GPIOA_BASE 0x40010800
#define GPIOC_BASE 0x40011000
#define RCC_BASE   0x40021000

// Структуры для доступа к регистрам
typedef struct {
    volatile uint32_t CRL;
    volatile uint32_t CRH;
    volatile uint32_t IDR;
    volatile uint32_t ODR;
    volatile uint32_t BSRR;
    volatile uint32_t BRR;
    volatile uint32_t LCKR;
} GPIO_TypeDef;

typedef struct {
    volatile uint32_t CR;
    volatile uint32_t CFGR;
    volatile uint32_t CIR;
    volatile uint32_t APB2RSTR;
    volatile uint32_t APB1RSTR;
    volatile uint32_t AHBENR;
    volatile uint32_t APB2ENR;
    volatile uint32_t APB1ENR;
    volatile uint32_t BDCR;
    volatile uint32_t CSR;
} RCC_TypeDef;

#define GPIOA ((GPIO_TypeDef *) GPIOA_BASE)
#define GPIOC ((GPIO_TypeDef *) GPIOC_BASE)
#define RCC   ((RCC_TypeDef *) RCC_BASE)

// Определения для фьюзов
#define HFUSE  0x747C
#define LFUSE  0x646C
#define EFUSE  0x666E
#define ATTINY13 0x9007

// Макросы для работы с битами
#define BIT_SET(reg, bit) ((reg) |= (1 << (bit)))
#define BIT_CLEAR(reg, bit) ((reg) &= ~(1 << (bit)))
#define BIT_READ(reg, bit) ((reg) & (1 << (bit)))

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
    GPIOx->BSRR = (1 << GPIO_Pin);
}

void GPIO_ResetBit(GPIO_TypeDef* GPIOx, uint16_t GPIO_Pin) {
    GPIOx->BRR = (1 << GPIO_Pin);
}

uint8_t GPIO_ReadInputDataBit(GPIO_TypeDef* GPIOx, uint16_t GPIO_Pin) {
    return (GPIOx->IDR & (1 << GPIO_Pin)) ? 1 : 0;
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
            
            // Установка SDO как выхода
            GPIOA->CRL &= ~(0xF << (SDO_PIN * 4)); // Очистка битов конфигурации
            GPIOA->CRL |= (0x1 << (SDO_PIN * 4));  // Output, 10MHz, Push-pull
            
            GPIO_ResetBit(GPIOA, SDO_PIN);
            GPIO_SetBit(GPIOA, RST_PIN);   // 12V Off
            GPIO_SetBit(GPIOA, VCC_PIN);   // VCC On
            
            delay_us(20);
            GPIO_ResetBit(GPIOA, RST_PIN); // 12V On
            delay_us(10);
            
            // Возврат SDO как входа
            GPIOA->CRL &= ~(0xF << (SDO_PIN * 4)); // Очистка битов конфигурации
            GPIOA->CRL |= (0x4 << (SDO_PIN * 4));  // Input, floating
            
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
    RCC->APB2ENR |= (1 << 2) | (1 << 4); // GPIOA и GPIOC
    
    // Настройка выходов PA0-PA5
    for(int i = 0; i <= 5; i++) {
        GPIOA->CRL &= ~(0xF << (i * 4));  // Очистка
        GPIOA->CRL |= (0x1 << (i * 4));   // Output, 10MHz, Push-pull
    }
    
    // Настройка светодиода PC13
    GPIOC->CRH &= ~(0xF << ((LED_PIN - 8) * 4));
    GPIOC->CRH |= (0x1 << ((LED_PIN - 8) * 4));
    
    // Настройка кнопки PA6 с подтяжкой к VCC
    GPIOA->CRL &= ~(0xF << (BUTTON_PIN * 4));
    GPIOA->CRL |= (0x8 << (BUTTON_PIN * 4));  // Input with pull-up/pull-down
    GPIOA->ODR |= (1 << BUTTON_PIN);          // Pull-up
    
    // Настройка SDO как входа по умолчанию
    GPIOA->CRL &= ~(0xF << (SDO_PIN * 4));
    GPIOA->CRL |= (0x4 << (SDO_PIN * 4));  // Input, floating
    
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
    uint8_t val;
    
    // Чтение LFUSE
    shiftOut(0x04, 0x4C);
    shiftOut(0x00, 0x68);
    val = shiftOut(0x00, 0x6C);
    
    // Чтение HFUSE  
    shiftOut(0x04, 0x4C);
    shiftOut(0x00, 0x7A);
    val = shiftOut(0x00, 0x7E);
    
    // Чтение EFUSE (для совместимости)
    shiftOut(0x04, 0x4C);
    shiftOut(0x00, 0x6A);
    val = shiftOut(0x00, 0x6E);
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