
#include <assert.h>
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <getopt.h>
#include <sys/stat.h>
#include <unistd.h>

struct integer_range {
	long lo_bound,
	     hi_bound;
};

static struct {
	char *exename,
	     *usage_str;

	char *opt_delim;

	char *opt_fields;
	struct integer_range *fields;
	long num_fields;
} _g;


static void str_to_ranged_list(char *str, struct integer_range **, size_t *);
static int do_cut(char *filename);
static void do_fields(char *line);
static void usage(char *msg, ...);


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

int do_cut(char *filename)
{
	FILE *fp = strcmp(filename, "-") ? fopen(filename, "r") : stdin;
	char *line = NULL;
	ssize_t n = 0;


	if (fp == NULL) {
		fprintf(stderr, "%s: opening '%s' failed: %s\n", _g.exename, filename, strerror(errno));
		return EXIT_FAILURE;
	} else while (getline(&line, &n, fp) != -1) {
		if (line[strlen(line)-1] == '\n')
			line[strlen(line)-1] = '\0';
		if (_g.opt_fields)
			do_fields(line);
	}

	if (ferror(fp))
		fprintf(stderr, "%s: error reading '%s': %s\n", _g.exename, filename, strerror(errno));

	if (line) free(line);
	if (fp != stdin)
		fclose(fp);
	return EXIT_SUCCESS;
}

void do_fields(char *line)
{
	long fnr = 0;
	for (char *p = strtok(line, _g.opt_delim); p; p = strtok(NULL, _g.opt_delim)) {
		fnr++;
		for (int i = 0; i < _g.num_fields; i++)
			if (fnr >= _g.fields[i].lo_bound && fnr <= _g.fields[i].hi_bound) {
				if (i > 0) printf("%s", _g.opt_delim[0]);
				printf("%s", p);
			}
	}
	putchar('\n');
}

void str_to_ranged_list(char *str, struct integer_range **ranges, size_t *num_ranges)
{
	char *p;
	assert(str != NULL && ranges && num_ranges != NULL);
	for (p = strtok(str, ","); p; p = strtok(NULL, ",")) {
		char *endptr;
		struct integer_range range;

		errno = 0;
		if ( (range.lo_bound = strtol(p, &endptr, 10)) == 0 && (errno || endptr == p) )
			continue;
		if (*endptr == '-') {
			p = endptr+1;
			errno = 0;
			if ( (range.hi_bound = strtol(p, &endptr, 10)) == 0 && (errno || endptr == p) )
				continue;
		} else
			range.hi_bound = range.lo_bound;

		if (range.lo_bound < 0 || range.hi_bound < 0 || range.lo_bound > range.hi_bound)
			continue;
		(*num_ranges)++;
		assert(*ranges = realloc(*ranges, sizeof(**ranges)**num_ranges));
		(*ranges)[*num_ranges-1] = range;
	}
}

int main(int argc, char *argv[])
{
	int opt, retv=EXIT_SUCCESS;
	
	_g.exename = argv[0];
	_g.usage_str = "[-v] FILE...";
	while ((opt = getopt(argc, argv, "d:f:")) != -1) {
		switch (opt) {
		case 'd': _g.opt_delim = optarg; break;
		case 'f': _g.opt_fields = optarg; break;
		default:
			usage(NULL);
			exit(EXIT_FAILURE);
		}
	}

	if (!_g.opt_delim) _g.opt_delim = "\t";
	if (_g.opt_fields) str_to_ranged_list(_g.opt_fields, &_g.fields, &_g.num_fields);

	if (optind >= argc)
		retv = do_cut("-");
	else for ( ; optind < argc; optind++ )
		retv |= do_cut(argv[optind]);

	return retv;
}

