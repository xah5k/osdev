#!/usr/bin/bash
set -euo pipefail

IMG="osdev.img"
ESP_SIZE_MB=64
EXT2_SIZE_MB=2048
IMG_SIZE_MB=$((1 + 1 + ESP_SIZE_MB + EXT2_SIZE_MB + 1))
LIMINE_BOOTF=$(limine --print-datadir)

# create blank image
dd if=/dev/zero of="$IMG" bs=1M count=$IMG_SIZE_MB

parted -s "$IMG" mklabel gpt

# 1MiB BIOS boot partition (no filesystem, just space for stage2 embedding)
parted -s "$IMG" mkpart biosboot 1MiB 2MiB
parted -s "$IMG" set 1 bios_grub on

# ESP
parted -s "$IMG" mkpart ESP fat32 2MiB $((2 + ESP_SIZE_MB))MiB
parted -s "$IMG" set 2 esp on

# ext2
parted -s "$IMG" mkpart ext2part ext2 $((2 + ESP_SIZE_MB))MiB 100%

read -r ESP_START ESP_SECTORS < <(fdisk -l "$IMG" | awk '/^osdev.img2/ {print $2, $4}')
read -r EXT2_START EXT2_SECTORS < <(fdisk -l "$IMG" | awk '/^osdev.img3/ {print $2, $4}')

ESP_OFFSET=$((ESP_START * 512))
EXT2_OFFSET=$((EXT2_START * 512))
EXT2_SIZE_K=$((EXT2_SECTORS * 512 / 1024))

mformat -i "$IMG"@@$ESP_OFFSET -F ::
mmd -i "$IMG"@@$ESP_OFFSET ::/EFI ::/EFI/BOOT
mcopy -i "$IMG"@@$ESP_OFFSET $LIMINE_BOOTF/BOOTX64.EFI ::/EFI/BOOT/
mcopy -i "$IMG"@@$ESP_OFFSET $LIMINE_BOOTF/limine-bios.sys ::/
mcopy -i "$IMG"@@$ESP_OFFSET sysroot/boot/limine.conf ::/limine.conf

tar -cf initrd.tar -C "sysroot" --transform 's,^\./,,' .
mcopy -i "$IMG"@@$ESP_OFFSET initrd.tar ::/initrd
rm -f initrd.tar

mcopy -i "$IMG"@@$ESP_OFFSET sysroot/boot/osdev.elf ::/osdev.elf

limine bios-install "$IMG"

mkfs.ext2 -F -E offset=$EXT2_OFFSET "$IMG" "${EXT2_SIZE_K}K"
qemu-img convert -f raw -O vmdk $IMG osdev.vmdk
echo "done"