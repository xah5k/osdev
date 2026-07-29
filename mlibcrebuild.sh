#!/usr/bin/bash
pushd ~/ostoolchain/mlibc
ninja -C build
DESTDIR=$HOME/ostoolchain/sysroot ninja -C build install
popd
