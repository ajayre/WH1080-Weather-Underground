CC=gcc
CFLAGS=-c -Wall

all: wh1080_rf

wh1080_rf: wh1080_rf.o bcm2835.o bmp085.o wunderground.o
	$(CC) -lm wh1080_rf.o bcm2835.o bmp085.o wunderground.o -o wh1080_rf

wh1080_rf.o: wh1080_rf.c
	$(CC) $(CFLAGS) wh1080_rf.c

bcm2835.o: bcm2835.c
	$(CC) $(CFLAGS) bcm2835.c

bmp085.o: bmp085.c
	$(CC) $(CFLAGS) bmp085.c

wunderground.o: wunderground.c
	$(CC) $(CFLAGS) wunderground.c

clean:
	rm -f wh1080_rf.o bcm2835.o bmp085.o wunderground.o wh1080_rf
