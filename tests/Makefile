# Test programs for the C64. The assembly ones are built with cc65's
# assembler and linker, the C ones with llvm-mos.
#
#   make            Build every .prg
#   make clean      You know what this does

CA65 ?= ca65
LD65 ?= ld65
MOS_C64_CLANG ?= mos-c64-clang

# The SDK's exit() loops for ever; save-basic.o puts BASIC's zero page
# back and returns to READY.
CFLAGS = -Os -Wall
LDLIBS = -l:save-basic.o

PRGS = reuvtest.prg reuvhist.prg

all : $(PRGS)

%.prg : %.s c64.cfg
	$(CA65) -t c64 -o $*.o $<
	$(LD65) -C c64.cfg -o $@ $*.o

reuvhist.prg : reuvhist.c reu.h
	$(MOS_C64_CLANG) $(CFLAGS) -o $@ $< $(LDLIBS)

clean :
	rm -f $(PRGS) $(PRGS:=.elf) *.o

.PHONY : all clean
