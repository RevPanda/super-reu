# Test programs for the C64, built with cc65's assembler and linker.
#
#   make            Build every .prg
#   make clean      You know what this does

CA65 ?= ca65
LD65 ?= ld65

PRGS = reuvtest.prg

all : $(PRGS)

%.prg : %.s c64.cfg
	$(CA65) -t c64 -o $*.o $<
	$(LD65) -C c64.cfg -o $@ $*.o

clean :
	rm -f $(PRGS) *.o

.PHONY : all clean
