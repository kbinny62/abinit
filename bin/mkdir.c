
#define	_POSIX_SOURCE

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <getopt.h>
#include <limits.h>
#include <sys/stat.h>

static struct {
	char *exename;
	int opt_p, opt_v;
} _g;

inline static void usage(const char *msg)
{
	if (msg) fprintf(stderr, "%s: %s\n", _g.exename, msg);
	fprintf(stderr, "Usage: %s [-p] DIR...\n", _g.exename);	
}

int main(int argc, char *argv[])
{
	int opt, retv = 0;

	_g.exename = argv[0];
	while ((opt = getopt(argc, argv, "p")) != -1)
		switch (opt) {
		case 'p': _g.opt_p = 1; break;
		case 'v': _g.opt_v = 1; break;
		default:
			usage(NULL); exit(EXIT_FAILURE);
	}

	if (optind >= argc)
		usage(NULL);
	else for(; optind < argc; optind++) {
		if (_g.opt_v)
			printf("%s: %s\n", _g.exename, argv[optind]);
		if (_g.opt_p) {
			char *tok, *cumul_path=malloc(PATH_MAX+1);
			*cumul_path = '\0';
			for (tok=strtok(argv[optind], "/"); tok != NULL; tok = strtok(NULL, "/")) {
				if (PATH_MAX-strlen(cumul_path) < strlen(tok)) {
					fprintf(stderr, "%s: path name too long (>%u), ignoring rest of directory path.\n", _g.exename, PATH_MAX);
					break;
				}
				strncat(cumul_path, tok, PATH_MAX-strlen(cumul_path));
				if (mkdir(cumul_path, 0750) == -1 && errno != EEXIST) {
					fprintf(stderr, "%s: mkdir failed for '%s': %s\n", _g.exename, cumul_path, strerror(errno));
					retv |= EXIT_FAILURE;
					break;
				}
				strncat(cumul_path, "/", PATH_MAX-strlen(cumul_path));
			}
		} else if (mkdir(argv[optind], 0750) == -1) {
			fprintf(stderr, "%s: mkdir failed for '%s': %s\n", _g.exename, argv[optind], strerror(errno));
			retv |= EXIT_FAILURE;
		}
	}

	return retv;
}
