#!/usr/bin/bash
./mountext2.sh
echo "hi!" | sudo tee ext2root/hi.txt
./unmountext2.sh 