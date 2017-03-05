CC8E="C:\Progra~1\bknd\CC8E"
RS=../atmelrun

CC=${CC8E}/CC8E.exe

CFLAGS= -CC -fINHX32 -p18F2455 -I$(RS) -I${CC8E} -L -cae

all: battery.hex

battery.hex: main.c ${RS}/eprommap.h ${RS}/blink.c ${RS}/eprom.c \
		${RS}/dataflash.c ${RS}/prepare.c \
		${RS}/adc.c run.c ${RS}/fat16.c ${RS}/tod.h \
		${RS}/tod.c ${RS}/slowmo.c
	${CC} -DNOERASE main.c ${CFLAGS}
	mv main.hex battery.hex
