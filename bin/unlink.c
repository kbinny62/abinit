
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <getopt.h>
#include <sys/stat.h>
#include <unistd.h>


static struct {
	char *exename,
	     *usage_str;
	int opt_v;
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
	exit(EXIT_FAILURE);
}

int main(int argc, char *argv[])
{
	int opt, retv=EXIT_SUCCESS;
	
	_g.exename = argv[0];
	_g.usage_str = "[-v] FILE...";
	while ((opt = getopt(argc, argv, "v")) != -1) {
		switch (opt) {
		case 'v': _g.opt_v = 1; break;
		default:
			usage(NULL);
			exit(EXIT_FAILURE);
		}
	}

	if (optind >= argc)
		usage(NULL);
	else for ( ; optind < argc; optind++ ) {
		if (_g.opt_v)
			printf("%s: %s\n", _g.exename, argv[optind]);
		if (unlink(argv[optind]) == -1) {
			fprintf(stderr, "%s: failed to unlink '%s': %s\n", _g.exename, argv[optind], strerror(errno));
			retv |= EXIT_FAILURE;
		}
	}

	return retv;
}

