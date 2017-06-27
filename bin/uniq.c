
#define	_POSIX_SOURCE

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
	int opt_c;	/* output repeat count */
} _g;


/**
 *  formats error message and append strerror(errno) */
static int ERR(const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	fprintf(stderr, "%s: ", _g.exename);
	vfprintf(stderr, fmt, ap);
	fprintf(stderr, ": %s\n", strerror(errno));
	va_end(ap);
	return EXIT_FAILURE;
}

static int usage(char *msg, ...)
{
	if (msg) {
		va_list ap;
		va_start(ap, msg);
		fprintf(stderr, "%s: ", _g.exename);
		vfprintf(stderr, msg, ap);
		va_end(ap);
	}
	fprintf(stderr, "Usage: %s %s\n", _g.exename, _g.usage_str);
	return EXIT_FAILURE;
}

int main(int argc, char *argv[])
{
	int opt, retv=EXIT_SUCCESS;
	FILE *in = stdin, *out = stdout;
	char *line = NULL, *lastline = NULL;
	size_t n = 0, rep_count;
	
	_g.exename = argv[0];
	_g.usage_str = "[-c] [INFILE [OUTFILE]]";
	while ((opt = getopt(argc, argv, "c")) != -1) {
		switch (opt) {
		case 'c': _g.opt_c = 1; break;
		default:
			exit(usage(NULL));
		}
	}

	if (argc-optind > 0)
		if ((in = strcmp(argv[optind], "-") ? fopen(argv[optind], "r") : stdin) == NULL)
			return ERR("error opening '%s' (input)", argv[optind]);
	if (argc-optind > 1)
		if ((out = strcmp(argv[optind+1], "-") ? fopen(argv[optind+1], "w") : stdout) == NULL)
			return ERR("error opening '%s' (output)", argv[optind+1]);

	for (rep_count = 1; (opt = getline(&line, &n, in)) || 1!=0; ) {
		/* Handle special cases of EOF and I/O error */
		if (opt == -1 && feof(in)) {
			if (_g.opt_c)
				fprintf(out, "%4lu ", rep_count);
			fprintf(out, "%s", line);
			break;
		} else if (opt == -1 && ferror(in))
			return ERR("error reading '%s'", argv[optind]);

		if (lastline != NULL) {
			if (strcmp(lastline, line) != 0) {
				if (_g.opt_c)
					fprintf(out, "%4lu ", rep_count);
				fprintf(out, "%s", lastline);
				free(lastline);
				lastline = strdup(line);
				rep_count = 1;
			} else {
				rep_count++;
				continue;
			}
		} else
			lastline = strdup(line);
	}

	return retv;
}

