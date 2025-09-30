#!/usr/bin/env bash
set -euo pipefail

error() {
	echo "[ERROR] $1" >&2
	exit 1
}

info() {
	echo "[INFO] $1"
}

# Workspace Setup
CURRENT_DIR=$(pwd)
WORKSPACE_ROOT=$CURRENT_DIR
KERNEL_DIR=../ # change if necessary
while [ ! -f "$WORKSPACE_ROOT/edksetup.sh" ] && [ "$WORKSPACE_ROOT" != "/" ]; do
	WORKSPACE_ROOT=$(dirname "$WORKSPACE_ROOT")
done
if [ ! -f "$WORKSPACE_ROOT/edksetup.sh" ]; then
	error "Could not find edksetup.sh in any parent directory. Are you in the edk2 tree?"
fi

cd "$WORKSPACE_ROOT"
export EDK_TOOLS_PATH="$WORKSPACE_ROOT/BaseTools"
info "Using WORKSPACE_ROOT=$WORKSPACE_ROOT"
info "EDK_TOOLS_PATH=$EDK_TOOLS_PATH"

# Source build environment
set +u
if [ -f "$WORKSPACE_ROOT/Conf/BuildEnv.sh" ]; then
	# shellcheck disable=SC1091
	. "$WORKSPACE_ROOT/Conf/BuildEnv.sh"
fi
export PYTHON_COMMAND=${PYTHON_COMMAND:-python3}
export WORKSPACE=${WORKSPACE:-$WORKSPACE_ROOT}
# shellcheck disable=SC1091
. "$WORKSPACE_ROOT/edksetup.sh" BaseTools
set -u

# Toolchain Detection (for cross-compiling)
host_arch=$(uname -m)
if [[ "$host_arch" != "x86_64" ]]; then
	cross_gcc=""
	if command -v x86_64-linux-gnu-gcc >/dev/null 2>&1; then
		cross_gcc=$(command -v x86_64-linux-gnu-gcc)
	else
		oldshopt=$(shopt -p nullglob || true)
		shopt -s nullglob
		for dir in ${PATH//:/ }; do
			for f in "$dir"/x86_64-*-gcc; do
				if [ -x "$f" ]; then
					cross_gcc="$f"
					break 2
				fi
			done
		done
		eval "$oldshopt"
	fi
	if [ -n "$cross_gcc" ]; then
		cross_prefix=${cross_gcc%gcc}
		export GCC_BIN="$cross_prefix"
		export GCC5_BIN="$cross_prefix"
		info "Detected x86_64 cross-compiler: $cross_gcc"
		info "Exported GCC_BIN and GCC5_BIN to '$cross_prefix' so edk2 will use the cross toolchain."
	else
		cat <<EOF
Error: host is $host_arch (not x86_64) and the edk2 build is targeting X64.
The native gcc on this host will not accept x86_64-specific flags like -m64.
Install a cross-compiler that targets x86_64 (for example a toolchain providing
an executable named like "x86_64-linux-gnu-gcc"), ensure it is in PATH, and
re-run this script. Alternatively, run the build on an x86_64 machine.

Example (Debian/Ubuntu):
	sudo apt-get install gcc-multilib g++-multilib gcc-x86-64-linux-gnu

Or configure edk2 to use a cross toolchain by exporting GCC_BIN to the
cross-compiler prefix (e.g. export GCC_BIN=/usr/bin/x86_64-linux-gnu-)
EOF
		exit 1
	fi
fi

# Build Step
# Use a conservative thread count
NPROC=1
if command -v nproc >/dev/null 2>&1; then
	NPROC=$(nproc)
	if [ "$NPROC" -gt 4 ]; then
		NPROC=4
	fi
fi
info "Running edk2 build (threads=$NPROC) from $WORKSPACE_ROOT"
build

# Image Creation (requires root)
cd "$CURRENT_DIR"
mkdir -p ./bin /mnt/d

dd if=/dev/zero bs=1048576 count=700 of=./bin/os.img
LOOPDEV=$(sudo losetup --find --show --partscan ./bin/os.img)
info "Loop Device $LOOPDEV"

# Partition and format the image
sudo parted "$LOOPDEV" --script mklabel gpt
sudo parted "$LOOPDEV" --script mkpart ESP fat16 1Mib 50%
sudo parted "$LOOPDEV" --script mkpart ESP fat16 50% 100%
sudo parted "$LOOPDEV" --script set 1 esp on
sudo partprobe "$LOOPDEV"
sleep 2
lsblk "$LOOPDEV"

sudo mkfs.vfat -n ABC "${LOOPDEV}p1"
sudo mkfs.vfat -n MYOS "${LOOPDEV}p2"
sudo mount -t vfat "${LOOPDEV}p2" /mnt/d

# Build MyOS Kernel
info "Building 64-bit my-os kernel..."
cd "$KERNEL_DIR"
make clean
./build.sh
cd "$CURRENT_DIR"

# Copy UEFI Bootloader
cp ../../../Build/MdeModule/DEBUG_GCC/X64/MyOS.efi ./bin/MyOS.efi
sudo mkdir -p /mnt/d/EFI/BOOT
sudo cp ./bin/MyOS.efi /mnt/d/EFI/Boot/BOOTX64.efi
sudo umount /mnt/d
sudo losetup -d "$LOOPDEV"

info "Build completed."
