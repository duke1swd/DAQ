CC8E="C:\Progra~1\bknd\CC8E"

CC=${CC8E}/CC8E.exe

CFLAGS= -CC -fINHX32 -p18F2550 -I${CC8E} -L -cae

all: quickcheck.hex vqcheck.hex

quickcheck.hex: main.c adc.c slowmo.c run.c
	${CC} main.c ${CFLAGS}
	mv main.hex  quickcheck.hex

vqcheck.hex: main.c adc.c slowmo.c run.c
	${CC} -DVQC main.c ${CFLAGS}
	mv main.hex  vqcheck.hex
