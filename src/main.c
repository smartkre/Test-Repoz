#include "stm32f1xx.h"
#include "stm32f103x6.h"
#include "system_stm32f1xx.h"

// Определение пинов
#define RST_PIN    GPIO_Pin_0
#define SCI_PIN    GPIO_Pin_1
#define SDO_PIN    GPIO_Pin_2
#define SII_PIN    GPIO_Pin_3
#define SDI_PIN    GPIO_Pin_4
#define VCC_PIN    GPIO_Pin_5
#define BUTTON_PIN GPIO_Pin_6
#define LED_PIN    GPIO_Pin_13

#define RST_PORT    GPIOA
#define SCI_PORT    GPIOA
#define SDO_PORT    GPIOA
#define SII_PORT    GPIOA
#define SDI_PORT    GPIOA
#define VCC_PORT    GPIOA
#define BUTTON_PORT GPIOA
#define LED_PORT    GPIOC

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

int main(void) {
    // Инициализация системы
    SystemInit();
    GPIO_Init();
    
    // Начальная индикация - 2 моргания по 0.5 сек
    blinkLED(2, 500);
    
    // Основной цикл
    while(1) {
        // Мигание раз в секунду в режиме ожидания
        GPIO_WriteBit(LED_PORT, LED_PIN, Bit_SET);
        delay_ms(500);
        GPIO_WriteBit(LED_PORT, LED_PIN, Bit_RESET);
        delay_ms(500);
        
        // Проверка нажатия кнопки
        if(checkButton()) {
            // Начало процедуры программирования
            GPIO_WriteBit(SDI_PORT, SDI_PIN, Bit_RESET);
            GPIO_WriteBit(SII_PORT, SII_PIN, Bit_RESET);
            
            // Установка SDO как выхода
            GPIO_InitTypeDef GPIO_InitStruct;
            GPIO_InitStruct.GPIO_Pin = SDO_PIN;
            GPIO_InitStruct.GPIO_Mode = GPIO_Mode_Out_PP;
            GPIO_InitStruct.GPIO_Speed = GPIO_Speed_2MHz;
            GPIO_Init(SDO_PORT, &GPIO_InitStruct);
            
            GPIO_WriteBit(SDO_PORT, SDO_PIN, Bit_RESET);
            GPIO_WriteBit(RST_PORT, RST_PIN, Bit_SET);  // 12V Off
            GPIO_WriteBit(VCC_PORT, VCC_PIN, Bit_SET);  // VCC On
            
            delay_us(20);
            GPIO_WriteBit(RST_PORT, RST_PIN, Bit_RESET); // 12V On
            delay_us(10);
            
            // Возврат SDO как входа
            GPIO_InitStruct.GPIO_Pin = SDO_PIN;
            GPIO_InitStruct.GPIO_Mode = GPIO_Mode_IN_FLOATING;
            GPIO_Init(SDO_PORT, &GPIO_InitStruct);
            
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
            GPIO_WriteBit(SCI_PORT, SCI_PIN, Bit_RESET);
            GPIO_WriteBit(VCC_PORT, VCC_PIN, Bit_RESET);
            GPIO_WriteBit(RST_PORT, RST_PIN, Bit_SET);
            
            delay_ms(1000);
        }
    }
}

// Инициализация GPIO
void GPIO_Init(void) {
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA | RCC_APB2Periph_GPIOC, ENABLE);
    
    GPIO_InitTypeDef GPIO_InitStruct;
    
    // Настройка выходов
    GPIO_InitStruct.GPIO_Speed = GPIO_Speed_2MHz;
    GPIO_InitStruct.GPIO_Mode = GPIO_Mode_Out_PP;
    
    GPIO_InitStruct.GPIO_Pin = RST_PIN | SCI_PIN | SII_PIN | SDI_PIN | VCC_PIN;
    GPIO_Init(GPIOA, &GPIO_InitStruct);
    
    GPIO_InitStruct.GPIO_Pin = LED_PIN;
    GPIO_Init(GPIOC, &GPIO_InitStruct);
    
    // Настройка входа кнопки с подтяжкой к VCC
    GPIO_InitStruct.GPIO_Pin = BUTTON_PIN;
    GPIO_InitStruct.GPIO_Mode = GPIO_Mode_IPU;
    GPIO_Init(GPIOA, &GPIO_InitStruct);
    
    // Настройка SDO как входа по умолчанию
    GPIO_InitStruct.GPIO_Pin = SDO_PIN;
    GPIO_InitStruct.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(GPIOA, &GPIO_InitStruct);
    
    // Установка начальных состояний
    GPIO_WriteBit(RST_PORT, RST_PIN, Bit_SET);
    GPIO_WriteBit(VCC_PORT, VCC_PIN, Bit_RESET);
}

// Функция моргания светодиодом
void blinkLED(uint8_t times, uint16_t duration) {
    uint16_t delay_time = duration / (times * 2);
    
    for(uint8_t i = 0; i < times; i++) {
        GPIO_WriteBit(LED_PORT, LED_PIN, Bit_SET);
        delay_ms(delay_time);
        GPIO_WriteBit(LED_PORT, LED_PIN, Bit_RESET);
        delay_ms(delay_time);
    }
}

// Проверка нажатия кнопки с антидребезгом
uint8_t checkButton(void) {
    if(GPIO_ReadInputDataBit(BUTTON_PORT, BUTTON_PIN) == Bit_RESET) {
        delay_ms(50);  // Антидребезг
        if(GPIO_ReadInputDataBit(BUTTON_PORT, BUTTON_PIN) == Bit_RESET) {
            while(GPIO_ReadInputDataBit(BUTTON_PORT, BUTTON_PIN) == Bit_RESET); // Ожидание отпускания
            return 1;
        }
    }
    return 0;
}

// Функции HVSP (аналогичные Arduino коду)
uint8_t shiftOut(uint8_t val1, uint8_t val2) {
    uint16_t inBits = 0;
    
    // Ожидание готовности SDO
    while(GPIO_ReadInputDataBit(SDO_PORT, SDO_PIN) == Bit_RESET);
    
    uint16_t dout = (uint16_t)val1 << 2;
    uint16_t iout = (uint16_t)val2 << 2;
    
    for(int8_t ii = 10; ii >= 0; ii--) {
        GPIO_WriteBit(SDI_PORT, SDI_PIN, (dout & (1 << ii)) ? Bit_SET : Bit_RESET);
        GPIO_WriteBit(SII_PORT, SII_PIN, (iout & (1 << ii)) ? Bit_SET : Bit_RESET);
        
        inBits <<= 1;
        inBits |= (GPIO_ReadInputDataBit(SDO_PORT, SDO_PIN) == Bit_SET) ? 1 : 0;
        
        GPIO_WriteBit(SCI_PORT, SCI_PIN, Bit_SET);
        GPIO_WriteBit(SCI_PORT, SCI_PIN, Bit_RESET);
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