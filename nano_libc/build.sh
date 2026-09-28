../nanohdd ../hda.img mkdir /bin
make clean
make 
../nanohdd ../hda.img push app.bin /bin/app
make clean
make APP=test_malloc
make APP=test_sleep
make APP=time
make APP=date
make APP=mandel
make APP=shell
make APP=shutdown
../nanohdd ../hda.img push shutdown.bin /bin/quit
../nanohdd ../hda.img push shutdown.bin /bin/q
make APP=ls
make APP=nanoasm
make APP=format
make APP=rm
make APP=cat
make APP=mkdir

make clean

../nanohdd ../hda.img push program.s program.s

qemu-system-x86_64 -cdrom ../kernel.iso -drive file=../hda.img,format=raw,index=0,media=disk -display curses