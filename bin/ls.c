
#include <assert.h>
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <dirent.h>
#include <getopt.h>
#include <grp.h>
#include <limits.h>
#include <pwd.h>
#include <sys/stat.h>
#include <unistd.h>



static struct {
	char *exename,
	     *usage_str;
	int opt_a, opt_l, opt_r, /* r: reverse sort */
	    opt_A, opt_R, opt_F, /* R: recurse, F: no-follow+filetype */
	    opt_L;		 /* L: follow symlinks */
	char *cwd;
} _g;



/**
 * the `file list' data structure + scaffolding and helpers
 */
enum file_list_sorter {
	SORTER_NONE = 0,
	SORTER_COLL,
	SORTER_SIZE,
	SORTER_TYPE,
};

struct file_list {
	char *filename;
	char *pathname;
	struct stat sbuf;

	struct file_list *next, *prev;
};

void file_list_add(struct file_list *head, const char *filename, const char *pathname, struct stat *sbuf)
{
	struct file_list *p = head;
	assert(head != NULL && filename != NULL);
	while (p->next)
		p = p->next;
	assert(p->next = malloc(sizeof(*p)));
	p->next->filename = strdup(filename);
	p->next->pathname = strdup(pathname);
	p->next->sbuf = *sbuf;
	p->next->next = NULL;
	p->next->prev = p;
}

int sortfunc_coll(const struct file_list *f1, const struct file_list *f2)
{
	return strcmp(f1->filename, f2->filename);
}

int sortfunc_size(const struct file_list *f1, const struct file_list *f2)
{
	return f1->sbuf.st_size - f2->sbuf.st_size;
}

int sortfunc_type(const struct file_list *f1, const struct file_list *f2)
{
	return (f1->sbuf.st_mode & S_IFMT) - (f2->sbuf.st_mode & S_IFMT);
}

void file_list_sort(struct file_list *head, enum file_list_sorter sorter)
{
	struct file_list *p1, *p2;
	int (*sortfunc)(const struct file_list *, const struct file_list *);

	switch (sorter) {
	case SORTER_COLL:
		sortfunc = sortfunc_coll;
		break;
	case SORTER_SIZE:
		sortfunc = sortfunc_size;
		break;
	case SORTER_TYPE:
		sortfunc = sortfunc_type;
		break;
	default:
		sortfunc = NULL;
		return;
	}

	assert(sortfunc != NULL);	
	for (p1 = head->next; p1; p1 = p1->next) {
		for (p2 = p1->next; p2 ; p2 = p2->next) {
			register int result = sortfunc(p1, p2);
			if (result > 0 || _g.opt_r && result < 0) {
				struct file_list *r1 = p1->prev, *r2 = p2->prev;
				struct file_list *p2next = p2->next, *tmp = p1;

				r1->next = p2;
				p2->prev = r1;

				p2->next = p1->next != p2 ? p1->next : p1;
				p2->next->prev = p2;

				r2->next = r2 != p1 ? p1 : p2next;
				p1->prev = r2 != p1 ? r2 : p2;

				p1->next = p2next;
				if (p1->next)
					p1->next->prev = p1;

				p1 = p2;
				p2 = tmp;
			}
		}
	}
}


#if 0
void file_list_dump(struct file_list *head)
{
	struct file_list *p = head;
	while (head = head->next) {
		if (head->prev->next != head)
			printf("Inconsistent prev (head=%p, prev=%p) on '%s'\n", head, head->prev, head->filename);
		printf("%s [%p] (next=%p,prev=%p)%s", head->filename, head, head->next, head->prev, head->next ? "--> " : "\n");
	}
}
#endif

void file_list_free(struct file_list *head)
{
	if (head->next) {
		file_list_free(head->next);
		free(head->next);
		head->next = head->prev = NULL;
	}
	if (head->filename) free(head->filename);
	if (head->pathname) free(head->pathname);
}


