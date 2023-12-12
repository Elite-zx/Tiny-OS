##################
# Author: Zhang Xun
# Time: 2023-12-04
##################

BUILD_DIR =./build
ENTRY_POINT= 0xc0001500
AS = nasm
CC = gcc
LD = ld

LIB = -I lib/ -I lib/kernel/ -I lib/user/ -I kernel/ -I device/ -I thread/ -I userprog/ -I fs/ -I shell/
ASFLAGS = -f elf
CFLAGS = -m32 -Wall $(LIB) -c -fno-builtin -fno-stack-protector
LDFLAGS= -m elf_i386 -Ttext $(ENTRY_POINT) -e main -Map $(BUILD_DIR)/kernel.map

OBJS=$(BUILD_DIR)/main.o $(BUILD_DIR)/init.o $(BUILD_DIR)/interrupt.o  \
		 $(BUILD_DIR)/timer.o $(BUILD_DIR)/kernel.o $(BUILD_DIR)/print.o \
		 $(BUILD_DIR)/debug.o $(BUILD_DIR)/string.o $(BUILD_DIR)/bitmap.o \
     $(BUILD_DIR)/memory.o $(BUILD_DIR)/thread.o $(BUILD_DIR)/list.o \
		 $(BUILD_DIR)/switch.o $(BUILD_DIR)/console.o $(BUILD_DIR)/sync.o \
		 $(BUILD_DIR)/keyboard.o $(BUILD_DIR)/io_queue.o $(BUILD_DIR)/tss.o \
		 $(BUILD_DIR)/process.o $(BUILD_DIR)/syscall_init.o $(BUILD_DIR)/syscall.o \
		 $(BUILD_DIR)/stdio.o $(BUILD_DIR)/stdio_kernel.o $(BUILD_DIR)/ide.o \
		 $(BUILD_DIR)/fs.o $(BUILD_DIR)/inode.o $(BUILD_DIR)/dir.o $(BUILD_DIR)/file.o \
		 $(BUILD_DIR)/fork.o $(BUILD_DIR)/shell.o

################## compile C program ##################
$(BUILD_DIR)/main.o: kernel/main.c lib/kernel/print.h lib/stdint.h kernel/init.h thread/thread.h \
	fs/fs.h fs/dir.h lib/user/syscall.h userprog/process.h userprog/syscall_init.h kernel/memory.h \
	device/io_queue.h  kernel/init.h kernel/debug.h device/keyboard.h lib/stdio.h kernel/interrupt.h \
	shell/shell.c lib/user/syscall.h lib/kernel/stdio_kernel.h device/console.h
	$(CC) $(CFLAGS) $< -o $@

$(BUILD_DIR)/init.o: kernel/init.c kernel/init.h kernel/interrupt.h kernel/global.h \
	lib/kernel/io.h lib/kernel/print.h lib/stdint.h thread/thread.h userprog/syscall_init.h\
  device/ide.h
	$(CC) $(CFLAGS) $< -o $@

$(BUILD_DIR)/interrupt.o: kernel/interrupt.c kernel/interrupt.h kernel/global.h \
	lib/stdint.h lib/kernel/io.h lib/kernel/print.h
	$(CC) $(CFLAGS) $< -o $@

$(BUILD_DIR)/timer.o: device/timer.c device/timer.h lib/stdint.h \
	lib/kernel/io.h lib/kernel/print.h thread/thread.h
	$(CC) $(CFLAGS) $< -o $@

$(BUILD_DIR)/debug.o: kernel/debug.c kernel/debug.h lib/stdint.h \
	kernel/interrupt.h lib/kernel/print.h
	$(CC) $(CFLAGS) $< -o $@

$(BUILD_DIR)/string.o: lib/string.c lib/string.h  lib/stdint.h \
kernel/debug.h  kernel/global.h
	$(CC) $(CFLAGS) $< -o $@

$(BUILD_DIR)/bitmap.o: lib/kernel/bitmap.c lib/kernel/bitmap.h lib/stdint.h \
	lib/string.h kernel/global.h kernel/debug.h kernel/interrupt.h
	$(CC) $(CFLAGS) $< -o $@

$(BUILD_DIR)/memory.o: kernel/memory.c kernel/memory.h lib/stdint.h \
	lib/kernel/bitmap.h lib/kernel/print.h kernel/global.h  kernel/debug.h \
	lib/string.h
	$(CC) $(CFLAGS) $< -o $@

$(BUILD_DIR)/thread.o: thread/thread.c thread/thread.h thread/switch.h lib/stdint.h \
	kernel/global.h kernel/memory.h lib/string.h
	$(CC) $(CFLAGS) $< -o $@

$(BUILD_DIR)/list.o: lib/kernel/list.c lib/kernel/list.h kernel/global.h\
kernel/interrupt.h
	$(CC) $(CFLAGS) $< -o $@

$(BUILD_DIR)/sync.o:  thread/sync.c thread/sync.h lib/stdint.h  thread/thread.h\
	kernel/debug.h  kernel/interrupt.h  lib/kernel/list.h  lib/kernel/stdio_kernel.h
	$(CC) $(CFLAGS) $< -o $@

$(BUILD_DIR)/console.o: device/console.c device/console.h lib/stdint.h \
	lib/kernel/print.h thread/sync.h
	$(CC) $(CFLAGS) $< -o $@

