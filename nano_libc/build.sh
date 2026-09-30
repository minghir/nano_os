../nanohdd ../hda.img format
../nanohdd ../hda.img mkdir /bin
../nanohdd ../hda.img mkdir /sbin
../nanohdd ../hda.img mkdir /cfg
../nanohdd ../hda.img mkdir /tests
../nanohdd ../hda.img push env.cfg /cfg/env.cfg

make clean
make

# Acum apelezi doar make APP=nume, iar Makefile-ul se ocupă și de binar și de push!
make APP=shell
make APP=time
make APP=date
make APP=shutdown
# Dacă vrei cazul special pentru shutdown și în /sbin/q:
../nanohdd ../hda.img push shutdown.bin /sbin/q

make APP=ls
make APP=format
make APP=rm
make APP=cat
make APP=mkdir
make APP=touch
make APP=pwd
make APP=ps
make APP=kill

make APP=mandel
make APP=nanoasm
make APP=nanovi

make APP=chr
make APP=argt
make APP=test_malloc
make APP=test_sleep


make clean

../nanohdd ../hda.img push program.s program.s

qemu-system-x86_64 -cdrom ../kernel.iso -drive file=../hda.img,format=raw,index=0,media=disk -display curses