hexcrack=../hex/hexcrack.exe
TD=/cygdrive/d

modes= mode1.daq mode2.daq mode3.daq mode2com.hex boot2.hex usbdrive.hex \
	mode4.daq quickcheck.hex vqcheck.hex mode7.daq

targets= ${TD}/mode1.daq ${TD}/mode2.daq ${TD}/mode3.daq ${TD}/mode4.daq \
	${TD}/mode5.daq ${TD}/mode6.daq ${TD}/mode7.daq


all: ${modes}

install: ${targets}

clean:
	rm ${modes}

mode1.daq: usbdrive.hex
	${hexcrack} -L 1000 $< $@

mode2.daq: mode2com.hex
	${hexcrack} -L 1000 $< $@

mode3.daq: ../atmelrun/erase.hex
	${hexcrack} -L 1000 $< $@

mode4.daq: ../atmelrun/run.hex
	${hexcrack} -L 1000 $< $@

mode5.daq: ../boot2/timetest.hex
	${hexcrack} -L 1000 $< $@

mode6.daq: vqcheck.hex
	${hexcrack} -L 1000 $< $@

# mode7.daq: quickcheck.hex
#	${hexcrack} -L 1000 $< $@

mode7.daq: battery.hex
	${hexcrack} -L 1000 $< $@


mode2com.hex: ../Mode2com/_output/MCHPUSB.hex
	cp $< $@

boot2.hex: ../boot2/boot2.hex
	cp $< $@

usbdrive.hex: ../USBDrive/USBDrive.hex
	cp $< $@

vqcheck.hex: ../quickcheck/vqcheck.hex
	cp $< $@

battery.hex: ../batterytest/battery.hex
	cp $< $@

quickcheck.hex: ../quickcheck/quickcheck.hex
	cp $< $@

${TD}/mode1.daq: mode1.daq
	-rm $@
	cp $< $@

${TD}/mode2.daq: mode2.daq
	-rm $@
	cp $< $@

${TD}/mode3.daq: mode3.daq
	-rm $@
	cp $< $@

${TD}/mode4.daq: mode4.daq
	-rm $@
	cp $< $@

${TD}/mode5.daq: mode5.daq
	-rm $@
	cp $< $@

${TD}/mode6.daq: mode6.daq
	-rm $@
	cp $< $@

${TD}/mode7.daq: mode7.daq
	-rm $@
	cp $< $@
