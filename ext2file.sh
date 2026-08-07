#!/usr/bin/bash
./mountext2.sh
echo "hi!" | sudo tee ext2root/hi.txt
sudo dd if=/dev/urandom of=ext2root/big.bin bs=64K count=1
./unmountext2.sh 