#!/bin/bash
set -e

echo "[+] Creare imagini de disc hda_v3.img și hdb_v3.img..."
rm -f hda_v3.img hdb_v3.img
qemu-img create -f raw hda_v3.img 50M
qemu-img create -f raw hdb_v3.img 50M

echo "[+] Formatare și creare directoare pe hda_v3.img..."
tools/nan3hdd hdb_v3.img format
tools/nan3hdd hda_v3.img format

tools/nan3hdd hda_v3.img mkdir /bin
tools/nan3hdd hda_v3.img mkdir /sbin
tools/nan3hdd hda_v3.img mkdir /cfg
tools/nan3hdd hda_v3.img mkdir /tests
tools/nan3hdd hda_v3.img mkdir /asm
tools/nan3hdd hda_v3.img mkdir /data
tools/nan3hdd hda_v3.img mkdir /data/mp3

echo "[+] Copiere fișiere de sistem și config..."
tools/nan3hdd hda_v3.img push env.cfg /cfg/env.cfg
tools/nan3hdd hda_v3.img push data/mp3/laser.wav /data/mp3/1.wav

echo "[+] Copiere fișiere assembly pentru nanoasm..."
tools/nan3hdd hda_v3.img push nanoasm/program.s /asm/program.s
tools/nan3hdd hda_v3.img push nanoasm/loop.s /asm/loop.s
tools/nan3hdd hda_v3.img push nanoasm/call.s /asm/call.s
tools/nan3hdd hda_v3.img push nanoasm/math.s /asm/math.s
tools/nan3hdd hda_v3.img push nanoasm/mem.s /asm/mem.s
tools/nan3hdd hda_v3.img push nanoasm/kin.s /asm/kin.s
tools/nan3hdd hda_v3.img push nanoasm/var.s /asm/var.s
tools/nan3hdd hda_v3.img push nanoasm/float.s /asm/float.s
tools/nan3hdd hda_v3.img push nanoasm/julia.s /asm/julia.s

echo "[+] Copiere aplicații din src/ (binarele compilate)..."
# Aici poți urca automat tot ce s-a compilat în src/
for bin in src/*.bin; do
    if [ -f "$bin" ]; then
        name=$(basename "$bin" .bin)
        # Dacă e shell sau altceva ce merge în /sbin sau /bin, le direcționăm corect:
        if [ "$name" = "shell" ] || [ "$name" = "init" ] || [ "$name" = "su" ]; then
            tools/nan3hdd hda_v3.img push "$bin" "/sbin/$name"
            echo "   [SBIN] $bin -> /sbin/$name"
        else
            tools/nan3hdd hda_v3.img push "$bin" "/bin/$name"
            echo "   [BIN]  $bin -> /bin/$name"
        fi
    fi
done

echo "[+] Copiere teste din tests/..."
for bin in tests/*.bin; do
    if [ -f "$bin" ]; then
        name=$(basename "$bin" .bin)
        tools/nan3hdd hda_v3.img push "$bin" "/tests/$name"
        echo "   [TEST] $bin -> /tests/$name"
    fi
done

echo "=== GATA! Imaginea hda_v3.img este complet echipată și pregătită! ==="