char  *file_class_char(struct file_list *f)
{
	switch (f->sbuf.st_mode & S_IFMT) {
	case S_IFDIR:
		return "/";
	case S_IFLNK:
		return "@";
	case S_IFIFO:
		return "|";
	case S_IFREG:
		if (f->sbuf.st_mode & (S_IXUSR | S_IXGRP | S_IXOTH))
			return "*";
		/* fall-thru */
	default:
		return "";
	}
}

/* TODO: move up top */
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


/***
 * below is 'driver' do_ls() and supporting routines
 */

const char *uid_to_str(uid_t uid)
{
	struct passwd *pwd;
	static char buf[0x100];
	if (pwd = getpwuid(uid))
		return pwd->pw_name;
	/* else */
	snprintf(buf, sizeof(buf), "%lu", (long)uid);
	return buf;
}

const char *gid_to_str(gid_t gid)
{
	struct group *grp;
	static char buf[0x100];
	if (grp = getgrgid(gid))
		return grp->gr_name;
	/* else */
	snprintf(buf, sizeof(buf), "%lu", (long)gid);
	return buf;
}

/**
 *  str representation of a 3-component grouping of access permissions shifted to least significant position
 *  called by mode_to_str for each of the 3 access groups [UGO]
 */
const char *access_to_str(mode_t access)
{
	static char buf[4];
	buf[0] = access & (S_IRUSR | S_IRGRP | S_IROTH) ? 'r' : '-';
	buf[1] = access & (S_IWUSR | S_IWGRP | S_IWOTH) ? 'w' : '-';
	buf[2] = access & (S_IXUSR | S_IXGRP | S_IXOTH) ? 'x' : '-';
	buf[3] = '\0';
	return buf;
}

const char *mode_to_str(mode_t m)
{
	static char buf[10+1];

	switch (m & S_IFMT) {
	case S_IFDIR: *buf = 'd'; break;
	case S_IFBLK: *buf = 'b'; break;
	case S_IFCHR: *buf = 'c'; break;
	case S_IFLNK: *buf = 'l'; break;
	case S_IFIFO: *buf = 'p'; break;
	case S_IFREG: *buf = '-'; break;
#ifdef	S_IFSOCK
	case S_IFSOCK: *buf = 's'; break;
#endif
	default: *buf = '?'; break;
	}

	buf[1] = '\0';

	strcat(buf, access_to_str(m & S_IRWXU));
	strcat(buf, access_to_str(m & S_IRWXG));
	strcat(buf, access_to_str(m & S_IRWXO));
	buf[sizeof(buf)-1] = '\0';
	return buf;
}

#define	SECONDS_MONTH	(60 * 60 * 24 * 30)
const char *time_to_str(time_t t)
{
	static char buf[0x100];
	if (time(NULL) - t < SECONDS_MONTH * 6)	/* arbitrary threshold, POSIX-friendly */
		strftime(buf, sizeof(buf), "%b %e %H:%M", localtime(&t));
	else
		strftime(buf, sizeof(buf), "%b %e %Y", localtime(&t));
	return buf;
}

