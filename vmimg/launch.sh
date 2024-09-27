#!/bin/bash

project_dir=$PWD/..

sudo qemu-system-x86_64 -cpu host,-smap,-smep -smp 32 -accel kvm -m 128G -nographic \
	-drive file=$project_dir/vmimg/jammy-server-cloudimg-amd64-disk-kvm.img,if=none,id=disk0,format=qcow2 \
	-device virtio-blk-pci,drive=disk0 \
	-nic user,model=virtio-net-pci,hostfwd=tcp::60022-:22
