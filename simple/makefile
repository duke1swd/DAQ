CC8E="C:\Progra~1\bknd\CC8E"

CC=${CC8E}/CC8E.exe

CFLAGS= -CC -fINHX32 -p18F1320 -I${CC8E} -L

all: simple.hex simpleV2.hex simpleMarch.hex simpleApril.hex

simpleApril.hex: simpleApril.c eprom.c
	${CC} simpleApril.c ${CFLAGS}

simpleMarch.hex: simpleMarch.c eprom.c
	${CC} simpleMarch.c ${CFLAGS}

simple.hex: simple.c eprom.c
	${CC} simple.c ${CFLAGS}

simpleV2.hex: simpleV2.c eprom.c
	${CC} simpleV2.c ${CFLAGS}

sonic.hex: sonic.c
	${CC} sonic.c ${CFLAGS}

adcsample.hex: adcsample.c
	${CC} adcsample.c -CC -p18F2455 -I${CC8E} -L
