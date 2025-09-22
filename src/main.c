#define STM32F103x6
#include "stm32f1xx.h"

// Pin definitions for STM32F103C6T6
#define RST_PIN     (1 << 0)    // PB0 - RST (Output to transistor for !RESET)
#define SCI_PIN     (1 << 4)    // PB4 - SCI (Target Clock Input)
#define SDO_PIN     (1 << 1)    // PB1 - SDO (Target Data Output)
#define SII_PIN     (1 << 6)    // PA6 - SII (Target Instruction Input)  
#define SDI_PIN     (1 << 7)    // PA7 - SDI (Target Data Input)
#define BUTTON_PIN  (1 << 9)    // PA9 - Button input
#define LED_PIN     (1 << 13)   // PC13 - LED indicator

// HVSP Protocol definitions
#define HFUSE 0x747C
#define LFUSE 0x646C
#define EFUSE 0x666E
#define ATTINY13 0x9007

// Global variables
volatile uint32_t sys_tick = 0;
volatile uint8_t button_pressed = 0;

// --- System tick handler for delays ---
void SysTick_Handler(void) {
    sys_tick++;
}

// --- Delay functions ---
void delay_ms(uint32_t ms) {
    uint32_t start = sys_tick;
    while ((sys_tick - start) < ms);
}

void delay_us(uint32_t us) {
    // Simple delay loop for microseconds (approximate at 8MHz)
    volatile uint32_t count = us * 2;
    while (count--);
}

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
    
    // Включаем тактирование GPIOA, GPIOB, GPIOC, AFIO
    RCC->APB2ENR |= RCC_APB2ENR_IOPAEN | RCC_APB2ENR_IOPBEN | RCC_APB2ENR_IOPCEN | RCC_APB2ENR_AFIOEN;
    
    // Configure SysTick for 1ms interrupts
    SysTick_Config(8000); // 8MHz / 8000 = 1kHz (1ms)
}

// --- GPIO Initialization ---
void GPIO_Init(void) {
    // Configure GPIOA pins
    // PA6 (SII) and PA7 (SDI) as output push-pull 10MHz
    GPIOA->CRL &= ~(0xF << (6 * 4) | 0xF << (7 * 4));
    GPIOA->CRL |= (0x1 << (6 * 4) | 0x1 << (7 * 4)); // Output push-pull 10MHz
    
    // PA9 (Button) as input with pull-up
    GPIOA->CRH &= ~(0xF << ((9-8) * 4));
    GPIOA->CRH |= (0x8 << ((9-8) * 4)); // Input pull-up/pull-down
    GPIOA->BSRR = BUTTON_PIN; // Enable pull-up
    
    // Configure GPIOB pins
    // PB0 (RST), PB1 (SDO), PB4 (SCI) as output push-pull 10MHz initially
    GPIOB->CRL &= ~(0xF << (0 * 4) | 0xF << (1 * 4) | 0xF << (4 * 4));
    GPIOB->CRL |= (0x1 << (0 * 4) | 0x1 << (1 * 4) | 0x1 << (4 * 4));
    
    // Configure GPIOC pins  
    // PC13 (LED) as output push-pull 10MHz
    GPIOC->CRH &= ~(0xF << ((13-8) * 4));
    GPIOC->CRH |= (0x1 << ((13-8) * 4)); // Output push-pull 10MHz
    
    // Set initial states
    GPIOB->BSRR = RST_PIN;  // RST HIGH (12V off)
    GPIOC->BSRR = LED_PIN;  // LED OFF (PC13 is inverted)
}

// --- LED Control Functions ---
void LED_On(void) {
    GPIOC->BRR = LED_PIN;  // PC13 low = LED on
}

void LED_Off(void) {
    GPIOC->BSRR = LED_PIN; // PC13 high = LED off  
}

void LED_Blink(uint8_t count, uint16_t on_time, uint16_t off_time) {
    for (uint8_t i = 0; i < count; i++) {
        LED_On();
        delay_ms(on_time);
        LED_Off();
        if (i < count - 1) {
            delay_ms(off_time);
        }
    }
}

// --- Button handling ---
uint8_t Button_Pressed(void) {
    static uint8_t last_state = 1;
    static uint32_t debounce_time = 0;
    
    uint8_t current_state = (GPIOA->IDR & BUTTON_PIN) ? 1 : 0;
    
    if (current_state != last_state) {
        debounce_time = sys_tick;
    }
    
    if ((sys_tick - debounce_time) > 50) { // 50ms debounce
        if (current_state == 0 && last_state == 1) { // Button pressed
            last_state = current_state;
            return 1;
        }
        last_state = current_state;
    }
    
    return 0;
}

// --- HVSP Protocol Functions ---

// Set pin as output
void Set_Pin_Output(GPIO_TypeDef* gpio, uint32_t pin) {
    if (gpio == GPIOA) {
        if (pin == SII_PIN) {
            GPIOA->CRL &= ~(0xF << (6 * 4));
            GPIOA->CRL |= (0x1 << (6 * 4));
        } else if (pin == SDI_PIN) {
            GPIOA->CRL &= ~(0xF << (7 * 4));
            GPIOA->CRL |= (0x1 << (7 * 4));
        }
    } else if (gpio == GPIOB) {
        if (pin == SDO_PIN) {
            GPIOB->CRL &= ~(0xF << (1 * 4));
            GPIOB->CRL |= (0x1 << (1 * 4));
        }
    }
}

