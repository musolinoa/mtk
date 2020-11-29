#include <u.h>
#include <libc.h>
#include <bio.h>
#include <ctype.h>
#include "fns.h"

int debug;

int
dprint(char *fmt, ...)
{
	int n;
	va_list args;

	if(!debug)
		return 0;
	va_start(args, fmt);
	n = vfprint(2, fmt, args);
	va_end(args);
	return n;
}

typedef struct Dev Dev;
typedef struct Nibbler Nibbler;

struct Dev
{
	Biobuf *in;
	Biobuf *out;
};

struct Nibbler
{
	uint len;
	uint cap;
	uchar *buf;
	uchar tmp;
	uchar idx;
};

static void
initnibbler(Nibbler *n, uint cap)
{
	n->len = 0;
	n->cap = cap;
	n->buf = malloc(n->cap);
	if(n->buf == nil)
		sysfatal("out of memory");
	n->idx = 0;
}

static void
putnibble(Nibbler *n, uchar nibble)
{
	if(n->idx == 0)
		n->tmp = nibble;
	else{
		if(n->len == n->cap){
			n->cap *= 2;
			n->buf = realloc(n->buf, n->cap);
		}
		n->buf[n->len++] = n->tmp << 4 | nibble;
	}
	n->idx = !n->idx;
}

static void
resetnibbler(Nibbler *n)
{
	n->len = 0;
	n->idx = 0;
}

static void
freenibbler(Nibbler *n)
{
	free(n->buf);
}

Dev dev;

static void
usage(void)
{
	fprint(2, "usage: %s dev\n", argv0);
	exits("usage");
}

static char*
docmd(char *cmd, char *expect)
{
	Bprint(dev.out, "%s*%02x\r\n", cmd, crc(cmd));
	Bflush(dev.out);
	return readline(dev.in, expect, 5000);
}

static void
identify(void)
{
	char *line;

	line = docmd("$PMTK605", "$PMTK705,");
	if(strstr(line, ",BT-Q1000EX2,") == nil){
		fprint(2, "mtk: unsupported device\n");
		exits("unsupported");
	}
}

static char*
strnchr(char *s, int n, char c)
{
	if(n <= 0)
		return nil;
	while(n-- > 0){
		s = strchr(s, c);
		if(s == nil)
			break;
		s++;
	}
	return s;
}

static uchar*
parsebyte(char *s, uchar *buf)
{
	if(!isxdigit(*s)){
		fprint(2, "parsebyte: expected hex digit, got %c\n", *s);
		return nil;
	}
	if(isalpha(*s))
		*buf = tolower(*s) - 'a' + 10;
	else
		*buf = *s - '0';
	*buf <<= 4;
	s++;
	if(!isxdigit(*s)){
		fprint(2, "parsebyte: expected hex digit, got %c\n", *s);
		return nil;
	}
	if(isalpha(*s))
		*buf |= tolower(*s) - 'a' + 10;
	else
		*buf |= *s - '0';
	return ++buf;
}

static uchar*
dordlogcmd(uchar *buf, ulong off, ulong len)
{
	ulong n;
	uchar *ebuf;
	char *f, *line;
	char cmd[32];

	snprint(cmd, sizeof(cmd), "$PMTK182,7,%08ulx,%08ulx", off, len);
	line = docmd(cmd, "$PMTK182,8,");
	//fprint(2, "log output: %s", line);
	f = strnchr(line, 2, ',');
	if(f == nil)
		sysfatal("bad data log response");
	f++;
	n = strtoul(f, nil, 16);
	if(n != off)
		sysfatal("data log response for wrong offset");
	f = strchr(f, ',');
	if(f == nil)
		sysfatal("empty data log response");
	f++;
	ebuf = buf + len;
	while(buf < ebuf && *f != '*'){
		if(parsebyte(f, buf) == nil)
			sysfatal("parsebyte failed");
		buf++;
		f += 2;
	}
	return buf;
}

