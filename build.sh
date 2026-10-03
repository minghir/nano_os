#!/bin/bash

echo "[Nano OS] Updating system..."
sudo apt update

echo "[Nano OS] Installing build tools..."
sudo apt install -y build-essential

echo "[Nano OS] Installing NASM..."
sudo apt install -y nasm

echo "[Nano OS] Installing Clang + LLD..."
sudo apt install -y clang lld

echo "[Nano OS] Installing GRUB tools..."
sudo apt install -y grub-pc-bin

echo "[Nano OS] Installing ISO tools..."
sudo apt install -y xorriso mtools

echo "[Nano OS] Installing QEMU..."
sudo apt install -y qemu-system-x86

echo "[Nano OS] All dependencies installed!"

echo "[Nano OS] Cleaning old build..."
make clean

echo "[Nano OS] Building kernel..."
make

echo "[Nano OS] Generating ISO..."
make kernel.iso

echo "[Nano OS] Generating IMGs..."
make img_files

tools/nan2hdd hda_v2.img format
tools/nan2hdd hda_v2.img mkdir /bin
tools/nan2hdd hda_v2.img mkdir /sbin
tools/nan2hdd hda_v2.img mkdir /cfg
tools/nan2hdd hda_v2.img mkdir /tests
tools/nan2hdd hda_v2.img mkdir /asm
tools/nan2hdd hda_v2.img mkdir /data
tools/nan2hdd hda_v2.img mkdir /data/mp3

tools/nan2hdd hda_v2.img push env.cfg /cfg/env.cfg

tools/nan2hdd hda_v2.img push data/mp3/laser.wav /data/mp3/1.wav

tools/nan2hdd hda_v2.img push nanoasm/program.s /asm/program.s
tools/nan2hdd hda_v2.img push nanoasm/loop.s /asm/loop.s
tools/nan2hdd hda_v2.img push nanoasm/call.s /asm/call.s
tools/nan2hdd hda_v2.img push nanoasm/math.s /asm/math.s
tools/nan2hdd hda_v2.img push nanoasm/mem.s /asm/mem.s
tools/nan2hdd hda_v2.img push nanoasm/kin.s /asm/kin.s
tools/nan2hdd hda_v2.img push nanoasm/var.s /asm/var.s
tools/nan2hdd hda_v2.img push nanoasm/float.s /asm/float.s
tools/nan2hdd hda_v2.img push nanoasm/julia.s /asm/julia.s



echo "[Nano OS] Running Nano OS in QEMU..."
make run

