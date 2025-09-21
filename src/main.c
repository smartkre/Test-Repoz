#include "stm32f1xx.h"

// Определение выводов согласно вашему переназначению
#define HVSP_RST_PORT GPIOB
#define HVSP_RST_PIN  GPIO_PIN_0

#define HVSP_SCI_PORT GPIOB
#define HVSP_SCI_PIN  GPIO_PIN_4

#define HVSP_SDO_PORT GPIOB
#define HVSP_SDO_PIN  GPIO_PIN_1

#define HVSP_SII_PORT GPIOA
#define HVSP_SII_PIN  GPIO_PIN_6

#define HVSP_SDI_PORT GPIOA
#define HVSP_SDI_PIN  GPIO_PIN_7

#define BUTTON_PORT GPIOA
#define BUTTON_PIN  GPIO_PIN_9

#define LED_PORT GPIOC
#define LED_PIN  GPIO_PIN_13

// Определения для фьюзов и сигнатур
#define HFUSE 0x747C
#define LFUSE 0x646C
#define EFUSE 0x666E

#define ATTINY13_SIGNATURE 0x9007

// Прототипы функций
void System_Init(void);
void GPIO_Init(void);
void LED_Blink(uint8_t count, uint32_t duration_ms, uint32_t pause_ms);
uint8_t Button_Pressed(void);
void HVSP_Enter(void);
void HVSP_Exit(void);
uint8_t HVSP_ShiftOut(uint8_t val1, uint8_t val2);
uint16_t HVSP_ReadSignature(void);
void HVSP_WriteFuse(uint16_t fuse, uint8_t value);
void HVSP_ReadFuses(void);
void Delay_ms(uint32_t ms);
void Delay_us(uint32_t us);

int main(void) {
    System_Init();
    GPIO_Init();
    
    // Последовательность инициализации
    LED_Blink(3, 100, 300); // 3 моргания по 100мс с паузой 300мс (итого ~1.5с)
    
    uint8_t programming_done = 0;
    
    while(1) {
        if (!programming_done) {
            // Мигание каждую секунду в режиме ожидания
            LED_PORT->ODR ^= LED_PIN;
            Delay_ms(1000);
        }
        
        if (Button_Pressed() && !programming_done) {
            // Нажата кнопка - начинаем программирование
            programming_done = 1;
            
            // Входим в режим программирования
            HVSP_Enter();
            
            // Читаем сигнатуру
            uint16_t signature = HVSP_ReadSignature();
            
            if (signature == ATTINY13_SIGNATURE) {
                // Программируем фьюзы для ATtiny13
                HVSP_WriteFuse(LFUSE, 0x6A);
                HVSP_WriteFuse(HFUSE, 0xFF);
                
                // Читаем и проверяем (опционально)
                HVSP_ReadFuses();
                
                // Успешное завершение - 5 медленных морганий
                HVSP_Exit();
                LED_Blink(5, 200, 200); // 5 морганий по 200мс с паузой 200мс (итого 2с)
            } else {
                // Ошибка - быстрые моргания
                HVSP_Exit();
                LED_Blink(5, 100, 100); // 5 быстрых морганий
            }
        }
    }
}

// Инициализация системы
void System_Init(void) {
    // Включение HSI
    RCC->CR |= RCC_CR_HSION;
    while ((RCC->CR & RCC_CR_HSIRDY) == 0);
    
    // Включение Backup Domain
    RCC->APB1ENR |= RCC_APB1ENR_PWREN;
    PWR->CR |= PWR_CR_DBP;
    
    // Отключение LSE
    RCC->BDCR &= ~RCC_BDCR_LSEON;
    while (RCC->BDCR & RCC_BDCR_LSERDY);
    
    // Включение HSE
    RCC->CR |= RCC_CR_HSEON;
    while ((RCC->CR & RCC_CR_HSERDY) == 0);
    
    // Настройка Flash Latency
    FLASH->ACR &= ~FLASH_ACR_LATENCY;
    FLASH->ACR |= FLASH_ACR_LATENCY_0;
    
    // Переключение на HSE
    RCC->CFGR &= ~RCC_CFGR_SW;
    RCC->CFGR |= RCC_CFGR_SW_HSE;
    while ((RCC->CFGR & RCC_CFGR_SWS) != RCC_CFGR_SWS_HSE);
    
    // Включение тактирования портов
    RCC->APB2ENR |= RCC_APB2ENR_IOPAEN | RCC_APB2ENR_IOPBEN | RCC_APB2ENR_IOPCEN;
}

