all: readdata.exe read3data.exe downsample.exe msgfromseq.exe

readdata.exe: readdata.c print_tod.c
	gcc -Wall -o readdata readdata.c print_tod.c

read3data.exe: read3data.c print_tod.c
	gcc -Wall -o read3data read3data.c print_tod.c

downsample.exe: downsample.c
	gcc -Wall -o downsample downsample.c

msgfromseq.exe: msgfromseq.c
	gcc -Wall -o msgfromseq msgfromseq.c
