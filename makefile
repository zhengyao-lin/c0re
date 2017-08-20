# main project

export BASE := $(PWD)

export CC := gcc
export OBJCOPY := objcopy
export TERMINAL := gnome-terminal
export QEMU := qemu-system-i386
export BOCHS := bochs

export INCLUDES := $(BASE)
export CFLAGS := -Wall -c -g -ggdb -m32 -I$(INCLUDES) -fno-builtin -fno-stack-protector -Os -nostdinc
export LDFLAGS := -nostdlib -m $(shell $(LD) -V | grep elf_i386 2>/dev/null)

export QEMUOPTS := -m 512

export OUTPUT := $(BASE)/bin

FINAL := $(OUTPUT)/c0re.img

MAIN: tool img

boot: NOSKIP
	cd boot; make
	
pub: NOSKIP
	cd pub; make
	
output: NOSKIP
	$(shell if [ ! -d $(OUTPUT) ]; then mkdir $(OUTPUT); fi)
	
kernel: output pub
	cd kernel; make
	$(LD) $(LDFLAGS) -T tool/kernel.ld -o $(OUTPUT)/kernel.elf pub/pub.o kernel/kernel.o
	
img: output tool pub kernel boot
	$(OUTPUT)/sign boot/boot $(OUTPUT)/boot.img
	dd if=/dev/zero of=$(FINAL) count=10000
	dd if=$(OUTPUT)/boot.img of=$(FINAL) conv=notrunc
	dd if=$(OUTPUT)/kernel.elf of=$(FINAL) seek=1 conv=notrunc
	
tool: output
	cd tool; make

debug: img
	$(QEMU) -S -s -parallel stdio -drive format=raw,file=$(FINAL) -serial null $(QEMUOPTS) &
	sleep 2
	$(TERMINAL) -e "gdb -q -tui -x tool/gdbinit"

run: img
	$(QEMU) -drive format=raw,file=$(FINAL) $(QEMUOPTS) -parallel stdio

bochs: img
	$(BOCHS) -qf tool/bochs.bxrc &
	sleep 2
	$(TERMINAL) -e "gdb -q -tui -x tool/gdbinit"
	
clean: NOSKIP
	cd pub; make clean
	cd kernel; make clean
	cd boot; make clean
	cd tool; make clean
	rm -f $(FINAL) boot.img kernel.elf
	rm -rf $(OUTPUT)

NOSKIP:
