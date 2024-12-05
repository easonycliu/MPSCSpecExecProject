#!/bin/bash

project_dir=$PWD/..

sudo qemu-system-x86_64 -s -S -cpu host,-smap,-smep -smp 32 -accel kvm -m 128G -nographic \
	-drive file=$project_dir/vmimg/jammy-server-cloudimg-amd64-disk-kvm.img,if=none,id=disk0,format=qcow2 \
	-kernel $project_dir/linux/arch/x86_64/boot/bzImage -append "nokaslr root=PARTUUID=64c23ff6-ab45-e343-a2cf-fc57421f2643 rw console=tty1 console=ttyS0" \
	-device virtio-blk-pci,drive=disk0 \
	-nic user,model=virtio-net-pci,hostfwd=tcp::60022-:22
