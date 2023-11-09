nasm -f elf -o lib/kernel/print.o lib/kernel/print.S &&
gcc -I lib/kernel/ -m32 -c -o kernel/main.o kernel/main.c &&
ld -m elf_i386 -Ttext 0xc0001500 -e main -o kernel.bin \
kernel/main.o lib/kernel/print.o &&
dd if=kernel.bin of=/home/elite-zx/bochs/hd60M.img bs=512 count=200 seek=9 conv=notrunc

