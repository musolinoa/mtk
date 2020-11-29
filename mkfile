</$objtype/mkfile

LIB=\
	common.a$O\

LIBOFILES=\
	crc.$O\
	cmd.$O\
	read.$O\

HFILES=\
	dat.h\
	fns.h\

TARG=\
	bin2txt\
	config\
	dump\
	erase

BIN=$home/bin/$objtype/mtk

</sys/src/cmd/mkmany

$BIN:
	mkdir -p $BIN

$BIN/%: $BIN

common.a$O: $LIBOFILES
	ar vu $LIB $newprereq
