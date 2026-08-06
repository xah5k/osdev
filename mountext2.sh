#!/usr/bin/bash
IMG=osdev.img
read -r START SECTORS < <(fdisk -l "$IMG" | awk '/^osdev.img2/ {print $2, $4}')
OFFSET=$((START * 512))
SIZE_K=$((SECTORS * 512 / 1024))

mkdir -p ext2root
sudo mount -o loop,offset=$OFFSET osdev.img ./ext2root