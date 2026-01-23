#!/usr/bin/env bash

qemu-system-x86_64 \
  -machine pc \
  -drive file=./bin/os.img,format=raw,if=ide \
  -m 512M \
  -cpu Skylake-Server,tsc-frequency=2800000000 \
  -bios /usr/share/ovmf/OVMF.fd
