rm -f hda_v3.img hdb_v3.img
qemu-img create -f raw hda_v3.img 50M
qemu-img create -f raw hdb_v3.img 50M

tools/nan3hdd hdb_v3.img format
tools/nan3hdd hda_v3.img format
tools/nan3hdd hda_v3.img mkdir /bin
tools/nan3hdd hda_v3.img mkdir /sbin
tools/nan3hdd hda_v3.img mkdir /cfg
tools/nan3hdd hda_v3.img mkdir /tests
tools/nan3hdd hda_v3.img mkdir /asm
tools/nan3hdd hda_v3.img mkdir /data
tools/nan3hdd hda_v3.img mkdir /data/mp3

tools/nan3hdd hda_v3.img push env.cfg /cfg/env.cfg

tools/nan3hdd hda_v3.img push data/mp3/laser.wav /data/mp3/1.wav

tools/nan3hdd hda_v3.img push nanoasm/program.s /asm/program.s
tools/nan3hdd hda_v3.img push nanoasm/loop.s /asm/loop.s
tools/nan3hdd hda_v3.img push nanoasm/call.s /asm/call.s
tools/nan3hdd hda_v3.img push nanoasm/math.s /asm/math.s
tools/nan3hdd hda_v3.img push nanoasm/mem.s /asm/mem.s
tools/nan3hdd hda_v3.img push nanoasm/kin.s /asm/kin.s
tools/nan3hdd hda_v3.img push nanoasm/var.s /asm/var.s
tools/nan3hdd hda_v3.img push nanoasm/float.s /asm/float.s
tools/nan3hdd hda_v3.img push nanoasm/julia.s /asm/julia.s






