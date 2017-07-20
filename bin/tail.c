
#define	_POSIX_SOURCE

#include <assert.h>
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
	int opt_n;
	char **linebuf;
	size_t nlines;
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

/**
 *  Append the (non-statically stored) line to the buffer
 *  shifting pointers and allocating memory when necessary */
void linebuf_add(char *line)
{
	assert(_g.linebuf != NULL);
	if (_g.nlines < _g.opt_n) {
		_g.linebuf[_g.nlines++] = line;
	} else {
		size_t i;
		free(_g.linebuf[0]);
		for (i=0; i<_g.opt_n-1; i++)
			_g.linebuf[i] = _g.linebuf[i+1];
		_g.linebuf[_g.opt_n-1] = line;
	}
}

void dump_linebuf()
{
	char **p = _g.linebuf;
	assert(_g.linebuf != NULL);
	while (p < _g.linebuf+_g.nlines)
		printf("%s", *(p++));
}

int do_tail(const char *filename)
{
	FILE *in;
	ssize_t n, nread;

	if (filename == NULL || filename[0] == '-' && filename[1] == '\0') {
		filename = "<stdin>";
		in = stdin;
	} else
		if ((in = fopen(filename, "r")) == NULL)
			return ERR("unable to open '%s' (input)", filename);

	/* do_tail reuses the linebuffer across subsequent calls
	 * and only resets the line index */
	_g.nlines = 0;

	do {
		char *line = NULL;
		if ( (nread = getline(&line, (size_t*)&n, in)) == -1 ) {
			if (ferror(in))
				ERR("error reading '%s'", filename);
		} else
			linebuf_add(line);
	} while ( !(feof(in) || ferror(in)) );

	dump_linebuf();

	if (in != stdin)
		fclose(in);
	return 0;
}

int main(int argc, char *argv[])
{
	int opt, retv=EXIT_SUCCESS;
	
	_g.exename = argv[0];
	_g.usage_str = "[-n NUMBER] FILE...";
	_g.opt_n = 10;
	while ((opt = getopt(argc, argv, "n:")) != -1) {
		errno = 0;
		switch (opt) {
		case 'n': _g.opt_n = strtol(optarg, NULL, 0);
			  if (_g.opt_n == 0 && (errno=EINVAL)==EINVAL )
				  return ERR("invalid line number specifier '%s'", optarg);
			  break;
		default:
			exit(usage(NULL));
		}
	}

	assert(_g.linebuf = calloc(_g.opt_n, sizeof(char*)));
	_g.nlines = 0;

	if (argc-optind > 0)
		while (optind < argc)
			retv |= do_tail(argv[optind++]);
	else
		retv = do_tail("-");

	return retv;
}

