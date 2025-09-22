#include "stm32f1xx.h"

/* ======= Макросы для GPIO (ваши оригинальные) ======= */
#define pin_set(PORT,PIN)     ((PORT)->BSRR = (1U << (PIN)))
#define pin_clear(PORT,PIN)   ((PORT)->BRR  = (1U << (PIN)))
#define pin_read(PORT,PIN)    (((PORT)->IDR >> (PIN)) & 1U)

/* ======= Конфигурация пинов ======= */
#define LED_PORT    GPIOC
#define LED_PIN     13

#define BUTTON_PORT GPIOA
#define BUTTON_PIN  9

#define VCC_CONTROL_PIN_USED 0
#define VCC_PORT    GPIOA
#define VCC_PIN     0

#define RST_PORT    GPIOB
#define RST_PIN     0

#define SCI_PORT    GPIOB
#define SCI_PIN     4

#define SII_PORT    GPIOA
#define SII_PIN     6

#define SDI_PORT    GPIOA
#define SDI_PIN     7

#define SDO_PORT    GPIOB
#define SDO_PIN     1

/* ======= Тайминги ======= */
#define SCI_FREQ_HZ         100000U
#define SCI_HALF_PERIOD_US  (1000000U / (2 * SCI_FREQ_HZ))

#define DEBOUNCE_DELAY_US   50000U
#define RESET_PULSE_US      10U
#define HVSP_SETUP_US       1000U
#define PROGRAM_DELAY_US    10000U

/* ======= Прототипы функций ======= */
static void SystemClock_Config(void);
static void GPIO_Init_All(void);
static void DWT_Init(void);
static inline void delay_us(uint32_t us);
static void led_blink_pattern(uint8_t times, uint32_t delay_ms);
static void hvsp_shiftOut(uint16_t data);
static uint16_t hvsp_read(uint16_t instr);
static uint16_t hvsp_readSignature(void);
static void hvsp_writeFuse(uint16_t cmd);

/* ======= Реализация функций ======= */

/* Настройка системного тактирования */
static void SystemClock_Config(void) {
    // Включение HSI (8 MHz)
    RCC->CR |= RCC_CR_HSION;
    while ((RCC->CR & RCC_CR_HSIRDY) == 0) {
        // Ожидание готовности HSI
    }
    
    // Настройка задержки флэш-памяти
    FLASH->ACR |= FLASH_ACR_LATENCY_1;
    
    // Выбор HSI как источника системной частоты
    RCC->CFGR &= ~RCC_CFGR_SW;
    RCC->CFGR |= RCC_CFGR_SW_HSI;
    while ((RCC->CFGR & RCC_CFGR_SWS) != RCC_CFGR_SWS_HSI) {
        // Ожидание переключения
    }
}

/* Инициализация GPIO */
static void GPIO_Init_All(void) {
    // Включение тактирования портов
    RCC->APB2ENR |= RCC_APB2ENR_IOPAEN | 
                    RCC_APB2ENR_IOPBEN | 
                    RCC_APB2ENR_IOPCEN | 
                    RCC_APB2ENR_AFIOEN;
    
    // Барьер памяти
    __DSB();

    // Освобождение PB4 от JTAG
    AFIO->MAPR &= ~AFIO_MAPR_SWJ_CFG_Msk;
    AFIO->MAPR |= AFIO_MAPR_SWJ_CFG_JTAGDISABLE;

    // Настройка PC13 (LED) как выход 2MHz push-pull
    GPIOC->CRH &= ~(0xFU << ((LED_PIN - 8) * 4));
    GPIOC->CRH |=  (0x2U << ((LED_PIN - 8) * 4));
    pin_set(LED_PORT, LED_PIN); // LED off

    // Настройка PA9 (кнопка) как вход с подтяжкой вверх
    GPIOA->CRH &= ~(0xFU << ((BUTTON_PIN - 8) * 4));
    GPIOA->CRH |=  (0x8U << ((BUTTON_PIN - 8) * 4));
    GPIOA->ODR |= (1U << BUTTON_PIN);

    // Настройка PB0 (RST control) как выход 2MHz
    GPIOB->CRL &= ~(0xFU << (RST_PIN * 4));
    GPIOB->CRL |=  (0x2U << (RST_PIN * 4));
    pin_set(RST_PORT, RST_PIN); // По умолчанию высокий уровень

#if VCC_CONTROL_PIN_USED
    // Настройка PA0 (VCC control) как выход 2MHz
    GPIOA->CRL &= ~(0xFU << (VCC_PIN * 4));
    GPIOA->CRL |=  (0x2U << (VCC_PIN * 4));
    pin_clear(VCC_PORT, VCC_PIN); // По умолчанию выключено
#endif

    // Настройка PB4 (SCI) как выход 2MHz
    GPIOB->CRL &= ~(0xFU << (SCI_PIN * 4));
    GPIOB->CRL |=  (0x2U << (SCI_PIN * 4));
    pin_clear(SCI_PORT, SCI_PIN);

    // Настройка PA6 (SII) как выход 2MHz
    GPIOA->CRL &= ~(0xFU << (SII_PIN * 4));
    GPIOA->CRL |=  (0x2U << (SII_PIN * 4));
    pin_clear(SII_PORT, SII_PIN);

    // Настройка PA7 (SDI) как выход 2MHz
    GPIOA->CRL &= ~(0xFU << (SDI_PIN * 4));
    GPIOA->CRL |=  (0x2U << (SDI_PIN * 4));
    pin_clear(SDI_PORT, SDI_PIN);

    // Настройка PB1 (SDO) как вход с подтяжкой вверх
    GPIOB->CRL &= ~(0xFU << (SDO_PIN * 4));
    GPIOB->CRL |=  (0x8U << (SDO_PIN * 4));
    GPIOB->ODR |= (1U << SDO_PIN);
}

