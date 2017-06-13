
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <ftw.h>
#include <getopt.h>
#include <sys/stat.h>
#include <unistd.h>

struct sym_mode;

static struct {
	char *exename,
	     *usage_str;
	int opt_R;

	struct sym_mode *smode;
	unsigned long nmode;
} _g;

enum { WHO_USER=0x1, WHO_GROUP=0x2, WHO_OTHER=0x4, WHO_ALL=0x7 };
enum { OP_ADD, OP_SUB, OP_EQU };
enum { PERM_r, PERM_w, PERM_x, PERM_X, PERM_s, PERM_t };

struct sym_mode {
	int who;
	int op;
	int perm;
	struct sym_mode *next;
};


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


struct sym_mode *sym_mode_parse(const char *str)
{
	return NULL;
}

mode_t sym_mode_apply(struct sym_mode *smode, mode_t mode)
{
	return mode;
}

int do_single_chmod(const char *path, const struct stat *sbuf)
{
	mode_t newmode = _g.smode ?  sym_mode_apply(_g.smode, sbuf->st_mode) : _g.nmode;
	if (chmod(path, newmode) == -1)
		return ERR("chmod '%s'", path);
	return EXIT_SUCCESS;
}

int do_ftw_chmod(const char *path, const struct stat *sbuf, int flag, struct FTW* ftwbuf)
{
	do_single_chmod(path, sbuf);
	return 0;
}

int main(int argc, char *argv[])
{
	int opt, retv=EXIT_SUCCESS;
	struct sym_mode *smode;
	unsigned long nmode;
	char *endptr;
	
	_g.exename = argv[0];
	_g.usage_str = "[-R] MODE FILE...";
	while ((opt = getopt(argc, argv, "R")) != -1) {
		switch (opt) {
		case 'R': _g.opt_R = 1; break;
		default:
			exit(usage(NULL));
		}
	}

	if (argc-optind < 2)
		exit(usage(NULL));

	if (!(_g.smode = sym_mode_parse(argv[1]))) {
		errno = 0;
		_g.nmode = strtoul(argv[1], &endptr, 8);
		if (_g.nmode == 0 && errno != 0)
			exit(usage("invalid symbolic/numeric mode specifier '%s'\n"));
	}

	for (optind++; optind < argc; optind++) {
		struct stat lsbuf;
	}

	return retv;
}

