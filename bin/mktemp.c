
#define	_POSIX_SOURCE

#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <getopt.h>
#include <libgen.h>
#include <limits.h>
#include <sys/stat.h>
#include <unistd.h>

#define	DEF_TEMPLATE	"tmp.XXXXXXXXXX"

static struct {
	char *exename,
	     *usage_str;
	int opt_d;
	char *opt_tmpdir;
	char *template;
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
	int opt;
	
	_g.exename = argv[0];
	_g.usage_str = "[-d] [-p [TMPDIR]] TEMPLATE...";
	while ((opt = getopt(argc, argv, ":dp:")) != -1) {
		switch (opt) {
		case 'd': _g.opt_d = 1; break;
		case 'p': _g.opt_tmpdir = optarg; break;
		case ':': _g.opt_tmpdir = getenv("TMPDIR"); break;
		default:
			usage(NULL);
			exit(EXIT_FAILURE);
		}
	}

	snprintf(_g.template = malloc(PATH_MAX+1), PATH_MAX+1, "%s/%s",
		_g.opt_tmpdir ? _g.opt_tmpdir : "/tmp",
		optind < argc ? basename(argv[optind]) : DEF_TEMPLATE);

	if ( (_g.opt_d ? mkdtemp(_g.template) : mkstemp(_g.template)) == (_g.opt_d ? NULL : -1)) {
		fprintf(stderr, "%s: error create temporary: %s\n", argv[0], strerror(errno));
		return EXIT_FAILURE;
	} else {
		printf("%s\n", _g.template);
		return EXIT_SUCCESS;
	}
}

