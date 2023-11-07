gcc -m32 -c -o main.o main.c &&
ld -m elf_i386 main.o -Ttext 0xc0001500 -e main -o kernel.bin &&
dd if=kernel.bin of=/home/elite-zx/bochs/hd60M.img bs=512 count=200 seek=9 conv=notrunc

