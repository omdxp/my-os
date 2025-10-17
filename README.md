# MyOS - Custom 64-bit Operating System

A custom x86_64 operating system with UEFI bootloader, GPT partition support, and FAT16 filesystem.

## Features

- **UEFI Boot**: Modern UEFI bootloader instead of legacy BIOS
- **64-bit Long Mode**: Full x86_64 support with 4-level paging
- **GPT Partitions**: GUID Partition Table support
- **FAT16 Filesystem**: File I/O with FAT16 support
- **Graphics**: Framebuffer graphics with BMP image support
- **Process Management**: Task switching and ELF executable support
- **Kernel Heap**: Dynamic memory allocation
- **Interrupts**: IDT, IRQs, and system calls (int 0x80)

## Prerequisites

### 1. Cross-Compiler Toolchain

You need a x86_64-elf cross-compiler. Follow the [OSDev Wiki GCC Cross-Compiler guide](https://wiki.osdev.org/GCC_Cross-Compiler) to build one.

**Quick setup:**

```bash
export PREFIX="$HOME/opt/cross"
export TARGET=x86_64-elf
export PATH="$PREFIX/bin:$PATH"
```

Add these to your `~/.bashrc` or `~/.zshrc` to make them permanent:

```bash
echo 'export PREFIX="$HOME/opt/cross"' >> ~/.bashrc
echo 'export TARGET=x86_64-elf' >> ~/.bashrc
echo 'export PATH="$PREFIX/bin:$PATH"' >> ~/.bashrc
source ~/.bashrc
```

Verify installation:
```bash
x86_64-elf-gcc --version
x86_64-elf-ld --version
```

### 2. EDK2 (UEFI Development Kit)

Clone the EDK2 repository:

```bash
cd ~
git clone https://github.com/tianocore/edk2.git
cd edk2
git submodule update --init
```

Build EDK2 BaseTools:

```bash
make -C BaseTools
source edksetup.sh
```

Configure EDK2 for 64-bit compilation by editing `Conf/target.txt`:

```
ACTIVE_PLATFORM       = MdeModulePkg/MdeModulePkg.dsc
TARGET_ARCH           = X64
TOOL_CHAIN_TAG        = GCC5
```

### 3. Required Tools

Install required packages (Ubuntu/Debian):

```bash
sudo apt-get install build-essential nasm qemu-system-x86 gdb parted dosfstools
```

## Project Structure

```
my-os/
├── kernel/              # Kernel source code
│   ├── src/            # C and assembly source files
│   ├── programs/       # User programs (shell, echo, etc.)
│   ├── data/           # Resources (fonts, images)
│   ├── Makefile        # Kernel build configuration
│   └── build.sh        # Kernel build script (called by main build)
└── edk2/               # UEFI bootloader (place inside EDK2 repo)
    ├── MyOS.c          # UEFI bootloader implementation
    ├── MyOS.inf        # EDK2 module definition
    ├── build.sh        # Main build script (builds everything)
    └── run.sh          # QEMU launch script
```

## Installation

### Step 1: Set up the project in EDK2

Copy or symlink the `edk2` directory from this project into your EDK2 repository:

```bash
# From the my-os directory
cd /path/to/my-os
cp -r edk2 ~/edk2/MdeModulePkg/Application/MyOS

# OR create a symlink
ln -s /path/to/my-os/edk2 ~/edk2/MdeModulePkg/Application/MyOS
```

### Step 2: Build the UEFI application

Add MyOS to the EDK2 build by editing `~/edk2/MdeModulePkg/MdeModulePkg.dsc`:

Find the `[Components]` section and add:

```
[Components]
  # ... existing components ...
  MdeModulePkg/Application/MyOS/MyOS.inf
```

### Step 3: Build the OS

Navigate to the MyOS UEFI application directory and run the main build script:

```bash
cd ~/edk2/MdeModulePkg/Application/MyOS
./build.sh
```

This script will:
1. Build the EDK2 UEFI application (MyOS.efi)
2. Build the kernel and user programs
3. Create a 700MB disk image (`bin/os.img`)
4. Set up GPT partitions:
   - Partition 1: EFI System Partition (ESP)
   - Partition 2: MyOS filesystem (FAT16, label: MYOSFS)
5. Copy the kernel, programs, and resources to the filesystem
6. Install the UEFI bootloader

## Running

### Quick Start with QEMU

The easiest way to run after building:

```bash
cd ~/edk2/MdeModulePkg/Application/MyOS
./run.sh
```

This will automatically launch QEMU with the correct settings.

### Manual QEMU Launch

Alternatively, run QEMU manually:

```bash
cd ~/edk2/MdeModulePkg/Application/MyOS
qemu-system-x86_64 -drive file=bin/os.img,format=raw -m 512M -cpu qemu64 -bios /usr/share/ovmf/OVMF.fd
```

**Note:** You need OVMF (UEFI firmware for QEMU). Install it:

```bash
sudo apt-get install ovmf
```

### With QEMU + GDB (for debugging)

Terminal 1 (run QEMU with GDB server):
```bash
qemu-system-x86_64 -drive file=bin/os.img,format=raw -m 512M -cpu qemu64 \
  -bios /usr/share/ovmf/OVMF.fd -s -S
```

Terminal 2 (connect GDB):
```bash
cd ~/edk2/MdeModulePkg/Application/MyOS/../../my-os/kernel
gdb
(gdb) target remote :1234
(gdb) add-symbol-file build/kernelfull.o 0x100000
(gdb) break kernel_main
(gdb) continue
```

### On Real Hardware

⚠️ **Warning:** Test in a VM first! Writing to the wrong disk can destroy data.

1. Write the image to a USB drive:
   ```bash
   sudo dd if=bin/os.img of=/dev/sdX bs=4M status=progress
   sync
   ```
   Replace `/dev/sdX` with your USB drive (check with `lsblk`)

2. Boot from the USB drive (enable UEFI boot in BIOS/UEFI settings)

## File Structure on Disk

Files on Partition 2 (MYOSFS):

- `kernel.bin` - The kernel binary
- `shell.elf` - Shell program (ELF format)
- `echo.elf` - Echo test program
- `blank.elf` - Blank test program
- `simple.bin` - Simple test binary
- `backgrnd.bmp` - Background image (8.3 filename format)

**Important:** FAT16 without LFN support requires 8.3 filenames (max 8 chars + 3 extension).

## Development

### Full Rebuild

To rebuild everything (UEFI bootloader, kernel, and disk image):

```bash
cd ~/edk2/MdeModulePkg/Application/MyOS
./build.sh
```

### Building Only the Kernel

If you only need to rebuild the kernel without recreating the disk:

```bash
cd /path/to/my-os/kernel
./build.sh
```

### Building User Programs

```bash
cd /path/to/my-os/kernel/programs/shell
make
```

### Clean Build

```bash
cd /path/to/my-os/kernel
make clean
```

## Troubleshooting

### "x86_64-elf-gcc: command not found"

Make sure the cross-compiler is in your PATH:
```bash
export PATH="$HOME/opt/cross/bin:$PATH"
x86_64-elf-gcc --version
```

### "Build failed: EDK2 not set up"

Run the EDK2 setup script:
```bash
cd ~/edk2
source edksetup.sh
```

### "/mnt/d: Read-only file system"

Unmount any stale mounts:
```bash
sudo umount /mnt/d
sudo losetup -D  # Detach all loop devices
```

### "GDB cannot find symbols"

Make sure you're loading the correct symbol file:
```bash
add-symbol-file /path/to/my-os/kernel/build/kernelfull.o 0x100000
```

### Image file not loading

Ensure filename follows 8.3 format (8 chars max for name, 3 for extension). Your FAT16 implementation doesn't support long filenames.

## Architecture

- **Bootloader**: UEFI application (MyOS.efi) loads kernel at 0x100000
- **Kernel**: 64-bit kernel with 4-level paging (PML4)
- **Memory**: Physical memory mapped via E820, kernel heap allocator
- **Filesystems**: FAT16 support with GPT partition table
- **Graphics**: Direct framebuffer access with double-buffering
- **Tasks**: Process switching with separate page tables per process
- **System Calls**: Software interrupt 0x80 for kernel services

## Contributing

This is a personal OS project for learning. Feel free to fork and experiment!

## License

This project is licensed under the MIT License - see the [LICENSE.md](LICENSE.md) file for details.

## Resources

- [OSDev Wiki](https://wiki.osdev.org/)
- [UEFI Specification](https://uefi.org/specifications)
- [Intel 64 and IA-32 Architectures Software Developer Manuals](https://www.intel.com/content/www/us/en/developer/articles/technical/intel-sdm.html)
- [EDK2 Documentation](https://github.com/tianocore/tianocore.github.io/wiki/EDK-II)
