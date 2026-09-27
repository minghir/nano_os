../nanohdd ../hda.img mkdir /bin
make clean
make 
../nanohdd ../hda.img push app.bin /bin/app
make clean
make APP=test_malloc
../nanohdd ../hda.img push test_malloc.bin /bin/tmalloc
make clean

make clean
make APP=test_sleep
../nanohdd ../hda.img push test_sleep.bin /bin/tsleep
make clean

make clean
make APP=time
../nanohdd ../hda.img push time.bin /bin/time
make clean

make clean
make APP=date
../nanohdd ../hda.img push date.bin /bin/date
make clean

make clean
make APP=mandel
../nanohdd ../hda.img push mandel.bin /bin/mandel
make clean

make clean
make APP=shell
../nanohdd ../hda.img push shell.bin /bin/sh
make clean


make APP=shutdown
../nanohdd ../hda.img push shutdown.bin /bin/shutdown
make clean



qemu-system-x86_64 -cdrom ../kernel.iso -drive file=../hda.img,format=raw,index=0,media=disk -display curses