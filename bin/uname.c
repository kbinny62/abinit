
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <getopt.h>
#include <sys/stat.h>
#include <sys/utsname.h>
#include <unistd.h>


static struct {
	char *exename,
	     *usage_str;
	int opt_a, opt_m, opt_n,
	    opt_r, opt_s, opt_v;
} _g;


void usage(char *msg, ...)
{
	if (msg) {
		va_list ap;
		va_start(ap, msg);
		fprintf(stderr, "%s: ", _g.exename);
		vfprintf(stderr, msg, ap);
		va_end(ap);
	}
	fprintf(stderr, "Usage: %s %s\n", _g.exename, _g.usage_str);
}

int main(int argc, char *argv[])
{
	int opt, retv=EXIT_SUCCESS;
	struct utsname utsn;
	
	_g.exename = argv[0];
	_g.usage_str = "[-amnrsv]";
	while ((opt = getopt(argc, argv, "armnrsv")) != -1) {
		switch (opt) {
		case 'a': _g.opt_a = 1; break;
		case 'm': _g.opt_m = 1; break;
		case 'n': _g.opt_n = 1; break;
		case 'r': _g.opt_r = 1; break;
		case 's': _g.opt_s = 1; break;
		case 'v': _g.opt_v = 1; break;
		default:
			usage(NULL);
			exit(EXIT_FAILURE);
		}
	}

	if (optind == 1)
		_g.opt_s = 1;

	if (uname(&utsn) == -1) {
		fprintf(stderr, "%s: utsname failed: %s\n", _g.exename, strerror(errno));
		exit(EXIT_FAILURE);
	}

	if (_g.opt_m || _g.opt_a)
		printf("%s ", utsn.machine);
	if (_g.opt_n || _g.opt_a)
		printf("%s ", utsn.nodename);
	if (_g.opt_r || _g.opt_a)
		printf("%s ", utsn.release);
	if (_g.opt_s || _g.opt_a)
		printf("%s ", utsn.sysname);
	if (_g.opt_v || _g.opt_a)
		printf("%s ", utsn.version);
	putchar('\n');

	return retv;
}