static void
download(void)
{
	uchar *buf, *bp, *nbp;
	ulong addr, maxaddr;
	char *f, *line;

	line = docmd("$PMTK182,2,7", "$PMTK182,3,7,");
	dprint("got log status response! %s", line);

	sleep(10);

	line = docmd("$PMTK182,5", "$PMTK");
	dprint("got log disable response! %s", line);

	sleep(100);

	line = docmd("$PMTK182,2,8", "$PMTK182,3,8,");
	dprint("got flash usage response! %s", line);

	f = strnchr(line, 3, ',');
	if(f == nil)
		sysfatal("unexpected response");
	f++;
	maxaddr = strtoul(f, nil, 16);
	if(maxaddr == 0)
		sysfatal("could not determine flash usage");
	dprint("downloading %uldKB from device\n", (maxaddr+1)>>10);
	buf = malloc(maxaddr + 1);
	if(buf == nil)
		sysfatal("could not allocate data buffer");
	bp = buf;
	addr = 0;
	while(addr < maxaddr){
		nbp = dordlogcmd(bp, addr, 0x100);
		write(1, bp, nbp - bp);
		addr += nbp - bp;
		bp = nbp;
		dprint("progess: %uld/%uld\n", addr, maxaddr);
	}
}

static void
dordlogcmd2(Nibbler *nblr, ulong off, ulong len)
{
	ulong n, chklen;
	char *f, *line;
	char cmd[32];

	snprint(cmd, sizeof(cmd), "$PMTK182,7,%.8ulX,%.8ulX", off, len);
dprint("cmd=%s\n", cmd);
	line = docmd(cmd, "$PMTK182,8,");
	goto Seek;
	while(len > 0){
		chklen = nblr->len;
		while(*f != '*'){
			if(!isxdigit(*f))
				sysfatal("bad character in response");
			if(isdigit(*f))
				putnibble(nblr, *f - '0');
			else
				putnibble(nblr, toupper(*f) - 'A');
			f++;
		}
		chklen = nblr->len - chklen;
dprint("got %uld byte chunk\n", chklen);
		len -= chklen;
		off += chklen;
		line = readline(dev.in, "$PMTK", 5000);
	Seek:
		if(strncmp(line, "$PMTK001,182,7,", 15) == 0){
			dprint("@@@@ got ack!\n");
			return;
		}
		if(strncmp(line, "$PMTK182,8", 10) != 0)
			sysfatal("unexpected data log response");
		dprint("log output: %s", line);
		f = strnchr(line, 2, ',');
		if(f == nil)
			sysfatal("bad data log response");
		f++;
		n = strtoul(f, nil, 16);
		//if(n != off)
		//	sysfatal("data log response for wrong offset");
		f = strchr(f, ',');
		if(f == nil)
			sysfatal("empty data log response");
		f++;
		while(n < off){
			n++;
			f += 2;
		}
	}
}

static void
download2(void)
{
	Nibbler n;
	ulong addr, maxaddr;
	char *f, *line;

	line = docmd("$PMTK182,2,7", "$PMTK182,3,7,");
	dprint("got log status response! %s", line);

	sleep(10);

	line = docmd("$PMTK182,5", "$PMTK");
	dprint("got log disable response! %s", line);

	sleep(100);

	line = docmd("$PMTK182,2,8", "$PMTK182,3,8,");
	dprint("got flash usage response! %s", line);

	f = strnchr(line, 3, ',');
	if(f == nil)
		sysfatal("unexpected response");
	f++;
	maxaddr = strtoul(f, nil, 16);
	if(maxaddr == 0)
		sysfatal("could not determine flash usage");
	dprint("downloading %uld bytes from device\n", maxaddr+1);

	initnibbler(&n, 0x400);
	addr = 0;
	maxaddr = 0x400;
	while(addr < maxaddr){
		dordlogcmd2(&n, addr, 0x400);
		write(1, n.buf, n.len);
		addr += n.len;
		dprint("progess: %.2f\n", 100.0*addr/maxaddr);
		resetnibbler(&n);
	}
	freenibbler(&n);
}

void
main(int argc, char **argv)
{
	ARGBEGIN{
	case 'd':
		debug++;
		break;
	default:
		usage();
	}ARGEND;
	if(argc != 1)
		usage();
	dev.in = Bopen(argv[0], OREAD);
	if(dev.in == nil)
		sysfatal("open: %r");
	dev.out = Bopen(argv[0], OWRITE);
	if(dev.out == nil)
		sysfatal("open: %r");
	identify();
	download2();
}
