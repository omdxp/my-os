TARGET ?= x86_64-elf
FILES = ./build/kernel.asm.o ./build/kernel.o ./build/string/string.o ./build/memory/heap/heap.o ./build/memory/heap/kheap.o ./build/memory/memory.o ./build/memory/paging/paging.o ./build/memory/paging/paging.asm.o ./build/memory/heap/multiheap.o ./build/io/io.asm.o ./build/idt/idt.o ./build/idt/idt.asm.o ./build/task/task.asm.o ./build/task/task.o ./build/task/process.o ./build/fs/fat/fat16.o ./build/fs/file.o ./build/fs/pparser.o ./build/disk/disk.o ./build/disk/streamer.o ./build/gdt/gdt.o ./build/task/tss.asm.o ./build/keyboard/keyboard.o ./build/keyboard/ps2.o ./build/isr80h/isr80h.o ./build/isr80h/io.o ./build/isr80h/misc.o ./build/isr80h/heap.o ./build/isr80h/process.o ./build/loader/formats/elf.o ./build/loader/formats/elfloader.o ./build/idt/irq.o
INCLUDES = -I./src
FLAGS = -g -ffreestanding -falign-jumps -falign-functions -falign-labels -falign-loops -fstrength-reduce -fomit-frame-pointer -finline-functions -Wno-unused-function -fno-builtin -Werror -Wno-unused-label -Wno-cpp -Wno-unused-parameter -nostdlib -nostartfiles -nodefaultlibs -Wall -O0 -Iinc

all: ./bin/boot.bin ./bin/kernel.bin user_programs
	rm -rf ./bin/os.bin
	dd if=./bin/boot.bin >> ./bin/os.bin
	dd if=./bin/kernel.bin >> ./bin/os.bin
	dd if=/dev/zero bs=1048576 count=16 >> ./bin/os.bin
	sudo mkdir -p /mnt/d
	sudo mount -t vfat ./bin/os.bin /mnt/d
	# copy a file over
	sudo cp ./hello.txt /mnt/d
	sudo cp ./programs/blank/blank.elf /mnt/d
	sudo cp ./programs/echo/echo.elf /mnt/d
	sudo cp ./programs/shell/shell.elf /mnt/d
	sudo cp ./programs/simple/build/simple.bin /mnt/d
	sudo umount /mnt/d

./bin/kernel.bin: $(FILES)
	$(TARGET)-ld -g -relocatable $(FILES) -o ./build/kernelfull.o
	$(TARGET)-gcc $(FLAGS) -T ./src/linker.ld -o ./bin/kernel.bin -ffreestanding -O0 -nostdlib ./build/kernelfull.o

./bin/boot.bin: ./src/boot/boot.asm
	nasm -f bin ./src/boot/boot.asm -o ./bin/boot.bin

./build/kernel.asm.o: ./src/kernel.asm
	nasm -f elf64 -g ./src/kernel.asm -o ./build/kernel.asm.o

./build/kernel.o: ./src/kernel.c
	$(TARGET)-gcc $(INCLUDES) $(FLAGS) -std=gnu99 -c ./src/kernel.c -o ./build/kernel.o

./build/idt/idt.asm.o: ./src/idt/idt.asm
	nasm -f elf64 -g ./src/idt/idt.asm -o ./build/idt/idt.asm.o

./build/gdt/gdt.o: ./src/gdt/gdt.c
	$(TARGET)-gcc $(INCLUDES) -I./src/gdt $(FLAGS) -std=gnu99 -c ./src/gdt/gdt.c -o ./build/gdt/gdt.o

./build/loader/formats/elf.o: ./src/loader/formats/elf.c
	$(TARGET)-gcc $(INCLUDES) -I./src/loader/formats $(FLAGS) -std=gnu99 -c ./src/loader/formats/elf.c -o ./build/loader/formats/elf.o

./build/loader/formats/elfloader.o: ./src/loader/formats/elfloader.c
	$(TARGET)-gcc $(INCLUDES) -I./src/loader/formats $(FLAGS) -std=gnu99 -c ./src/loader/formats/elfloader.c -o ./build/loader/formats/elfloader.o

./build/keyboard/keyboard.o: ./src/keyboard/keyboard.c
	$(TARGET)-gcc $(INCLUDES) -I./src/keyboard $(FLAGS) -std=gnu99 -c ./src/keyboard/keyboard.c -o ./build/keyboard/keyboard.o

./build/keyboard/ps2.o: ./src/keyboard/ps2.c
	$(TARGET)-gcc $(INCLUDES) -I./src/keyboard $(FLAGS) -std=gnu99 -c ./src/keyboard/ps2.c -o ./build/keyboard/ps2.o

./build/gdt/gdt.asm.o: ./src/gdt/gdt.asm
	nasm -f elf64 -g ./src/gdt/gdt.asm -o ./build/gdt/gdt.asm.o

./build/isr80h/isr80h.o: ./src/isr80h/isr80h.c
	$(TARGET)-gcc $(INCLUDES) -I./src/isr80h $(FLAGS) -std=gnu99 -c ./src/isr80h/isr80h.c -o ./build/isr80h/isr80h.o

./build/isr80h/misc.o: ./src/isr80h/misc.c
	$(TARGET)-gcc $(INCLUDES) -I./src/isr80h $(FLAGS) -std=gnu99 -c ./src/isr80h/misc.c -o ./build/isr80h/misc.o

