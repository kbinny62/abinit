
#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <getopt.h>
#include <sys/stat.h>
#include <unistd.h>


/* used for key definitions including field type suffixes */
struct numeric_range {
	long lo_bound, lo_frac,
	     hi_bound, hi_frac;
	char lo_suffix[2], hi_suffix[2];
};

static struct {
	char *exename,
	     *usage_str;

	int  opt_n;	/* numeric sort */
	int  opt_r;	/* reverse */
	char *opt_t;	/* field seperarator */

	char **buf;
	size_t buf_len;
	size_t num_lines;

	struct numeric_range *keys;
	size_t num_keys;
} _g;


/**
 *  formats error message and append strerror(errno) */
static void ERR(const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	fprintf(stderr, "%s: ", _g.exename);
	vfprintf(stderr, fmt, ap);
	fprintf(stderr, ": %s\n", strerror(errno));
	va_end(ap);
}

static void usage(char *msg, ...)
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


/* initial buffer length, and subsequent growth increments, in lines */
#define	LINES_INITLEN	0x100
#define	LINES_GRAIN	0x400

void append_line(char *line)
{
	if (!_g.buf) {
		_g.buf_len = LINES_INITLEN;
		assert(_g.buf = (char**)malloc( sizeof(char*) * _g.buf_len ));
		_g.num_lines = 0;
	} else if (_g.num_lines >= _g.buf_len) {
		_g.buf_len += LINES_GRAIN;
		assert(_g.buf = (char**)realloc( _g.buf, sizeof(char*) * _g.buf_len ));
	}
	_g.buf[_g.num_lines++] = line;
}

int num_sorter(const char **l1, const char **l2)
{
	register int retv = atol(*l1) - atol(*l2);
	return _g.opt_r ? -retv : retv;
}

int coll_sorter(const char **l1, const char **l2)
{
	register int retv = strcmp(*l1, *l2);
	return _g.opt_r ? -retv : retv;
}

int do_sort(const char *filename)
{
	FILE *fp = filename && strcmp(filename, "-") ? fopen(filename, "r") : stdin;
	ssize_t n, i;
	char *line;


	if (fp == NULL) {
		fprintf(stderr, "%s: opening '%s' failed: %s\n", _g.exename, filename, strerror(errno));
		return EXIT_FAILURE;
	} else for (line = NULL, n = _g.num_lines = 0; getline(&line, &n, fp) != -1; line = NULL, n = 0) {
		if (line[strlen(line)-1] == '\n')
			line[strlen(line)-1] = '\0';
		append_line(line);
	}

	if (ferror(fp))
		fprintf(stderr, "%s: error reading '%s': %s\n", _g.exename, filename, strerror(errno));
		
	qsort(_g.buf, _g.num_lines, sizeof(*_g.buf), _g.opt_n ? num_sorter: coll_sorter);
	for (i=0; i<_g.num_lines; i++)
		printf("%s\n", _g.buf[i]);
	
	if (line) free(line);
	if (fp != stdin)
		fclose(fp);
	return EXIT_SUCCESS;
}

#define BADKEY_RET(errcode,args...) { errno = errcode; ERR(args); return; }
void str_to_ranged_list(char *str, struct numeric_range **ranges, size_t *num_ranges)
{
	char *endptr, *p;
	struct numeric_range range;
	
	assert(str != NULL && ranges && num_ranges != NULL);
	memset(&range, 0, sizeof(range));

	errno = 0;
	if ( (range.lo_bound = strtol(str, &endptr, 0)) <= 0 || errno || endptr == str )
		BADKEY_RET(errno ? errno : (range.lo_bound <= 0 ? EDOM : EINVAL),
				"malformed range '%s' (lower bound)", str);
	if (*endptr == '.') {
		p = endptr + 1;
		errno = 0;
		if ( (range.lo_frac = strtol(p, &endptr, 0)) == 0 && (errno || endptr == p) || range.lo_frac < 0)
			BADKEY_RET(errno ? errno : (range.lo_frac < 0 ? EDOM : EINVAL),
				"malformed range '%s' (lower fractional)", str);
	}
	if (strchr("bdfinr", *endptr) != NULL)
		*range.lo_suffix = *endptr++;
	else if (*endptr != ',' && *endptr != '\0' && !isblank(*endptr))
		BADKEY_RET(EINVAL, "malformed range '%s' (unrecognized suffix '%c')", str, *endptr);

	while (isblank(*endptr))
		endptr++;
	if (*endptr == ',') {
		p = endptr + 1;
		errno = 0;
		if ( (range.hi_bound = strtol(p, &endptr, 0)) <= 0 || errno || endptr == p )
			BADKEY_RET(errno ? errno : (range.hi_bound <= 0 ? EDOM: EINVAL),
				"malformed range '%s' (higher bound)", str);
		if (*endptr == '.') {
			p = endptr + 1;
			errno = 0;
			if ( (range.hi_frac = strtol(p, &endptr, 0)) == 0 && (errno || endptr == p) || range.hi_frac < 0)
				BADKEY_RET(errno ? errno : (range.hi_frac < 0 ? EDOM: EINVAL),
					"malformed range '%s' (higher fractional)", str);
		}
		if (strchr("bdfinr", *endptr) != NULL)
			*range.hi_suffix = *endptr++;
		else if (*endptr != '\0' && !isblank(*endptr))
			BADKEY_RET(EINVAL, "malformed range '%s' (unrecognized suffix '%c')", str, *endptr);
	} else
		range.hi_bound = -1;
	
	assert( *ranges = realloc(*ranges, sizeof(**ranges) * ++(*num_ranges)) );
	(*ranges)[*num_ranges-1] = range;
}


int main(int argc, char *argv[])
{
	int opt, retv=EXIT_SUCCESS;
	
	_g.exename = argv[0];
	_g.usage_str = "[-nr] [-k KEYIDX[,KEYIDX]] [-t FLDSEP] FILE...";
	while ((opt = getopt(argc, argv, "k:nrt:")) != -1) {
		switch (opt) {
		case 'n': _g.opt_n = 1; break;
		case 'r': _g.opt_r = 1; break;
		case 'k':
			str_to_ranged_list(optarg, &_g.keys, &_g.num_keys);
			break;
		case 't': _g.opt_t = optarg; break;
		default:
			usage(NULL);
			exit(EXIT_FAILURE);
		}
	}

	if (!_g.opt_t)
		_g.opt_t = " \t";
	if (optind >= argc)
		retv = do_sort(NULL);
	else for ( ; optind < argc; optind++)
		retv |= do_sort(argv[optind]);

	return retv;
}

