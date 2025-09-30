TARGET ?= x86_64-elf
FILES=./build/simple.o

all:
	nasm -f bin ./simple.asm -o ./build/simple.bin

clean:
	rm -rf ./simple.bin
