#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <getopt.h>
#include <libgen.h>
#include <sys/stat.h>
#include <unistd.h>

struct {
	int opt_s, opt_f;
	int opt_v;
	int dest_isdir;
	const char *exename;
	const char *dest;
} _g;

void usage(const char *msg)
{
	if (msg)
		fprintf(stderr, "%s: %s\n", _g.exename, msg);
	fprintf(stderr, "Usage: %s [-f] [-s]\tTARGET    LINKNAME\n"
			"\t\t\tTARGETS.. DESTDIR\n",
			_g.exename, _g.exename);
}

int main(int argc, char *argv[])
{
	int opt, retv;
	struct stat sbuf;

	_g.exename = argv[0];
	while ((opt = getopt(argc, argv, "fsv")) != -1)
		switch (opt) {
		case 's':
			_g.opt_s = 1;
			break;
		case 'f':
			_g.opt_f = 1;
			break;
		case 'v':
			_g.opt_v = 1;
			break;
		default:
			usage(NULL);
			return EXIT_FAILURE;
		}

	if (argc - optind < 2) {
		usage(NULL);
		return EXIT_FAILURE;
	}

	_g.dest = argv[argc-1];
	if (lstat(argv[argc-1], &sbuf) != -1 && S_ISDIR(sbuf.st_mode))
		_g.dest_isdir = 1;

	if (argc-optind > 2 && !_g.dest_isdir) {
		usage(NULL);
		return EXIT_FAILURE;
	}


	while (optind < argc-1) {
		char *linkpath;
		if (_g.dest_isdir) {
			linkpath = (char*)malloc(strlen(_g.dest) + sizeof('/')
				+ strlen(basename(argv[optind])) + sizeof('\0'));
			sprintf(linkpath, "%s/%s", _g.dest, basename(argv[optind]));
		} else linkpath = _g.dest;

		if (_g.opt_v)
			fprintf(stdout, "%s -> %s\n", argv[optind], linkpath);
			
		retv = _g.opt_s ? symlink(argv[optind], linkpath) :
					link(argv[optind], linkpath);
		if (retv == -1 && errno == EEXIST && _g.opt_f) {
			if (unlink(linkpath) == -1) {
				fprintf(stderr, "%s: unable to unlink '%s': %s\n",
						_g.exename, linkpath, strerror(errno));
				return EXIT_FAILURE;
			}
			retv = _g.opt_s ? symlink(argv[optind], linkpath) :
					link(argv[optind], linkpath);
		}
		if (retv == -1 && !_g.opt_f) {
			fprintf(stderr, "%s: error linking %s -> %s: %s\n", _g.exename,
				argv[optind], linkpath, strerror(errno));
			return EXIT_FAILURE;
		}

		if (_g.dest_isdir)
			free(linkpath);
		optind++;
	}

	return EXIT_SUCCESS;
}

