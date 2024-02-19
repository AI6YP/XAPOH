CC = clang
OC = llvm-objcopy
OD = llvm-objdump

ASFLAGS += -c
# ASFLAGS += -std=c11
ASFLAGS += -Os
ASFLAGS += -Wall
ASFLAGS += --target=riscv32 -march=rv32imac -mabi=ilp32
# ASFLAGS += -mcmodel=medlow

# C compilation directives
CFLAGS += -c
# CFLAGS += -std=c11
CFLAGS += -Wall
CFLAGS += -Os
# CFLAGS += -g
# CFLAGS += -mcmodel=medlow
CFLAGS += --target=riscv32 -march=rv32imac -mabi=ilp32
CFLAGS += -mno-relax

# Linker directives.
LFLAGS += -Wall
LFLAGS += -Wl,--no-relax
LFLAGS += -nostdlib
# LFLAGS += -nostartfiles
# LFLAGS += -mcmodel=medlow
LFLAGS += --target=riscv32 -march=rv32imac -mabi=ilp32
LFLAGS += -T./../common/gd32vf103xb.ld

AS_SRC = ./../common/gd32vf103xb_boot.S
C_SRC = ./main.c

INCLUDE = -I./../common

OBJS  = $(AS_SRC:.S=.o)
OBJS += $(C_SRC:.c=.o)

.PHONY: all
all: main.bin main.asm

%.o: %.S
	$(CC) -x assembler-with-cpp $(ASFLAGS) $(INCLUDE) $< -o $@

%.o: %.c
	$(CC) $(CFLAGS) $(INCLUDE) $< -o $@

main.elf: $(OBJS)
	$(CC) $^ $(LFLAGS) -o $@

main.bin: main.elf
	$(OC) -S -O binary $< $@

main.asm: main.elf
	$(OD) -d -r $< > $@

dfu: main.bin main.asm
	dfu-util -a 0 -d 28e9:0189 --dfuse-address 0x08000000:leave -D main.bin

upload: main.bin
	openocd -f ../f232h-gd32v.cfg -c "program main.bin exit 0x08000000"

.PHONY: clean
clean:
	rm -f $(OBJS) main.elf main.bin main.asm main.o
