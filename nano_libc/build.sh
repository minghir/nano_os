../nanohdd ../hda_v2.img format
../nanohdd ../hda_v2.img mkdir /bin
../nanohdd ../hda_v2.img mkdir /sbin
../nanohdd ../hda_v2.img mkdir /cfg
../nanohdd ../hda_v2.img mkdir /tests
../nanohdd ../hda_v2.img mkdir /asm
../nanohdd ../hda_v2.img mkdir /data
../nanohdd ../hda_v2.img mkdir /data/mp3

../nanohdd ../hda_v2.img push env.cfg /cfg/env.cfg

../nanohdd ../hda_v2.img push ../data/mp3/laser.wav /data/mp3/1.wav

../nanohdd ../hda_v2.img push program.s /asm/program.s
../nanohdd ../hda_v2.img push loop.s /asm/loop.s
../nanohdd ../hda_v2.img push call.s /asm/call.s
../nanohdd ../hda_v2.img push math.s /asm/math.s
../nanohdd ../hda_v2.img push mem.s /asm/mem.s
../nanohdd ../hda_v2.img push kin.s /asm/kin.s
../nanohdd ../hda_v2.img push var.s /asm/var.s
../nanohdd ../hda_v2.img push float.s /asm/float.s
../nanohdd ../hda_v2.img push julia.s /asm/julia.s

make clean
make

# Acum apelezi doar make APP=nume, iar Makefile-ul se ocupă și de binar și de push!
make APP=shell
make APP=init
make APP=time
make APP=date
make APP=shutdown
# Dacă vrei cazul special pentru shutdown și în /sbin/q:
../nanohdd ../hda_v2.img push shutdown.bin /sbin/q

make APP=ls
make APP=format
make APP=rm
make APP=cat
make APP=mkdir
make APP=touch
make APP=pwd
make APP=ps
make APP=kill
make APP=free
make APP=dmesg

make APP=mandel
make APP=nanoasm
make APP=nanovi
make APP=watch
make APP=mp3_play
make APP=wavplay

make APP=chr
make APP=argt
make APP=test_malloc
make APP=test_sleep
make APP=tsr_sleep2
make APP=crash
make APP=mem_test


make clean

#qemu-system-x86_64 -cdrom kernel.iso -drive file=hda_v2.img,format=raw,index=0,media=disk file=hdb_v3.img,format=raw,index=0,media=disk  -audiodev sdl,id=snd0 -machine pcspk-audiodev=snd0 -device AC97,audiodev=snd0
