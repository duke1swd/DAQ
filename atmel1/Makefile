CC8E="C:\Progra~1\bknd\CC8E"

CC=${CC8E}/CC8E.exe

CFLAGS= -CC -fINHX32 -p18F2455 -I${CC8E} -L

all: atmel1.hex

atmel1.hex: main.c blink.c
	${CC} main.c ${CFLAGS}
