CC = gcc-4.7 # for better vectorisation support
CFLAGS=-mfpu=neon -funsafe-math-optimizations -O3 -Wall -std=c99 -fdump-tree-vect

ozone_test: ozone_test.c
	$(CC) -o ozone_test ozone_test.c -lrtlsdr -lfftw3f -lm -lpthread  $(CFLAGS)
