CC8E="C:\Progra~1\bknd\CC8E"

CC=${CC8E}/CC8E.exe

CFLAGS= -CC -fINHX32 -p18F2550 -I${CC8E} -L

all: boot2.hex small.hex medium.hex pgmtest.hex porttest.hex timetest.hex

boot2.hex: main.c eprommap.h sd.c blink.c eprom.c boot.c dataflash.c
	${CC} main.c ${CFLAGS}
	mv main.hex boot2.hex

small.hex: small.c
	${CC} small.c ${CFLAGS}

medium.hex: medium.c downcall.c
	${CC} medium.c ${CFLAGS}

pgmtest.hex: pgmtest.c blink.c
	${CC} pgmtest.c ${CFLAGS}

porttest.hex: porttest.c
	${CC} porttest.c ${CFLAGS}

timetest.hex: timetest.c downcall.c blink.c
	${CC} timetest.c ${CFLAGS}