$(BUILD_DIR)/keyboard.o: device/keyboard.c  device/keyboard.h kernel/interrupt.h \
	lib/kernel/io.h lib/kernel/print.h
	$(CC) $(CFLAGS) $< -o $@

$(BUILD_DIR)/io_queue.o: device/io_queue.c device/io_queue.h kernel/debug.h \
	kernel/global.h  kernel/interrupt.h thread/sync.h thread/thread.h
	$(CC) $(CFLAGS) $< -o $@

$(BUILD_DIR)/tss.o: userprog/tss.c userprog/tss.h kernel/global.h thread/thread.h lib/string.h lib/stdint.h \
	lib/kernel/print.h
	$(CC) $(CFLAGS) $< -o $@

$(BUILD_DIR)/process.o: userprog/process.c userprog/process.h lib/stdint.h thread/thread.h \
	lib/string.h kernel/memory.h kernel/global.h kernel/debug.h userprog/tss.h lib/kernel/list.h device/console.h \
  kernel/interrupt.h userprog/userprog.h
	$(CC) $(CFLAGS) $< -o $@

$(BUILD_DIR)/syscall.o: lib/user/syscall.c lib/user/syscall.h
	$(CC) $(CFLAGS) $< -o $@

$(BUILD_DIR)/syscall_init.o: userprog/syscall_init.c userprog/syscall_init.h lib/stdint.h \
	lib/kernel/print.h lib/user/syscall.h thread/thread.h fs/fs.h
	$(CC) $(CFLAGS) $< -o $@

$(BUILD_DIR)/stdio.o: lib/stdio.c lib/stdio.h lib/stdint.h lib/string.h
	$(CC) $(CFLAGS) $< -o $@

$(BUILD_DIR)/stdio_kernel.o: lib/kernel/stdio_kernel.c lib/kernel/stdio_kernel.h lib/stdio.h \
	device/console.h kernel/global.h
	$(CC) $(CFLAGS) $< -o $@

$(BUILD_DIR)/ide.o: device/ide.c device/ide.h device/timer.h lib/stdint.h kernel/debug.h kernel/global.h \
	kernel/interrupt.h kernel/memory.h lib/kernel/io.h lib/kernel/list.h  lib/kernel/stdio_kernel.h \
  thread/sync.h lib/string.h lib/stdio.h
	$(CC) $(CFLAGS) $< -o $@

$(BUILD_DIR)/inode.o: fs/inode.c fs/inode.h fs/super_block.h kernel/debug.h kernel/interrupt.h kernel/memory.h device/ide.h\
	lib/stdint.h lib/string.h thread/thread.h lib/kernel/list.h
	$(CC) $(CFLAGS) $< -o $@

$(BUILD_DIR)/dir.o: fs/dir.c fs/dir.h fs/super_block.h fs/inode.h fs/file.h lib/kernel/bitmap.h kernel/debug.h kernel/global.h\
	lib/stdint.h lib/string.h kernel/memory.h lib/kernel/stdio_kernel.h
	$(CC) $(CFLAGS) $< -o $@

$(BUILD_DIR)/file.o: fs/file.c fs/file.h fs/super_block.h fs/fs.h fs/dir.h fs/inode.h device/ide.h lib/kernel/bitmap.h lib/stdint.h \
	lib/string.h kernel/global.h kernel/memory.h thread/thread.h lib/kernel/stdio_kernel.h lib/kernel/list.h
	$(CC) $(CFLAGS) $< -o $@

$(BUILD_DIR)/fs.o: fs/fs.c fs/fs.h fs/dir.h fs/inode.h fs/super_block.h device/ide.h device/keyboard.h lib/stdint.h lib/string.h \
	lib/kernel/stdio_kernel.h kernel/memory.h kernel/global.h kernel/debug.h
	$(CC) $(CFLAGS) $< -o $@

$(BUILD_DIR)/fork.o: userprog/fork.c userprog/fork.h userprog/process.h thread/thread.h kernel/debug.h fs/dir.h \
	fs/file.h fs/fs.h fs/inode.h kernel/interrupt.h lib/kernel/list.h lib/stdint.h kernel/memory.h kernel/global.h
	$(CC) $(CFLAGS) $< -o $@

$(BUILD_DIR)/shell.o: shell/shell.c shell/shell.h fs/file.h lib/stdint.h lib/stdio.h lib/user/syscall.h
	$(CC) $(CFLAGS) $< -o $@

################## assemble assembly ##################
$(BUILD_DIR)/kernel.o: kernel/kernel.S
	$(AS) $(ASFLAGS) $< -o $@
$(BUILD_DIR)/print.o: lib/kernel/print.S
	$(AS) $(ASFLAGS) $< -o $@
$(BUILD_DIR)/switch.o: thread/switch.S
	$(AS) $(ASFLAGS) $< -o $@


################## link all Objects ##################
$(BUILD_DIR)/kernel.bin:$(OBJS)
	$(LD) $(LDFLAGS) $^ -o $@

################## phony target ##################
.PHONY: mk_dir hd clean all

mk_dir:
	if [ ! -d $(BUILD_DIR) ]; then mkdir $(BUILD_DIR);fi

hd:
	dd if=$(BUILD_DIR)/kernel.bin \
	of=/home/elite-zx/bochs/hd60M.img\
	bs=512 count=200 seek=9 conv=notrunc

clean:
	cd $(BUILD_DIR) && rm -f ./*

build: $(BUILD_DIR)/kernel.bin

all: mk_dir build hd