// Set pin as input
void Set_Pin_Input(GPIO_TypeDef* gpio, uint32_t pin) {
    if (gpio == GPIOB && pin == SDO_PIN) {
        GPIOB->CRL &= ~(0xF << (1 * 4));
        GPIOB->CRL |= (0x4 << (1 * 4)); // Input floating
    }
}

// Digital write
void Digital_Write(GPIO_TypeDef* gpio, uint32_t pin, uint8_t state) {
    if (state) {
        gpio->BSRR = pin;
    } else {
        gpio->BRR = pin;
    }
}

// Digital read
uint8_t Digital_Read(GPIO_TypeDef* gpio, uint32_t pin) {
    return (gpio->IDR & pin) ? 1 : 0;
}

// HVSP Shift Out function
uint8_t HVSP_ShiftOut(uint8_t val1, uint8_t val2) {
    uint8_t inBits = 0;
    
    // Wait until SDO goes high
    while (!Digital_Read(GPIOB, SDO_PIN));
    
    uint16_t dout = (uint16_t)val1 << 2;
    uint16_t iout = (uint16_t)val2 << 2;
    
    for (int ii = 10; ii >= 0; ii--) {
        Digital_Write(GPIOA, SDI_PIN, !!(dout & (1 << ii)));
        Digital_Write(GPIOA, SII_PIN, !!(iout & (1 << ii)));
        inBits <<= 1;
        inBits |= Digital_Read(GPIOB, SDO_PIN);
        Digital_Write(GPIOB, SCI_PIN, 1);
        Digital_Write(GPIOB, SCI_PIN, 0);
    }
    
    return inBits >> 2;
}

// Write fuse
void HVSP_WriteFuse(uint16_t fuse, uint8_t val) {
    HVSP_ShiftOut(0x40, 0x4C);
    HVSP_ShiftOut(val, 0x2C);
    HVSP_ShiftOut(0x00, (uint8_t)(fuse >> 8));
    HVSP_ShiftOut(0x00, (uint8_t)fuse);
}

// Read fuses (simplified for ATtiny13)
void HVSP_ReadFuses(void) {
    uint8_t val;
    
    // Read LFuse
    HVSP_ShiftOut(0x04, 0x4C);
    HVSP_ShiftOut(0x00, 0x68);
    val = HVSP_ShiftOut(0x00, 0x6C);
    (void)val; // Suppress unused variable warning
    
    // Read HFuse  
    HVSP_ShiftOut(0x04, 0x4C);
    HVSP_ShiftOut(0x00, 0x7A);
    val = HVSP_ShiftOut(0x00, 0x7E);
    (void)val; // Suppress unused variable warning
}

// Read signature
uint16_t HVSP_ReadSignature(void) {
    uint16_t sig = 0;
    uint8_t val;
    
    for (int ii = 1; ii < 3; ii++) {
        HVSP_ShiftOut(0x08, 0x4C);
        HVSP_ShiftOut(ii, 0x0C);
        HVSP_ShiftOut(0x00, 0x68);
        val = HVSP_ShiftOut(0x00, 0x6C);
        sig = (sig << 8) + val;
    }
    
    return sig;
}

// Main HVSP recovery function
uint8_t HVSP_RecoverATtiny13(void) {
    // Set SDO to output initially
    Set_Pin_Output(GPIOB, SDO_PIN);
    
    // Initialize pins
    Digital_Write(GPIOA, SDI_PIN, 0);
    Digital_Write(GPIOA, SII_PIN, 0);
    Digital_Write(GPIOB, SDO_PIN, 0);
    Digital_Write(GPIOB, RST_PIN, 1);  // 12V Off
    
    delay_us(20);
    Digital_Write(GPIOB, RST_PIN, 0);  // 12V On
    delay_us(10);
    
    // Set SDO to input
    Set_Pin_Input(GPIOB, SDO_PIN);
    delay_us(300);
    
    // Read signature
    uint16_t sig = HVSP_ReadSignature();
    
    // Check if it's ATtiny13
    if (sig != ATTINY13) {
        // Turn off programming voltage
        Digital_Write(GPIOB, SCI_PIN, 0);
        Digital_Write(GPIOB, RST_PIN, 1); // 12V Off
        return 0; // Error
    }
    
    // Read current fuses
    HVSP_ReadFuses();
    
    // Write new fuses for ATtiny13
    HVSP_WriteFuse(LFUSE, 0x6A);
    HVSP_WriteFuse(HFUSE, 0xFF);
    
    // Read fuses again to verify
    HVSP_ReadFuses();
    
    // Turn off programming voltage
    Digital_Write(GPIOB, SCI_PIN, 0);
    Digital_Write(GPIOB, RST_PIN, 1); // 12V Off
    
    return 1; // Success
}

// --- Main Function ---
int main(void) {
    System_Init();
    GPIO_Init();
    
    // Startup indication: 3 blinks in 1.5 seconds
    delay_ms(200);
    LED_Blink(3, 200, 300);
    
    while (1) {
        // Waiting state: blink once per second
        LED_Blink(1, 100, 900);
        
        // Check for button press
        if (Button_Pressed()) {
            // Button pressed, start recovery process
            LED_Off();
            delay_ms(100);
            
            // Attempt fuse recovery
            if (HVSP_RecoverATtiny13()) {
                // Success: 5 blinks in 3 seconds
                LED_Blink(5, 300, 300);
            } else {
                // Error: 5 fast blinks in 1 second  
                LED_Blink(5, 100, 100);
            }
            
            // Wait a bit before returning to waiting state
            delay_ms(2000);
        }
    }
}
