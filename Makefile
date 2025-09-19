# Makefile для STM32F103C6T6 с CMSIS include

# Инструменты
CC = arm-none-eabi-gcc
OBJCOPY = arm-none-eabi-objcopy

# Флаги компиляции и линковки
CFLAGS  = -Wall -Wextra -Os -ffreestanding -fno-builtin -mcpu=cortex-m3 -mthumb -Iinclude -Iinclude/CMSIS
LDFLAGS = -T STM32F103X6_FLASH.ld -nostdlib -Wl,-Map=build/firmware.map,--gc-sections

# Исходники
SRC = src/main.c src/system_stm32f1xx.c src/init.c
ASM = src/startup_stm32f103xb.s

# Каталог сборки и имя прошивки
BUILD_DIR = build
TARGET = $(BUILD_DIR)/firmware

.PHONY: all clean

# Главная цель — бинарник
all: $(TARGET).bin

# Создание каталога сборки
$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

# Сборка ELF-файла
$(TARGET).elf: $(BUILD_DIR) $(SRC) $(ASM)
	$(CC) $(CFLAGS) $(SRC) $(ASM) $(LDFLAGS) -o $@

# Преобразование ELF в BIN
$(TARGET).bin: $(TARGET).elf
	$(OBJCOPY) -O binary $< $@

# Очистка сборки
clean:
	rm -rf $(BUILD_DIR)
