
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


static struct {
	char *exename,
	     *usage_str;
	int opt_prompt,
	    opt_v;
	char *target;
} _g;


static int file_isdir(const char *pathname)
{
	struct stat sbuf;
	if (stat(pathname, &sbuf) != 0)
		return 0;
	else
		return S_ISDIR(sbuf.st_mode);
}

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
	_g.usage_str = "[-fiv] SRC... TARGET";
	while ((opt = getopt(argc, argv, "v")) != -1)
		switch (opt) {
		case 'i': _g.opt_prompt = 1; break;
		case 'f': _g.opt_prompt = 0; break;
		case 'v': _g.opt_v = 1; break;
		default:
			usage(NULL);
		}

	if (argc-optind < 2)
		usage(NULL);

	/* is last arg a directory */
	_g.target = argv[argc-1];

	if (argc-optind > 2 && !file_isdir(_g.target))
		usage("move with multiple source operands require a target directory\n");

	/* opt is # of non-option args (file operands) */
	for (opt=argc-optind ; optind < argc-1; optind++) {
		struct stat sbuf;
		char *target_path;

		if (stat(argv[optind], &sbuf) == -1) {
			fprintf(stderr, "%s: unable to stat source operand '%s': %s\n", _g.exename, argv[optind], strerror(errno));
			retv |= EXIT_FAILURE;
			continue;
		}

		target_path = opt > 2 || file_isdir(_g.target) ? malloc(PATH_MAX+1) : _g.target;
		if (target_path != _g.target)
			snprintf(target_path, PATH_MAX+1, "%s/%s", _g.target, basename(argv[optind]));
		if (_g.opt_v)
			printf("%s -> %s\n", argv[optind], target_path);
		if (rename(argv[optind], target_path) == -1) {
			fprintf(stderr, "%s: rename of %s -> %s failed: %s\n", _g.exename, argv[optind], target_path, strerror(errno));
			retv |= EXIT_FAILURE;
		}
		if (target_path != _g.target)
			free(target_path);
	}

	return retv;
}