// Инициализация GPIO
void GPIO_Init(void) {
    // Настройка LED (PC13) - выход Open-Drain
    GPIOC->CRH &= ~(GPIO_CRH_MODE13 | GPIO_CRH_CNF13);
    GPIOC->CRH |= GPIO_CRH_MODE13_0; // Output mode, max speed 10 MHz
    GPIOC->CRH |= GPIO_CRH_CNF13_0;  // Open-drain
    
    // Настройка кнопки (PA9) - вход с подтяжкой к VCC
    GPIOA->CRH &= ~(GPIO_CRH_MODE9 | GPIO_CRH_CNF9);
    GPIOA->CRH |= GPIO_CRH_CNF9_1;    // Input with pull-up/pull-down
    GPIOA->ODR |= BUTTON_PIN;          // Pull-up
    
    // Настройка HVSP выводов - изначально все на выход
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
    
    // SDO (PB1) - будет динамически меняться
    GPIOB->CRL &= ~(GPIO_CRL_MODE1 | GPIO_CRL_CNF1);
    GPIOB->CRL |= GPIO_CRL_MODE1;     // Output mode, max speed 50 MHz
    
    // Изначально все линии в LOW
    HVSP_RST_PORT->BRR = HVSP_RST_PIN;
    HVSP_SCI_PORT->BRR = HVSP_SCI_PIN;
    HVSP_SII_PORT->BRR = HVSP_SII_PIN;
    HVSP_SDI_PORT->BRR = HVSP_SDI_PIN;
    HVSP_SDO_PORT->BRR = HVSP_SDO_PIN;
}

// Функция мигания светодиодом
void LED_Blink(uint8_t count, uint32_t on_time_ms, uint32_t off_time_ms) {
    for (uint8_t i = 0; i < count; i++) {
        LED_PORT->BRR = LED_PIN; // LED ON
        Delay_ms(on_time_ms);
        LED_PORT->BSRR = LED_PIN; // LED OFF
        Delay_ms(off_time_ms);
    }
}

// Проверка нажатия кнопки с антидребезгом
uint8_t Button_Pressed(void) {
    if ((BUTTON_PORT->IDR & BUTTON_PIN) == 0) { // Кнопка нажата (0 на входе)
        Delay_ms(50); // Задержка для антидребезга
        if ((BUTTON_PORT->IDR & BUTTON_PIN) == 0) {
            return 1; // Кнопка действительно нажата
        }
    }
    return 0;
}

// Вход в режим HVSP программирования
void HVSP_Enter(void) {
    // Настройка SDO как выхода
    GPIOB->CRL &= ~(GPIO_CRL_MODE1 | GPIO_CRL_CNF1);
    GPIOB->CRL |= GPIO_CRL_MODE1; // Output mode
    
    // Устанавливаем начальные состояния
    HVSP_SDI_PORT->BRR = HVSP_SDI_PIN; // LOW
    HVSP_SII_PORT->BRR = HVSP_SII_PIN; // LOW
    HVSP_SDO_PORT->BRR = HVSP_SDO_PIN; // LOW
    HVSP_RST_PORT->BRR = HVSP_RST_PIN; // LOW (12V off)
    
    // Включаем питание целевого МК (3.3V)
    // VCC подключен напрямую к 3.3V - ничего не делаем
    
    Delay_us(20);
    
    // Включаем 12V на RST
    HVSP_RST_PORT->BSRR = HVSP_RST_PIN; // HIGH
    
    Delay_us(10);
    
    // Переключаем SDO на вход
    GPIOB->CRL &= ~(GPIO_CRL_MODE1 | GPIO_CRL_CNF1);
    GPIOB->CRL |= GPIO_CRL_CNF1_0; // Input with pull-up/pull-down
    GPIOB->ODR |= HVSP_SDO_PIN;     // Pull-up
    
    Delay_us(300);
}

