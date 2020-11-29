#include <u.h>
#include <libc.h>
#include <bio.h>

enum{
	SecSz = 0x10000,
	SecHdrSz = 0x200,
};

enum{
	LogFmtUTC,
	LogFmtValid,
	LogFmtLatitude,
	LogFmtLongitude,
	LogFmtHeight,
	LogFmtSpeed,
	LogFmtHeading,
	LogFmtDSta,
	LogFmtDAge,
	LogFmtPDOP,
	LogFmtHDOP,
	LogFmtVDOP,
	LogFmtNSat,
	LogFmtSId,
	LogFmtElevation,
	LogFmtAzimuth,
	LogFmtSNR,
	LogFmtRecReason,
	LogFmtMillis,
	LogFmtDistance,

	NumLogFmtBits
};

typedef struct LogRecord LogRecord;
struct LogRecord
{
	uvlong millis;
	double latitude;
	double longitude;
	ulong speed;
};

typedef struct LogFmtBit LogFmtBit;
struct LogFmtBit
{
	char *name;
	uchar size;
};

static LogFmtBit logfmtbits[] = {
	[LogFmtUTC]			{"UTC", 4},
	[LogFmtValid]		{"VALID", 2},
	[LogFmtLatitude]	{"LATITUDE", 8},
	[LogFmtLongitude]	{"LONGITUDE", 8},
	[LogFmtHeight]		{"HEIGHT", 4},
	[LogFmtSpeed]		{"SPEED", 4},
	[LogFmtHeading]		{"HEADING", 4},
	[LogFmtDSta]		{"DSTA", 2},
	[LogFmtDAge]		{"DAGE", 4},
	[LogFmtPDOP]		{"PDOP", 2},
	[LogFmtHDOP]		{"HDOP", 2},
	[LogFmtVDOP]		{"VDOP", 2},
	[LogFmtNSat]		{"NSAT", 2},
	[LogFmtSId]			{"SID", 4},
	[LogFmtElevation]	{"ELEVATION", 2},
	[LogFmtAzimuth]		{"AZIMUTH", 2},
	[LogFmtSNR]			{"SNR", 2},
	[LogFmtRecReason]	{"RCR", 2},
	[LogFmtMillis]		{"MILLISECOND", 2},
	[LogFmtDistance]	{"DISTANCE", 8},
};

typedef struct SecHdr SecHdr;
struct SecHdr
{
	ushort cnt;
	ushort mode;
	ulong fmt;
	ulong period;
	ulong distance;
	ulong speed;
	uchar failmask[32];
};

static void
usage(void)
{
	fprint(2, "usage: %s\n", argv0);
	exits("usage");
}

ushort
getu16le(uchar *buf)
{
	ushort h;

	h = buf[1];
	h <<= 8;
	h |= buf[0];
	return h;
}

ulong
getu32le(uchar *buf)
{
	ulong w;

	w = buf[3];
	w <<= 8;
	w |= buf[2];
	w <<= 8;
	w |= buf[1];
	w <<= 8;
	w |= buf[0];
	return w;
}

static char*
logfmtstr(ulong fmt)
{
	static char buf[256];
	int i;
	char *s, *e;

	s = buf;
	e = &buf[255];
	buf[1] = '\0';

	for(i = 0; i < NumLogFmtBits; i++){
		if((fmt & ((ulong)1<<i)) != 0)
			s = seprint(s, e, ",%s", logfmtbits[i].name);
	}
	return buf+1;
}

int
parsesector(Biobufhdr *buf)
{
	uchar hdr[SecHdrSz];
	int i;
	long n;

	n = Bread(buf, hdr, SecHdrSz);

	if(n == 0)
		return 0;

	if(n != SecHdrSz)
		return -1;

	if(hdr[SecHdrSz - 6] != '*')
		return -1;

	for(i = SecHdrSz - 1; i >= SecHdrSz - 4; i--){
		if(hdr[i] != 0xbb)
			return -1;
	}

	print("count=%uhd\n", getu16le(hdr+0));
	print("fmt=%08ulx (%s)\n", getu32le(hdr+2), logfmtstr(getu32le(hdr+2)));
	print("\n");
	return i;
}

void
main(int argc, char **argv)
{
	Biobuf *buf;
	int i;

	ARGBEGIN{
	default:
		usage();
	}ARGEND;
	if(argc != 0)
		usage();
	buf = Bfdopen(0, OREAD);
	if(buf == nil)
		sysfatal("Bfdopen: %r");
	i = 0;
	for(;;){
		Bseek(buf, i*SecSz, 0);
		if(parsesector(buf) == 0)
			break;
		i++;
	}
}
