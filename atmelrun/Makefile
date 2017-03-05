CC8E=../utilities/CC8E
CC8E="C:\Progra~2\bknd\CC8E"

CC=${CC8E}/CC8E.exe

CFLAGS= -CC -fINHX32 -p18F2455 -I${CC8E} -L -cae

all: run.hex erase.hex

run.hex: main.c eprommap.h blink.c eprom.c dataflash.c prepare.c \
		adc.c run.c fat16.c tod.h tod.c slowmo.c
	${CC} -DNOERASE main.c ${CFLAGS}
	mv main.hex run.hex

erase.hex: main.c eprommap.h blink.c eprom.c dataflash.c prepare.c \
		adc.c run.c fat16.c tod.h tod.c slowmo.c
	${CC} -DERASE main.c ${CFLAGS}
	mv main.hex erase.hex