/* Инициализация DWT для точных задержек */
static void DWT_Init(void) {
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CYCCNT = 0;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
}

/* Точная задержка в микросекундах */
static inline void delay_us(uint32_t us) {
    uint32_t start = DWT->CYCCNT;
    uint32_t cycles = (SystemCoreClock / 1000000U) * us;
    
    while ((DWT->CYCCNT - start) < cycles) {
        // Активное ожидание
    }
}

/* Моргание светодиодом */
static void led_blink_pattern(uint8_t times, uint32_t delay_ms) {
    for (uint8_t i = 0; i < times; i++) {
        pin_clear(LED_PORT, LED_PIN); // LED on
        delay_us(delay_ms * 1000U);
        pin_set(LED_PORT, LED_PIN);   // LED off
        if (i < times - 1) {
            delay_us(delay_ms * 1000U);
        }
    }
}

/* HVSP - передача данных */
static void hvsp_shiftOut(uint16_t data) {
    for (int8_t i = 10; i >= 0; i--) {
        // Установка SII
        if (data & (1U << i)) {
            pin_set(SII_PORT, SII_PIN);
        } else {
            pin_clear(SII_PORT, SII_PIN);
        }

        // Установка SDI
        if (data & (1U << i)) {
            pin_set(SDI_PORT, SDI_PIN);
        } else {
            pin_clear(SDI_PORT, SDI_PIN);
        }

        // Тактовый импульс SCI
        delay_us(SCI_HALF_PERIOD_US);
        pin_set(SCI_PORT, SCI_PIN);
        delay_us(SCI_HALF_PERIOD_US);
        pin_clear(SCI_PORT, SCI_PIN);
    }
}

/* HVSP - чтение данных */
static uint16_t hvsp_read(uint16_t instr) {
    uint16_t result = 0;
    
    for (int8_t i = 10; i >= 0; i--) {
        // Установка SII
        if (instr & (1U << i)) {
            pin_set(SII_PORT, SII_PIN);
        } else {
            pin_clear(SII_PORT, SII_PIN);
        }

        // Тактовый импульс SCI
        delay_us(SCI_HALF_PERIOD_US);
        pin_set(SCI_PORT, SCI_PIN);
        delay_us(SCI_HALF_PERIOD_US);

        // Чтение SDO
        if (pin_read(SDO_PORT, SDO_PIN)) {
            result |= (1U << i);
        }

        pin_clear(SCI_PORT, SCI_PIN);
        delay_us(SCI_HALF_PERIOD_US);
    }
    
    return result;
}

/* Чтение сигнатуры ATtiny */
static uint16_t hvsp_readSignature(void) {
    return hvsp_read(0x080U); // Команда чтения сигнатуры
}

/* Запись fuse-битов */
static void hvsp_writeFuse(uint16_t cmd) {
    hvsp_shiftOut(cmd);
    delay_us(PROGRAM_DELAY_US); // Задержка программирования
}

/* ======= Главная функция ======= */
int main(void) {
    // Инициализация системы
    SystemClock_Config();
    DWT_Init();
    GPIO_Init_All();

    // Тестовое моргание при старте
    led_blink_pattern(3, 200);

    // Основной цикл
    while (1) {
        // Проверка нажатия кнопки (активный низкий уровень)
        if (!pin_read(BUTTON_PORT, BUTTON_PIN)) {
            // Антидребезг
            delay_us(DEBOUNCE_DELAY_US);
            
            if (!pin_read(BUTTON_PORT, BUTTON_PIN)) {
#if VCC_CONTROL_PIN_USED
                // Подача VCC на ATtiny
                pin_set(VCC_PORT, VCC_PIN);
                delay_us(20);
#endif

                // Подача 12V на RESET (активный низкий)
                pin_clear(RST_PORT, RST_PIN);
                delay_us(RESET_PULSE_US);

                // Ожидание инициализации HVSP
                delay_us(HVSP_SETUP_US);

                // Чтение и проверка сигнатуры
                uint16_t signature = hvsp_readSignature();
                
                if ((signature & 0xFFU) == 0x0BU) { // ATtiny13
                    // Программирование fuse-битов
                    hvsp_writeFuse(0x040U); // LFUSE
                    hvsp_writeFuse(0x142U); // HFUSE
                    
                    // Сигнал успеха
                    led_blink_pattern(5, 100);
                } else {
                    // Сигнал ошибки
                    led_blink_pattern(2, 500);
                }

                // Отключение 12V от RESET
                pin_set(RST_PORT, RST_PIN);

#if VCC_CONTROL_PIN_USED
                // Отключение VCC
                pin_clear(VCC_PORT, VCC_PIN);
#endif

                // Задержка перед следующим нажатием
                delay_us(1000000U); // 1 секунда
            }
        }
    }
}