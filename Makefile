CC = gcc-4.7 # for better vectorisation support
CFLAGS=-mfpu=neon -funsafe-math-optimizations -O3 -Wall -std=c99

OBJS = ozone_test.o calcontrol.o rtldongle.o signalproc.o

LDFLAGS=-lrtlsdr -lfftw3f -lm -lpthread

ozone_test: $(OBJS)

calcontrol.o: calcontrol.h
ozone_test.o: calcontrol.h rtldongle.h signalproc.h common.h
rtldongle.o: rtldongle.h common.h
signalproc.o: signalproc.h common.h



