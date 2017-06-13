
#define	_POSIX_SOURCE

#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <ftw.h>
#include <getopt.h>
#include <sys/stat.h>
#include <unistd.h>


static struct {
	char *exename,
	     *usage_str;
	int opt_prompt,		/* force/prompt */
	    opt_r, opt_R,		/* recursive remove */
	    opt_v;
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

static inline int confirm_remove(const char *pathname)
{
	char *answer;
	size_t len;
	if (!isatty(STDIN_FILENO))
		return 0;
	do {
		ssize_t nread;
		fprintf(stdout, "%s: confirm removing '%s'? (y/N): ", _g.exename, pathname);
		fflush(stdout);
		if ((nread = getline(&answer, &len, stdin)) == -1 || nread < 2)
			return 0;
		if (nread > 2 || nread == 2 && answer[1] != '\n')
			continue;
		else if (*answer == 'y' || *answer == 'Y')
			return 1;
		else if (*answer == 'n' || *answer == 'N')
			return 0;
	} while(1);
}

int unlink_path(const char *pathname, const struct stat *sbuf)
{
	if (S_ISDIR(sbuf->st_mode)) {
		if (_g.opt_v)
			printf("%s: removing directory %s\n", _g.exename, pathname);
		if ((!_g.opt_prompt || _g.opt_prompt && confirm_remove(pathname)) && rmdir(pathname) == -1)
			return _g.opt_prompt ? ERR("unlink '%s'", pathname): EXIT_SUCCESS;
	} else {
		if (_g.opt_v)
			printf("%s: removing '%s'\n", _g.exename, pathname);
		if ((!_g.opt_prompt || _g.opt_prompt && confirm_remove(pathname)) && unlink(pathname) == -1)
			return _g.opt_prompt ? ERR("unlink '%s'", pathname) : EXIT_SUCCESS;
	}

	return EXIT_SUCCESS;
}

int do_ftw_rm(const char *fname, const struct stat *sbuf, int flag, struct FTW *ftwbuf)
{
	unlink_path(fname, sbuf);
	return 0;
}

int do_rm(char *const *pathnamev, size_t nargs)
{
	struct stat lsbuf;
	int retv = 0;
	size_t i;

	for (i=0; i < nargs; i++) {
		if (lstat(pathnamev[i], &lsbuf) == -1 && _g.opt_prompt)
			retv |= ERR("stat '%s'", pathnamev[i]);
		else if (!S_ISDIR(lsbuf.st_mode))
			retv |= unlink_path(pathnamev[i], &lsbuf);
		else
			retv |= nftw(pathnamev[i], do_ftw_rm, 16, FTW_DEPTH | FTW_PHYS);
	}
}

int main(int argc, char *argv[])
{
	int opt, retv=EXIT_SUCCESS;
	
	_g.exename = argv[0];
	_g.usage_str = "[-if(Rr)v] FILE...";
	_g.opt_prompt = isatty(STDIN_FILENO);
	while ((opt = getopt(argc, argv, "ifvRr")) != -1) {
		switch (opt) {
		case 'i': _g.opt_prompt = 1; break;
		case 'f': _g.opt_prompt = 0; break;
		case 'r': case 'R': _g.opt_r = 1; break;
		case 'v': _g.opt_v = 1; break;
		default:
			exit(usage(NULL));
		}
	}

	if (argc-optind < 1)
		return usage(NULL);
	else
		retv |= do_rm(argv+optind, argc-optind);

	return retv;
}

