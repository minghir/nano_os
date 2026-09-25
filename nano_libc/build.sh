make clean
make 
../nanohdd ../hda.img push app.bin app.bin
make clean
make APP=test_malloc
../nanohdd ../hda.img push test_malloc.bin tmalloc.bin
make clean

make clean
make APP=test_sleep
../nanohdd ../hda.img push test_sleep.bin tsleep.bin
make clean

#qemu-system-x86_64 -cdrom ../kernel.iso -drive file=../hda.img,format=raw,index=0,media=disk file:qemu_output.log
qemu-system-x86_64 -cdrom ../kernel.iso -drive file=../hda.img,format=raw,index=0,media=disk -serial file:qemu_output.log