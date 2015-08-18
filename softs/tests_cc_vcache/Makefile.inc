all: test.elf

COMMON= ../common
LDSCRIPT?= ../common/ldscript

CC= mipsel-unknown-elf-gcc
LD= mipsel-unknown-elf-ld
DU= mipsel-unknown-elf-objdump

CFLAGS= -mips32 -nostdinc -I. -I${COMMON} -mno-abicalls
### LDFLAGS=-mips32 -nostdlib -Wl,-T${LDSCRIPT} -Ttext=0xbfc00000

#### object files generation
test.o: test.S 
	${CC} ${CFLAGS} -c -o $@ $<
	${DU} -D $@ > $@.txt

subr.o: ${COMMON}/subr.S
	${CC} ${CFLAGS} -c -o $@ $<
	${DU} -D $@ > $@.txt

### linking : test.o has to be the first object 
test.elf: test.o subr.o ${EXTRA_OBJ}
	${LD} -o $@ -T ${LDSCRIPT} -Ttext 0xbfc00000 $^
	${DU} -D $@ > $@.txt
clean:
	rm -f *.o test.elf *.txt run.out temp
