#!/bin/bash
set -e

IMAGE="hda_ext2.img"
MOUNT_DIR="./mnt_hda"
#ROOTFS_DIR="./tmp/rootfs"
ROOTFS_DIR="./rootfs"
SIZE="50M"

echo "[+] Compilare init..."
make -C src init.bin
install -m 755 src/init.bin "$ROOTFS_DIR/bin/init"

echo "[+] 1. Creare imagine de disc $IMAGE ($SIZE)..."
rm -f "$IMAGE"
qemu-img create -f raw "$IMAGE" "$SIZE"

echo "[+] 2. Formatare imagine ca ext2..."
mkfs.ext2 -F "$IMAGE"

echo "[+] 3. Creare punct de montare și montare imagine..."
mkdir -p "$MOUNT_DIR"
sudo mount -o loop "$IMAGE" "$MOUNT_DIR"

echo "[+] 4. Copiere integrală a structurii din $ROOTFS_DIR în imaginea ext2..."
# Folosim -a (archive) ca să păstrăm permisiunile, structura și atributele fișierelor
sudo cp -a "$ROOTFS_DIR/." "$MOUNT_DIR/"

echo "[+] 5. Setare permisiuni corecte (root:root)..."
sudo chown -R root:root "$MOUNT_DIR"

echo "[+] 6. Sincronizare și demontare..."
sync
sudo umount "$MOUNT_DIR"
rmdir "$MOUNT_DIR"

echo "=== SUCCES! Imaginea $IMAGE a fost populată complet din $ROOTFS_DIR! ==="