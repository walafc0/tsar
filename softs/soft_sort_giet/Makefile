LD = mipsel-unknown-elf-ld
CC = mipsel-unknown-elf-gcc
AS = mipsel-unknown-elf-as
DU = mipsel-unknown-elf-objdump

OBJS = reset.o \
       giet.o \
       stdio.o \
       main.o

CFLAGS = -Wall -mno-gpopt -ffreestanding -fomit-frame-pointer -mips32

GIET =	../giet_tsar

bin.soft: $(OBJS) giet.ld
	$(LD) -o $@ -T giet.ld $(OBJS)
	$(DU) -D $@ > $@.txt

reset.o: $(GIET)/reset.S hard_config.h
	$(CC) -I. $(CFLAGS) -c -o $@ $<
	$(DU) -D $@ > $@.txt

giet.o: $(GIET)/giet.S hard_config.h
	$(CC) -I. $(CFLAGS) -c -o $@ $<
	$(DU) -D $@ > $@.txt

stdio.o: $(GIET)/stdio.c hard_config.h
	$(CC) -I. $(CFLAGS) -c -o $@ $<
	$(DU) -D $@ > $@.txt

main.o: main.c hard_config.h
	$(CC) -I. $(CFLAGS) -I$(GIET) -c -o $@ $<
	$(DU) -D $@ > $@.txt

giet.ld: giet.ld.in
	$(CC) -x c -P -E -I. $< -o $@

clean:
	rm -f *.o bin.soft *.txt core term* temp giet.ld
