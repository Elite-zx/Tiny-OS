current_dir=$(pwd)
 
nasm -I ${current_dir}/include/ -o ${current_dir}/mbr.bin ${current_dir}/mbr.S
nasm -I ${current_dir}/include/ -o ${current_dir}/loader.bin ${current_dir}/loader.S
dd if=${current_dir}/mbr.bin of=/home/elite-zx/bochs/hd60M.img bs=512 count=1 conv=notrunc
dd if=${current_dir}/loader.bin of=/home/elite-zx/bochs/hd60M.img bs=512 count=4 seek=2 conv=notrunc

