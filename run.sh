nasm -I include/ -o mbr.bin mbr.S
nasm -I include/ -o loader.bin loader.S
dd if=mbr.bin of=/home/elite-zx/bochs/hd60M.img bs=512 count=1 conv=notrunc
dd if=loader.bin of=/home/elite-zx/bochs/hd60M.img bs=512 count=4 seek=2 conv=notrunc

