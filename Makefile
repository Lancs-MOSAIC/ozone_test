CC = gcc-4.7 # for better vectorisation support
# _GNU_SOURCE needed for some pthread features
CFLAGS=-mfpu=neon -funsafe-math-optimizations -O3 -Wall -std=c99 -D_GNU_SOURCE

OBJS = ozone_test.o calcontrol.o rtldongle.o signalproc.o compthread.o \
	recthread.o

LDFLAGS=-lrtlsdr -lfftw3f -lm -lpthread

ozone_test: $(OBJS)

calcontrol.o: calcontrol.h
ozone_test.o: calcontrol.h signalproc.h recthread.h common.h
rtldongle.o: rtldongle.h common.h
signalproc.o: signalproc.h common.h
compthread.o: compthread.h signalproc.h common.h
recthread.o: recthread.h compthread.h rtldongle.h signalproc.h calcontrol.h \
		common.h