./build/isr80h/io.o: ./src/isr80h/io.c
	$(TARGET)-gcc $(INCLUDES) -I./src/isr80h $(FLAGS) -std=gnu99 -c ./src/isr80h/io.c -o ./build/isr80h/io.o

./build/isr80h/heap.o: ./src/isr80h/heap.c
	$(TARGET)-gcc $(INCLUDES) -I./src/isr80h $(FLAGS) -std=gnu99 -c ./src/isr80h/heap.c -o ./build/isr80h/heap.o

./build/isr80h/process.o: ./src/isr80h/process.c
	$(TARGET)-gcc $(INCLUDES) -I./src/isr80h $(FLAGS) -std=gnu99 -c ./src/isr80h/process.c -o ./build/isr80h/process.o

./build/idt/idt.o: ./src/idt/idt.c
	$(TARGET)-gcc $(INCLUDES) -I./src/idt $(FLAGS) -std=gnu99 -c ./src/idt/idt.c -o ./build/idt/idt.o

./build/idt/irq.o: ./src/idt/irq.c
	$(TARGET)-gcc $(INCLUDES) -I./src/idt $(FLAGS) -std=gnu99 -c ./src/idt/irq.c -o ./build/idt/irq.o

./build/memory/memory.o: ./src/memory/memory.c
	$(TARGET)-gcc $(INCLUDES) -I./src/memory $(FLAGS) -std=gnu99 -c ./src/memory/memory.c -o ./build/memory/memory.o

./build/task/task.asm.o: ./src/task/task.asm
	nasm -f elf64 -g ./src/task/task.asm -o ./build/task/task.asm.o

./build/task/tss.asm.o: ./src/task/tss.asm
	nasm -f elf64 -g ./src/task/tss.asm -o ./build/task/tss.asm.o

./build/task/task.o: ./src/task/task.c
	$(TARGET)-gcc $(INCLUDES) -I./src/task $(FLAGS) -std=gnu99 -c ./src/task/task.c -o ./build/task/task.o

./build/task/process.o: ./src/task/process.c
	$(TARGET)-gcc $(INCLUDES) -I./src/task $(FLAGS) -std=gnu99 -c ./src/task/process.c -o ./build/task/process.o

./build/io/io.asm.o: ./src/io/io.asm
	nasm -f elf64 -g ./src/io/io.asm -o ./build/io/io.asm.o

./build/memory/heap/multiheap.o: ./src/memory/heap/multiheap.c
	$(TARGET)-gcc $(INCLUDES) -I./src/memory/heap $(FLAGS) -std=gnu99 -c ./src/memory/heap/multiheap.c -o ./build/memory/heap/multiheap.o

./build/memory/heap/heap.o: ./src/memory/heap/heap.c
	$(TARGET)-gcc $(INCLUDES) -I./src/memory/heap $(FLAGS) -std=gnu99 -c ./src/memory/heap/heap.c -o ./build/memory/heap/heap.o

./build/memory/heap/kheap.o: ./src/memory/heap/kheap.c
	$(TARGET)-gcc $(INCLUDES) -I./src/memory/heap $(FLAGS) -std=gnu99 -c ./src/memory/heap/kheap.c -o ./build/memory/heap/kheap.o

./build/memory/paging/paging.o: ./src/memory/paging/paging.c
	$(TARGET)-gcc $(INCLUDES) -I./src/memory/paging $(FLAGS) -std=gnu99 -c ./src/memory/paging/paging.c -o ./build/memory/paging/paging.o

./build/memory/paging/paging.asm.o: ./src/memory/paging/paging.asm
	nasm -f elf64 -g ./src/memory/paging/paging.asm -o ./build/memory/paging/paging.asm.o

./build/disk/disk.o: ./src/disk/disk.c
	$(TARGET)-gcc $(INCLUDES) -I./src/disk $(FLAGS) -std=gnu99 -c ./src/disk/disk.c -o ./build/disk/disk.o

./build/disk/streamer.o: ./src/disk/streamer.c
	$(TARGET)-gcc $(INCLUDES) -I./src/disk $(FLAGS) -std=gnu99 -c ./src/disk/streamer.c -o ./build/disk/streamer.o

./build/fs/fat/fat16.o: ./src/fs/fat/fat16.c
	$(TARGET)-gcc $(INCLUDES) -I./src/fs -I./src/fat $(FLAGS) -std=gnu99 -c ./src/fs/fat/fat16.c -o ./build/fs/fat/fat16.o

./build/fs/file.o: ./src/fs/file.c
	$(TARGET)-gcc $(INCLUDES) -I./src/fs $(FLAGS) -std=gnu99 -c ./src/fs/file.c -o ./build/fs/file.o

./build/fs/pparser.o: ./src/fs/pparser.c
	$(TARGET)-gcc $(INCLUDES) -I./src/fs $(FLAGS) -std=gnu99 -c ./src/fs/pparser.c -o ./build/fs/pparser.o

./build/string/string.o: ./src/string/string.c
	$(TARGET)-gcc $(INCLUDES) -I./src/string $(FLAGS) -std=gnu99 -c ./src/string/string.c -o ./build/string/string.o

user_programs:
	cd ./programs/simple && $(MAKE) all
	cd ./programs/stdlib && $(MAKE) all
	cd ./programs/blank && $(MAKE) all
	cd ./programs/echo && $(MAKE) all
	cd ./programs/shell && $(MAKE) all

clean:
	find . -name '*.o' -delete
	find . -name '*.bin' -delete
	find . -name '*.elf' -delete
