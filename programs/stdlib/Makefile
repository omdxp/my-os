FILES=./build/start.o ./build/myos.o
INCLUDES=

all: $(FILES)
	i686-elf-ld -m elf_i386 -relocatable $(FILES) -o ./stdlib.elf

./build/start.o: ./src/start.asm
	nasm -f elf ./src/start.asm -o ./build/start.o

./build/myos.o: ./src/myos.asm
	nasm -f elf ./src/myos.asm -o ./build/myos.o

clean:
	rm -rf $(FILES)
	rm -rf ./stdlib.elf
