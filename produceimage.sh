#!/usr/bin/bash
set -euo pipefail

IMG="osdev.img"
CONFIG="bootimg.json"
tools/mkbootimg $CONFIG $IMG
read -r START SECTORS < <(fdisk -l "$IMG" | awk '/^osdev.img2/ {print $2, $4}')
OFFSET=$((START * 512))
SIZE_K=$((SECTORS * 512 / 1024))
echo "ext2 partition: start=$START sectors=$SECTORS offset=$OFFSET size=${SIZE_K}K"
# cuz bootboot doesnt support formatting as ext2 apparently?
mkfs.ext2 -F -E offset=$OFFSET "$IMG" "${SIZE_K}K"