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
    // More accurate delay for HVSP timing at 8MHz
    volatile uint32_t count = us * 8;
    while (count--) {
        __NOP();
    }
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
    // PB0 (RST), PB1 (SDO), PB4 (SCI) as output push-pull 10MHz
    GPIOB->CRL &= ~(0xF << (0 * 4) | 0xF << (1 * 4) | 0xF << (4 * 4));
    GPIOB->CRL |= (0x1 << (0 * 4) | 0x1 << (1 * 4) | 0x1 << (4 * 4));
    
    // Configure GPIOC pins  
    // PC13 (LED) as output push-pull 10MHz
    GPIOC->CRH &= ~(0xF << ((13-8) * 4));
    GPIOC->CRH |= (0x1 << ((13-8) * 4)); // Output push-pull 10MHz
    
    // Set initial states
    GPIOA->BRR = SDI_PIN | SII_PIN;     // SDI, SII LOW
    GPIOB->BSRR = RST_PIN;              // RST HIGH (12V off)
    GPIOB->BRR = SCI_PIN;               // SCI LOW
    GPIOC->BSRR = LED_PIN;              // LED OFF (PC13 is inverted)
}

// --- LED Control Functions ---
void LED_On(void) {
    GPIOC->BRR = LED_PIN;  // PC13 low = LED on
}

void LED_Off(void) {
    GPIOC->BSRR = LED_PIN; // PC13 high = LED off  
}

void LED_Toggle(void) {
    if (GPIOC->ODR & LED_PIN) {
        LED_On();
    } else {
        LED_Off();
    }
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

// --- Simplified button handling ---
uint8_t Button_Pressed(void) {
    static uint8_t last_state = 1;
    uint8_t current_state = (GPIOA->IDR & BUTTON_PIN) ? 1 : 0;
    
    if (current_state == 0 && last_state == 1) { // Button pressed (high to low)
        delay_ms(50); // Simple debounce
        current_state = (GPIOA->IDR & BUTTON_PIN) ? 1 : 0;
        if (current_state == 0) {
            last_state = current_state;
            return 1;
        }
    }
    last_state = current_state;
    return 0;
}

// --- HVSP Protocol Functions ---

// Set SDO as input
void SDO_Input(void) {
    GPIOB->CRL &= ~(0xF << (1 * 4));
    GPIOB->CRL |= (0x4 << (1 * 4)); // Input floating
}

// Set SDO as output
void SDO_Output(void) {
    GPIOB->CRL &= ~(0xF << (1 * 4));
    GPIOB->CRL |= (0x1 << (1 * 4)); // Output push-pull 10MHz
}

// HVSP Shift Out function
uint8_t HVSP_ShiftOut(uint8_t val1, uint8_t val2) {
    uint8_t inBits = 0;
    
    // Wait until SDO goes high (with timeout)
    uint32_t timeout = 1000;
    while (!(GPIOB->IDR & SDO_PIN) && timeout--) {
        delay_us(1);
    }
    
    if (!timeout) return 0; // Timeout error
    
    uint16_t dout = (uint16_t)val1 << 2;
    uint16_t iout = (uint16_t)val2 << 2;
    
    for (int ii = 10; ii >= 0; ii--) {
        // Set SDI
        if (dout & (1 << ii)) {
            GPIOA->BSRR = SDI_PIN;
        } else {
            GPIOA->BRR = SDI_PIN;
        }
        
        // Set SII
        if (iout & (1 << ii)) {
            GPIOA->BSRR = SII_PIN;
        } else {
            GPIOA->BRR = SII_PIN;
        }
        
        inBits <<= 1;
        inBits |= (GPIOB->IDR & SDO_PIN) ? 1 : 0;
        
        // Clock pulse
        GPIOB->BSRR = SCI_PIN;  // SCI HIGH
        delay_us(2);
        GPIOB->BRR = SCI_PIN;   // SCI LOW
        delay_us(2);
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
    // Debug: Fast blink to show function entered
    LED_Blink(2, 50, 50);
    delay_ms(200);
    
    // Set SDO to output initially
    SDO_Output();
    
    // Initialize pins for HVSP
    GPIOA->BRR = SDI_PIN | SII_PIN;     // SDI, SII LOW
    GPIOB->BRR = SDO_PIN;               // SDO LOW
    GPIOB->BSRR = RST_PIN;              // RST HIGH (12V Off)
    
    delay_us(20);
    GPIOB->BRR = RST_PIN;               // RST LOW (12V On)
    delay_us(10);
    
    // Set SDO to input for reading
    SDO_Input();
    delay_us(300);
    
    // Debug: Another blink to show we're about to read signature
    LED_Blink(3, 50, 50);
    delay_ms(200);
    
    // Read signature
    uint16_t sig = HVSP_ReadSignature();
    
    // Debug: Show signature result
    if (sig == ATTINY13) {
        LED_Blink(1, 500, 0); // Long blink for correct signature
    } else {
        LED_Blink(10, 50, 50); // Fast blinks for wrong signature
        // Turn off programming voltage
        GPIOB->BRR = SCI_PIN;
        GPIOB->BSRR = RST_PIN; // 12V Off
        return 0; // Error
    }
    
    delay_ms(500);
    
    // Read current fuses
    HVSP_ReadFuses();
    
    // Write new fuses for ATtiny13
    HVSP_WriteFuse(LFUSE, 0x6A);
    HVSP_WriteFuse(HFUSE, 0xFF);
    
    // Read fuses again to verify
    HVSP_ReadFuses();
    
    // Turn off programming voltage
    GPIOB->BRR = SCI_PIN;
    GPIOB->BSRR = RST_PIN; // 12V Off
    
    return 1; // Success
}

// --- Main Function ---
int main(void) {
    System_Init();
    GPIO_Init();
    
    // Startup indication: 3 blinks in 1.5 seconds
    delay_ms(200);
    LED_Blink(3, 200, 300);
    LED_Off();
    
    uint32_t last_blink = sys_tick;
    uint8_t led_state = 0;
    
    while (1) {
        // Waiting state: blink once per second
        if ((sys_tick - last_blink) >= 1000) {
            if (led_state) {
                LED_Off();
                led_state = 0;
            } else {
                LED_On();
                led_state = 1;
            }
            last_blink = sys_tick;
        }
        
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
            
            LED_Off();
            // Reset timing for waiting state
            last_blink = sys_tick;
            led_state = 0;
        }
    }
}
