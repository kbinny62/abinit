
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


/*
 * used for key definitions including field type suffixes
 */
struct numeric_range {
	long lo_bound, lo_frac,
	     hi_bound, hi_frac;
	char lo_suffix[2], hi_suffix[2];
};

/*
 * defines a logical, sortable record which by default is a line
 */
struct record {
	char *data;		/* Raw byte representation of buffer */
};

/*
 *  Global state
 */
static struct {
	char *exename,
	     *usage_str;

	int  opt_f;             /* ignore case */
	int  opt_n;             /* numeric sort */
	int  opt_r;             /* reverse */
	char *FS;               /* opt '-t': field seperarator */

	struct record **buf;	/* lines buffer */
	size_t buf_len;
	size_t num_records;

	struct numeric_range *keys;	/* sort key range list */
	size_t num_keys;		/* # of keys in list */
} _g;


/**
 *  formats error message and append strerror(errno)
 *  @param {string} fmt - static format param
 */
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
#define	RECORDBUF_INITLEN      0x100
#define	RECORDBUF_GRAIN        0x400

/**
 *  Wrap raw data into a struct member
 *  @param {string} data - raw data pointer
 */
static struct record *mkrecord(char *data)
{
	struct record *r;
       	assert((r = malloc(sizeof(struct record))) != NULL);
	r->data = data;
	/* TODO: Compute sort key fields */
	return r;
}

/**
 *  Appends record into the global buffer, growing buffer as needed.
 *  Assumes data storage pointed to by argument will uniquely point to the
 *  record for the remainder of the program's lifetime
 *  @param param {string} line - line to append, NOT duplicated
 */
void append_record(char *data)
{
	if (!_g.buf) {
		_g.buf_len = RECORDBUF_INITLEN;
		assert(_g.buf = (struct record **)malloc( sizeof(struct record *) * _g.buf_len ));
		_g.num_records = 0;
	} else if (_g.num_records >= _g.buf_len) {
		_g.buf_len += RECORDBUF_GRAIN;
		assert(_g.buf = (struct record **)realloc( _g.buf, sizeof(struct record *) * _g.buf_len ));
	}
	_g.buf[_g.num_records++] = mkrecord(data);
}

/* type definition for sort callback */
typedef int (*sort_callback_t)(const void*, const void*);

/**
 *  Numeric sort callback
 *  @param {string} l1, l2 - lines to be compared
 */
int numsort_cb(const struct record **l1, const struct record **l2)
{
	register int retv = atol((*l1)->data) - atol((*l2)->data);
	return _g.opt_r ? -retv : retv;
}

/**
 *  Alphanumeric sort callback
 *  @param {string} l1, l2 - lines to be sorted
 */
int collsort_cb(const struct record **l1, const struct record **l2)
{
	register int retv = _g.opt_f ? strcasecmp((*l1)->data, (*l2)->data)
		: strcmp((*l1)->data, (*l2)->data);
	return _g.opt_r ? -retv : retv;
}

/**
 *  Given a filename, read out its contents into the buffer to be sorted
 *  @param {string} filename - path to a filename, or "-" for stdin
 *  @return {int} - returns EXIT_FAILURE or EXIT_SUCCESS to reflect error status or lack thereof
 */
int do_sort(const char *filename)
{
	FILE *fp = filename && strcmp(filename, "-") ? fopen(filename, "r") : stdin;
	size_t n, i;
	char *line;


	if (fp == NULL) {
		fprintf(stderr, "%s: opening '%s' failed: %s\n", _g.exename, filename, strerror(errno));
		return EXIT_FAILURE;
	} else for (line = NULL, n = _g.num_records = 0; getline(&line, &n, fp) != -1; line = NULL, n = 0) {
		if (line[strlen(line)-1] == '\n')
			line[strlen(line)-1] = '\0';
		append_record(line);
	}

	if (ferror(fp))
		fprintf(stderr, "%s: error reading '%s': %s\n", _g.exename, filename, strerror(errno));

	if (fp != stdin)
		fclose(fp);
	return EXIT_SUCCESS;
}

#define BADKEY_RET(errcode,args...) { errno = errcode; ERR(args); return; }
/**
 *  Parses a numeric range (key) definition and updates the passed in data structures
 *  @param {string} str - numeric range
 *  @param {numeric_range} ranges - pointer to range list
 *  @param {size_t} num_ranges - number of entries in list
 */
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
	int i, opt, retv=EXIT_SUCCESS;

	_g.exename = argv[0];
	_g.usage_str = "[-fnr] [-k KEYIDX[,KEYIDX] ...] [-t FLDSEP] [FILE...]";
	while ((opt = getopt(argc, argv, "fk:nrt:")) != -1) {
		switch (opt) {
			case 'f': _g.opt_f = 1; break;
			case 'n': _g.opt_n = 1; break;
			case 'r': _g.opt_r = 1; break;
			case 'k':
				  str_to_ranged_list(optarg, &_g.keys, &_g.num_keys);
				  break;
			case 't': _g.FS = optarg; break;
			default:
				  usage(NULL);
				  exit(EXIT_FAILURE);
		}
	}

	if (!_g.FS)
		_g.FS = " \t";
	if (optind >= argc)
		retv = do_sort(NULL);
	else for ( ; optind < argc; optind++)
		retv |= do_sort(argv[optind]);

	/*  Sort and print the concatentation of content of all files specified as input */
	qsort(_g.buf, _g.num_records, sizeof(*_g.buf), (sort_callback_t)(_g.opt_n ? numsort_cb: collsort_cb));
	for (i=0; i<_g.num_records; i++)
		printf("%s\n", _g.buf[i]->data);

	return retv;
}

