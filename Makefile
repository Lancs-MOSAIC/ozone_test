CC = gcc-4.9 # Use gcc >= 4.7 for better vectorisation support
# _GNU_SOURCE needed for some pthread features
CFLAGS=-mfpu=neon -funsafe-math-optimizations -O3 -Wall -std=c99 -D_GNU_SOURCE

OBJS = ozonespec.o calcontrol.o rtldongle.o signalproc.o compthread.o \
	recthread.o config.o

LDFLAGS=-lrtlsdr -lfftw3f -lm -lpthread -lrt

all: ozonespec dtoverlay gaincheck

ozonespec: $(OBJS)

calcontrol.o: calcontrol.h
ozonespec.o: calcontrol.h signalproc.h recthread.h rtldongle.h config.h common.h
rtldongle.o: rtldongle.h common.h config.h
signalproc.o: signalproc.h common.h
compthread.o: compthread.h signalproc.h common.h
recthread.o: recthread.h compthread.h rtldongle.h signalproc.h calcontrol.h \
		config.h common.h
config.o: common.h


dtoverlay: MOSAIC-cape-00A0.dtbo

gaincheck: gaincheck.c common.h
	$(CC) -o gaincheck gaincheck.c -lrtlsdr -lm $(CFLAGS)

# Device tree overlays
%.dtbo: %.dts
	dtc -O dtb -o $@ -b 0 -@ $<

.PHONY : clean
clean:
	$(RM) *.o
