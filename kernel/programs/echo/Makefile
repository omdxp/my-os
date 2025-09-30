TARGET ?= x86_64-elf
FILES=./build/echo.o
INCLUDES=-I../stdlib/src
FLAGS=-g -ffreestanding -falign-jumps -falign-functions -falign-labels -falign-loops -fstrength-reduce -fomit-frame-pointer -finline-functions -Wno-unused-function -fno-builtin -Werror -Wno-unused-label -Wno-cpp -Wno-unused-parameter -nostdlib -nostartfiles -nodefaultlibs -Wall -O0 -Iinc

all: $(FILES)
	$(TARGET)-gcc -g -T ./linker.ld -o ./echo.elf -ffreestanding -O0 -nostdlib -fpic -g $(FILES) ../stdlib/stdlib.elf

./build/echo.o: ./echo.c
	$(TARGET)-gcc $(INCLUDES) -I./ $(FLAGS) -std=gnu99 -c ./echo.c -o ./build/echo.o

clean:
	rm -rf $(FILES)
	rm -rf ./echo.elf
