#!/bin/sh
#
# Report differences between this and the original project.
#

L=.
R=/cygdrive/c/MCHPFSUSB/fw/Cdc
echo
echo MAIN.c
echo
diff $L/main.c $R/main.c
echo
echo IO_CFG.h
echo
diff $L/io_cfg.h $R/io_cfg.h

L=$L/autofiles
R=$R/autofiles

for f in $L/*
do
	name=`basename $f`
	echo
	echo $name
	echo
	diff $f $R/$name
done
