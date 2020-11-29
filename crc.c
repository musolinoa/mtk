#include <u.h>
#include <libc.h>

uchar
crc(char *s)
{
	uchar crc;
	int i, len;

	crc = 0;
	len = strlen(s);
	for(i = 1; i < len; i++)
		crc ^= s[i];
	return crc;
}
