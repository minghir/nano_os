./nanohdd ./hda_v2.img format
./nanohdd ./hda_v2.img mkdir /bin
./nanohdd ./hda_v2.img mkdir /sbin
./nanohdd ./hda_v2.img mkdir /cfg
./nanohdd ./hda_v2.img mkdir /tests
./nanohdd ./hda_v2.img mkdir /asm
./nanohdd ./hda_v2.img mkdir /data
./nanohdd ./hda_v2.img mkdir /data/mp3

./nanohdd ./hda_v2.img push env.cfg /cfg/env.cfg

./nanohdd ./hda_v2.img push ./data/mp3/laser.wav /data/mp3/1.wav

./nanohdd ./hda_v2.img push ./nanoasm/program.s /asm/program.s
./nanohdd ./hda_v2.img push ./nanoasm/loop.s /asm/loop.s
./nanohdd ./hda_v2.img push ./nanoasm/call.s /asm/call.s
./nanohdd ./hda_v2.img push ./nanoasm/math.s /asm/math.s
./nanohdd ./hda_v2.img push ./nanoasm/mem.s /asm/mem.s
./nanohdd ./hda_v2.img push ./nanoasm/kin.s /asm/kin.s
./nanohdd ./hda_v2.img push ./nanoasm/var.s /asm/var.s
./nanohdd ./hda_v2.img push ./nanoasm/float.s /asm/float.s
./nanohdd ./hda_v2.img push ./nanoasm/julia.s /asm/julia.s