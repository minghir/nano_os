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

echo "[Nano OS] Running Nano OS in QEMU..."
make run

