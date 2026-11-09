
# ============================================================
# ARIES V3 - Bare-Metal Firmware Makefile
# Target: VEGA RISC-V (THEJAS32)
# Toolchain: Arduino VEGA GCC 13.2.0
# ============================================================

# -------------------- Project -------------------------------

PROJECT = firmware

BUILD = build
SRC_DIR = src
STARTUP_DIR = startup
INCLUDE_DIR = include
LINKER_DIR = linker

# -------------------- Toolchain -----------------------------

TOOLCHAIN = $(HOME)/.arduino15/packages/vega/tools/riscv32-vega-elf-gcc/002/bin

CC      = $(TOOLCHAIN)/riscv32-vega-elf-gcc
CXX     = $(TOOLCHAIN)/riscv32-vega-elf-g++
AS      = $(TOOLCHAIN)/riscv32-vega-elf-gcc
OBJCOPY = $(TOOLCHAIN)/riscv32-vega-elf-objcopy
OBJDUMP = $(TOOLCHAIN)/riscv32-vega-elf-objdump
SIZE    = $(TOOLCHAIN)/riscv32-vega-elf-size
NM      = $(TOOLCHAIN)/riscv32-vega-elf-nm
READELF = $(TOOLCHAIN)/riscv32-vega-elf-readelf

# -------------------- Architecture --------------------------

ARCH = rv32i_zicsr
ABI  = ilp32

# -------------------- Linker script -------------------------

LINKER = $(LINKER_DIR)/link1.lds

# -------------------- Output files --------------------------

ELF = $(BUILD)/$(PROJECT).elf
BIN = $(BUILD)/$(PROJECT).bin
MAP = $(BUILD)/$(PROJECT).map

# -------------------- Source files --------------------------

C_SOURCES = $(wildcard $(SRC_DIR)/*.c)
ASM_SOURCES = $(wildcard $(STARTUP_DIR)/*.S)

C_OBJECTS = $(patsubst $(SRC_DIR)/%.c,$(BUILD)/%.o,$(C_SOURCES))
ASM_OBJECTS = $(patsubst $(STARTUP_DIR)/%.S,$(BUILD)/%.o,$(ASM_SOURCES))

OBJECTS = $(ASM_OBJECTS) $(C_OBJECTS)

# -------------------- Compiler flags -----------------------

CFLAGS = \
	-O3 \
	-march=$(ARCH) \
	-mabi=$(ABI) \
	-fpeel-loops \
	-ffunction-sections \
	-fdata-sections \
	-ffreestanding \
	-Wall \
	-g \
	-include sys/cdefs.h \
	-I$(INCLUDE_DIR)

# -------------------- Assembly flags ------------------------

ASFLAGS = \
	-march=$(ARCH) \
	-mabi=$(ABI) \
	-g \
	-I$(INCLUDE_DIR)

# -------------------- Linker flags --------------------------

LDFLAGS = \
	-march=$(ARCH) \
	-mabi=$(ABI) \
	-T $(LINKER) \
	-nostartfiles \
	-nostdlib \
	-Wl,-N \
	-Wl,--gc-sections \
	-Wl,--wrap=malloc \
	-Wl,--wrap=free \
	-Wl,--wrap=sbrk \
	-Wl,-Map=$(MAP)

# IMPORTANT:
# Keep libraries separate from LDFLAGS.
# Object files must appear BEFORE these libraries in the link command.

LDLIBS = \
	-Wl,--start-group \
	-lm \
	-lstdc++ \
	-lc \
	-lgloss \
	-Wl,--end-group \
	-lgcc

# -------------------- Build rules ---------------------------

.PHONY: all clean flash size disassemble symbols

all: $(ELF)

# Create build directory
$(BUILD):
	mkdir -p $(BUILD)

# Compile C source files
$(BUILD)/%.o: $(SRC_DIR)/%.c | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

# Assemble startup files
$(BUILD)/%.o: $(STARTUP_DIR)/%.S | $(BUILD)
	$(AS) $(ASFLAGS) -c $< -o $@

# Link firmware
$(ELF): $(OBJECTS) $(LINKER) | $(BUILD)
	$(CXX) $(LDFLAGS) $(OBJECTS) $(LDLIBS) -o $@
	$(SIZE) -B $@

# Convert ELF to raw binary
$(BIN): $(ELF)
	$(OBJCOPY) -O binary $< $@

# Display firmware size
size: $(ELF)
	$(SIZE) -B $(ELF)

# Disassemble firmware
disassemble: $(ELF)
	$(OBJDUMP) -d $(ELF)

# Display symbols
symbols: $(ELF)
	$(NM) $(ELF)

# Clean generated files
clean:
	rm -rf $(BUILD)

# ============================================================
# End of Makefile
# ============================================================
