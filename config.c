#include <u.h>
#include <libc.h>
#include <bio.h>
#include "fns.h"

typedef struct Dev Dev;

struct Dev
{
	Biobuf *in;
	Biobuf *out;
};

int debug;
Dev dev;

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

static char*
docmd(char *cmd, char *expect, int timeout)
{
	Bprint(dev.out, "%s*%02x\r\n", cmd, crc(cmd));
	Bflush(dev.out);
	return readline(dev.in, expect, timeout);
}

static void
usage(void)
{
	fprint(2, "usage: %s dev\n", argv0);
	exits("usage");
}

static void
identify(void)
{
	char *line;

	line = docmd("$PMTK605", "$PMTK705,", 2);
	if(strstr(line, ",BT-Q1000EX2,") == nil){
		fprint(2, "mtk: unsupported device\n");
		exits("unsupported");
	}
}

static void
setup(void)
{
	dprint("setting log interval to 0.1s\n");
	docmd("$PMTK182,1,3,1", "$PMTK001,182,1,3", 2);

	dprint("enabling logging\n");
	docmd("$PMTK182,4", "$PMTK001,182,4,3", 2);
}

static Dev*
devopen(Dev *dev, char *path)
{
	dev->in = Bopen(path, OREAD);
	if(dev->in == nil)
		goto Error;
	dev->out = Bopen(path, OWRITE);
	if(dev->out == nil)
		goto Error;
	return dev;
Error:
	if(dev->in)
		Bterm(dev->in);
	if(dev->out)
		Bterm(dev->out);
	return nil;
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

	if(!devopen(&dev, argv[0]))
		sysfatal("devopen: %r");
	identify();
	setup();
	exits(nil);
}