int do_ls(char *pathname)
{
	struct stat sbuf, lsbuf;
	struct file_list files;
	int force_follow = 0;

	if (lstat(pathname, &lsbuf) == -1 || stat(pathname, &sbuf) == -1) {
		fprintf(stderr, "%s: stat '%s': %s\n", _g.exename, pathname, strerror(errno));
		return EXIT_FAILURE;
	}

	/* initialize head */
	memset(&files, 0, sizeof(files));

	if (S_ISDIR(lsbuf.st_mode) || S_ISLNK(lsbuf.st_mode) && S_ISDIR(sbuf.st_mode) && (!_g.opt_F || _g.opt_L)) {	
		DIR *dirp;
		struct dirent *dp;
		char *pathbuf = malloc(PATH_MAX+1);
		struct stat esbuf;

		assert(pathbuf != NULL);
		if ((dirp = opendir(pathname)) == NULL) {
			fprintf(stderr, "%s: opendir '%s': %s\n", _g.exename, pathname, strerror(errno));
			return EXIT_FAILURE;
		}

		do {
			errno = 0;
			if ((dp = readdir(dirp)) != NULL) {
				snprintf(pathbuf, PATH_MAX+1, "%s/%s", pathname, dp->d_name);
				if (lstat(pathbuf, &esbuf) == -1) {
					fprintf(stderr, "%s: lstat '%s': %s\n", _g.exename, pathbuf, strerror(errno));
					continue;
				}

				if (*dp->d_name == '.' && !(_g.opt_a || _g.opt_A))
					continue;
				if (!(strcmp(dp->d_name, ".") && strcmp(dp->d_name, "..")) && !_g.opt_a)
					continue;
				if (S_ISLNK(esbuf.st_mode) && _g.opt_L)
					stat(pathbuf, &esbuf);
				file_list_add(&files, dp->d_name, pathbuf, &esbuf);
			}
		} while (dp != NULL);

		free(pathbuf);
		closedir(dirp);
	} else
		file_list_add(&files, pathname, pathname, S_ISLNK(lsbuf.st_mode) && !_g.opt_L ? &lsbuf : &sbuf);


	file_list_sort(&files, SORTER_COLL);
	/* file_list_sort(&files, SORTER_TYPE); */

	if (_g.opt_l) {
		struct file_list *p = &files;
		char *linkbuf = malloc(PATH_MAX+1);

		while (p = p->next) {
			if (p->sbuf.st_mode & (S_IFBLK | S_IFCHR))
				printf("%s %u %s %s %u %s %s", mode_to_str(p->sbuf.st_mode), p->sbuf.st_nlink,
				uid_to_str(p->sbuf.st_uid), gid_to_str(p->sbuf.st_gid), p->sbuf.st_rdev,
				time_to_str(p->sbuf.st_mtim.tv_sec), p->filename);
			else
				printf("%s %u %s %s %u %s %s", mode_to_str(p->sbuf.st_mode), p->sbuf.st_nlink,
				uid_to_str(p->sbuf.st_uid), gid_to_str(p->sbuf.st_gid), p->sbuf.st_size,
				time_to_str(p->sbuf.st_mtim.tv_sec), p->filename);

			if (_g.opt_F && (!S_ISLNK(p->sbuf.st_mode) ||_g.opt_L))
				printf("%s", file_class_char(p));
			if (!_g.opt_L && S_ISLNK(p->sbuf.st_mode)) {
				ssize_t len = readlink(p->pathname, linkbuf, PATH_MAX);
				if (len != -1) {
					linkbuf[len] = '\0';
					printf(" -> %s", linkbuf);
				}
			}
			putchar('\n');
		}
		free(linkbuf);
	} else {
		struct file_list *p = &files;
		while (p = p->next) {
			printf("%s", p->filename);
			if (_g.opt_F)
				printf("%s", file_class_char(p));
			putchar('\n');
		}
	}

	file_list_free(&files);
	return EXIT_SUCCESS;
}

int main(int argc, char *argv[])
{
	int opt, retv=EXIT_SUCCESS;
	
	_g.exename = argv[0];
	_g.usage_str = "[-alrAFRL] FILE...";
	while ((opt = getopt(argc, argv, "alrAFRL")) != -1) {
		switch (opt) {
		case 'a': _g.opt_a = 1; break;
		case 'l': _g.opt_l = 1; break;
		case 'r': _g.opt_r = 1; break;
		case 'A': _g.opt_A = 1; break;
		case 'F': _g.opt_F = 1; break;
		case 'R': _g.opt_R = 1; break;
		case 'L': _g.opt_L = 1; break;
		default:
			usage(NULL);
			exit(EXIT_FAILURE);
		}
	}

	_g.cwd = malloc(PATH_MAX+1);
	if (getcwd(_g.cwd, PATH_MAX+1) == NULL) {
		fprintf(stderr, "%s: getcwd: %s\n", _g.exename, strerror(errno));
		exit(EXIT_FAILURE);
	}

	if (optind >= argc)
		retv = do_ls(_g.cwd);
	else for ( ; optind < argc; optind++ )
		retv |= do_ls(argv[optind]);

	return retv;
}

