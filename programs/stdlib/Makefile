FILES=./build/start.asm.o ./build/myos.asm.o ./build/start.o ./build/stdlib.o ./build/stdio.o ./build/myos.o ./build/string.o ./build/memory.o
INCLUDES=-I./src
FLAGS=-g -falign-jumps -falign-functions -falign-labels -falign-loops -fstrength-reduce -fomit-frame-pointer -finline-functions -Wno-unused-function -fno-builtin -Werror -Wno-unused-label -Wno-cpp -Wno-unused-parameter -nostdlib -nostartfiles -nodefaultlibs -Wall -O0 -Iinc

all: $(FILES)
	i686-elf-ld -m elf_i386 -relocatable $(FILES) -o ./stdlib.elf

./build/start.asm.o: ./src/start.asm
	nasm -f elf ./src/start.asm -o ./build/start.asm.o

./build/myos.asm.o: ./src/myos.asm
	nasm -f elf ./src/myos.asm -o ./build/myos.asm.o

./build/myos.o: ./src/myos.c
	i686-elf-gcc $(INCLUDES) $(FLAGS) -std=gnu99 -c ./src/myos.c -o ./build/myos.o

./build/start.o: ./src/start.c
	i686-elf-gcc $(INCLUDES) $(FLAGS) -std=gnu99 -c ./src/start.c -o ./build/start.o

./build/stdlib.o: ./src/stdlib.c
	i686-elf-gcc $(INCLUDES) $(FLAGS) -std=gnu99 -c ./src/stdlib.c -o ./build/stdlib.o

./build/stdio.o: ./src/stdio.c
	i686-elf-gcc $(INCLUDES) $(FLAGS) -std=gnu99 -c ./src/stdio.c -o ./build/stdio.o

./build/string.o: ./src/string.c
	i686-elf-gcc $(INCLUDES) $(FLAGS) -std=gnu99 -c ./src/string.c -o ./build/string.o

./build/memory.o: ./src/memory.c
	i686-elf-gcc $(INCLUDES) $(FLAGS) -std=gnu99 -c ./src/memory.c -o ./build/memory.o

clean:
	rm -rf $(FILES)
	rm -rf ./stdlib.elf
