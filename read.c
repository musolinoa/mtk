#include <u.h>
#include <libc.h>
#include <bio.h>

char*
readline(Biobufhdr *in, char *expect, int timeout)
{
	static char *line = nil;

	alarm(timeout * 1000);
	for(;;){
		if(line != nil)
			free(line);
		line = Brdstr(in, '\n', 0);
		if(line != nil)
		if(strncmp(line, expect, strlen(expect)) == 0)
			break;
	}
	alarm(0);
	return line;
}