// Выход из режима HVSP программирования
void HVSP_Exit(void) {
    HVSP_SCI_PORT->BRR = HVSP_SCI_PIN; // LOW
    // Выключаем питание целевого МК
    // VCC отключен - ничего не делаем
    HVSP_RST_PORT->BRR = HVSP_RST_PIN; // LOW (12V off)
}

// Основная функция передачи данных по HVSP
uint8_t HVSP_ShiftOut(uint8_t val1, uint8_t val2) {
    uint16_t inBits = 0;
    
    // Ждем пока SDO станет HIGH
    while ((HVSP_SDO_PORT->IDR & HVSP_SDO_PIN) == 0);
    
    uint16_t dout = (uint16_t)val1 << 2;
    uint16_t iout = (uint16_t)val2 << 2;
    
    for (int8_t ii = 7; ii >= 0; ii--) {
        // Устанавливаем SDI
        if (dout & (1 << ii)) {
            HVSP_SDI_PORT->BSRR = HVSP_SDI_PIN;
        } else {
            HVSP_SDI_PORT->BRR = HVSP_SDI_PIN;
        }
        
        // Устанавливаем SII
        if (iout & (1 << ii)) {
            HVSP_SII_PORT->BSRR = HVSP_SII_PIN;
        } else {
            HVSP_SII_PORT->BRR = HVSP_SII_PIN;
        }
        
        inBits <<= 1;
        if (HVSP_SDO_PORT->IDR & HVSP_SDO_PIN) {
            inBits |= 1;
        }
        
        // Тактовый импульс
        HVSP_SCI_PORT->BSRR = HVSP_SCI_PIN; // HIGH
        Delay_us(1);
        HVSP_SCI_PORT->BRR = HVSP_SCI_PIN;  // LOW
        Delay_us(1);
    }
    
    return (uint8_t)(inBits >> 2);
}

// Чтение сигнатуры
uint16_t HVSP_ReadSignature(void) {
    uint16_t sig = 0;
    uint8_t val;
    
    for (uint8_t i = 0; i < 3; i++) {
        HVSP_ShiftOut(0x08, 0x4C);
        HVSP_ShiftOut(i, 0x0C);
        HVSP_ShiftOut(0x00, 0x68);
        val = HVSP_ShiftOut(0x00, 0x6C);
        sig = (sig << 8) + val;
    }
    
    return sig;
}

// Запись фьюза
void HVSP_WriteFuse(uint16_t fuse, uint8_t value) {
    HVSP_ShiftOut(0x40, 0x4C);
    HVSP_ShiftOut(value, 0x2C);
    HVSP_ShiftOut(0x00, (uint8_t)(fuse >> 8));
    HVSP_ShiftOut(0x00, (uint8_t)fuse);
    Delay_ms(10); // Задержка для программирования
}

// Чтение фьюзов
void HVSP_ReadFuses(void) {
    uint8_t val;
    
    // Low fuse
    HVSP_ShiftOut(0x04, 0x4C);
    HVSP_ShiftOut(0x00, 0x68);
    val = HVSP_ShiftOut(0x00, 0x6C);
    
    // High fuse
    HVSP_ShiftOut(0x04, 0x4C);
    HVSP_ShiftOut(0x00, 0x7A);
    val = HVSP_ShiftOut(0x00, 0x7E);
    
    // Extended fuse (для ATtiny13 не используется, но оставим)
    HVSP_ShiftOut(0x04, 0x4C);
    HVSP_ShiftOut(0x00, 0x6A);
    val = HVSP_ShiftOut(0x00, 0x6E);
}

// Простые функции задержки
void Delay_ms(uint32_t ms) {
    for (uint32_t i = 0; i < ms * 1000; i++) {
        __NOP();
    }
}

void Delay_us(uint32_t us) {
    for (uint32_t i = 0; i < us; i++) {
        __NOP();
    }
}
