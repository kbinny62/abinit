
#define	_POSIX_SOURCE

#include <assert.h>
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <fcntl.h>
#include <ftw.h>
#include <getopt.h>
#include <libgen.h>
#include <limits.h>
#include <sys/stat.h>
#include <unistd.h>


static struct {
	char *exename,
	     *usage_str;
	int opt_prompt,	/* -i/-f: prompt */
	    opt_p,	/* -p: duplicate file metadata */
	    opt_R,	/* -R: recursive */
	    opt_L,	/* -L: follow symlink */
	    opt_v;
	char *target;
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
	exit(EXIT_FAILURE);
}


static int path_isdir(const char *pathname)
{
	struct stat sbuf;
	return (_g.opt_L ? stat(pathname, &sbuf) : lstat(pathname, &sbuf)) != 0 && S_ISDIR(sbuf.st_mode) ? 1 : 0;
}

static int confirm_overwrite(const char *pathname)
{
	char *answer = NULL;
	size_t len = 0;
	if (!isatty(STDIN_FILENO))
		return 0;
	do {
		ssize_t nread;
		fprintf(stdout, "%s: confirm overwriting '%s'? (y/N): ", _g.exename, pathname);
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

static int do_single_cp(const char *dest_path, char *src_path)
{
	struct stat sbuf, lsbuf;
	int dest_fd, src_fd;
	ssize_t nread, nwritten;
	char buf[PATH_MAX];

	assert(dest_path && src_path);

	if (path_isdir(dest_path) || path_isdir(src_path))
		usage("unexpected directory-type argument\n");
	if (stat(dest_path, &sbuf) != -1)
		if (_g.opt_prompt == 0 || confirm_overwrite(dest_path)) {
			if (unlink(dest_path) == -1)
				return ERR("unlink '%s'", dest_path);
		} else
			return EXIT_SUCCESS;

	if (lstat(src_path, &lsbuf) != -1) {
		if (S_ISLNK(lsbuf.st_mode)) {
			nread = readlink(src_path, buf, PATH_MAX);
			if (nread != -1) {
				buf[nread] = '\0';
				if (symlink(buf, dest_path) == -1)
					return ERR("symlink '%s'", dest_path);
				return EXIT_SUCCESS;
			} else
				return ERR("readlink '%s'", src_path);
		}
	} else
		return ERR("stat '%s'", src_path);

	if ((src_fd = open(src_path, O_RDONLY)) < 0)
		return ERR("open '%s'", dest_path);
	if ((dest_fd = creat(dest_path, 0644)) < 0)
		return ERR("creat '%s'", src_path);
#if defined __linux__ /* >= 2.6.33 */
	while ((nwritten = sendfile(dest_fd, src_fd, NULL, BUFSIZ)) > 0) ;
	if (nwritten == -1)
		ERR("sendfile '%s' -> '%s'", src_path, dest_path);
#else
	{
		ssize_t retv;
		for (nwritten = 0; (nread = read(src_fd, buf, BUFSIZ)) > 0; nwritten=0 ) {
eagain:
			while ((retv = write(dest_fd, buf+nwritten, nread-nwritten)) > 0)
				nwritten += retv;
			if (retv == -1 && errno == EAGAIN)
				goto eagain;
			else if (retv == -1) {
				ERR("write '%s'", dest_path);
				break;
			}
		}
	}
#endif
	close(dest_fd);
	close(src_fd);
	return EXIT_SUCCESS;
}

static int do_ftw_cp(const char *name, const struct stat *ptr, int flag, struct FTW *ftwbuf)
{
	return 0;
}

static int do_cp(const char *dest_path, char* const* pathnamev, size_t nargs)
{
	struct stat sbuf, lsbuf;
	char buf[PATH_MAX];
	size_t i;

	assert(dest_path && pathnamev && nargs > 0);
	if (path_isdir(dest_path))
		usage("invokation requires a directory target.\n");

	for (i=0; i<nargs; i++) {
		if (stat(pathnamev[i], &sbuf) == -1)
			return ERR("stat '%s'", pathnamev[i]);
		if (lstat(pathnamev[i], &lsbuf) == -1)
			return ERR("lstat '%s'", pathnamev[i]);
		if (!(S_ISDIR(lsbuf.st_mode) || S_ISLNK(lsbuf.st_mode) && S_ISDIR(sbuf.st_mode) && _g.opt_L)) {
			snprintf(buf, sizeof buf, "%s/%s", dest_path, basename(pathnamev[i]));
			do_single_cp(buf, pathnamev[i]);
		} else if (nftw(pathnamev[i], do_ftw_cp, 16, (_g.opt_L ? 0 : FTW_PHYS) | 0) == -1)
			return ERR("ftw '%s'", pathnamev[i]);
	}

	return 0;
}

int main(int argc, char *argv[])
{
	int opt, retv=EXIT_SUCCESS;
	
	_g.exename = argv[0];
	_g.usage_str = "[-ifpRLv] SRC... TARGET";
	while ((opt = getopt(argc, argv, "ifpRLv")) != -1)
		switch (opt) {
		case 'i': _g.opt_prompt = 1; break;
		case 'f': _g.opt_prompt = 0; break;
		case 'p': _g.opt_p = 1; break;
		case 'R': _g.opt_R = 1; break;
		case 'L': _g.opt_L = 1; break;
		case 'v': _g.opt_v = 1; break;
		default:
			usage(NULL);
		}

	if (argc-optind < 2)
		usage(NULL);

	/* is last arg a directory */
	_g.target = argv[argc-1];

	if (argc-optind == 2 && !(path_isdir(argv[optind])||path_isdir(argv[optind+1])))
		retv = do_single_cp(argv[optind+1], argv[optind]);
	else
		retv = do_cp(argv[argc-1], &argv[optind], argc-optind-1);
	return retv;
}

