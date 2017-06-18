
#define	_POSIX_SOURCE

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
#include <sys/time.h>
#include <unistd.h>


struct file_list;

static struct {
	char *exename,
	     *usage_str;
	int opt_a, opt_c, opt_i, opt_l,
	    opt_r, opt_t, opt_u, opt_A,
	    opt_R, opt_F, opt_L, opt_S;
	struct file_list *listed_dirs;
} _g;


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


/**
 * the `file list' data structure + scaffolding and helpers
 * an instance of file_list is initialized by a simple memset to 0
 */
enum file_list_sorter {
	SORTER_NONE = 0,
	SORTER_COLL,
	SORTER_SIZE,
	SORTER_TIME
};

struct file_list {
	char *filename;		/* dir-relative filename */
	char *pathname;		/* ls_root-relative pathname */
	struct stat sbuf;

	struct file_list *next, *prev;
};

void file_list_add(struct file_list *head, const char *filename, const char *pathname, struct stat *sbuf)
{
	struct file_list *p = head;
	assert(head != NULL && sbuf != NULL);
	while (p->next)
		p = p->next;
	assert(p->next = malloc(sizeof(*p)));
	p->next->filename = filename ? strdup(filename) : NULL;
	p->next->pathname = pathname ? strdup(pathname) : NULL;
	p->next->sbuf = *sbuf;
	p->next->next = NULL;
	p->next->prev = p;
}

/* collating sequence sort camparison callback */
int sortfunc_coll(const struct file_list *f1, const struct file_list *f2)
{
	return strcmp(f1->filename, f2->filename);
}

/* file size sort comparison callback */
int sortfunc_size(const struct file_list *f1, const struct file_list *f2)
{
	return f1->sbuf.st_size - f2->sbuf.st_size;
}

/* [acm]time sort compare callback, return newest-first for non-reverse (-r) mode */
int sortfunc_time(const struct file_list *f1, const struct file_list *f2)
{
	struct timespec *t1 = _g.opt_c ? &f1->sbuf.st_ctim : _g.opt_u ? &f1->sbuf.st_atim : &f1->sbuf.st_mtim;
	struct timespec *t2 = _g.opt_c ? &f2->sbuf.st_ctim : _g.opt_u ? &f2->sbuf.st_atim : &f2->sbuf.st_mtim;
	if (t1->tv_sec == t2->tv_sec)
		return t2->tv_nsec - t1->tv_nsec;
	else
		return t2->tv_sec - t1->tv_sec;
}

/* Sort forall f in file_list pointed by head (starting @head->next) */
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
	case SORTER_TIME:
		sortfunc = sortfunc_time;
		break;
	default:
		sortfunc = NULL;
		return;
	}

	assert(sortfunc != NULL);	
	for (p1 = head->next; p1; p1 = p1->next) {
		for (p2 = p1->next; p2 ; p2 = p2->next) {
			register int result = _g.opt_r ? sortfunc(p2, p1) : sortfunc(p1, p2);
			if (result > 0) {
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


#if 1
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

/* return file classification char[2] for -F */
const char *file_class_char(struct stat *sbuf)
{
	register int fmt = sbuf->st_mode & S_IFMT;
	return	fmt == S_IFDIR ? "/" : fmt == S_IFLNK ? "@" : fmt == S_IFIFO ? "|" : \
		fmt == S_IFREG && sbuf->st_mode & (S_IXUSR | S_IXGRP | S_IXOTH) ? "*" \
		: "";
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

void list_file(struct file_list p[1])
{
	static char *linkbuf = NULL;
	struct stat sbuf;
	if (!linkbuf) assert(linkbuf = malloc(PATH_MAX+1));

	if (_g.opt_L)
		stat(p->pathname, &sbuf);

	if (_g.opt_i)
		printf("%-8lu ", _g.opt_L ? sbuf.st_ino : p->sbuf.st_ino);
	if (_g.opt_l) {
		struct timespec *tmspec = _g.opt_c ? &p->sbuf.st_ctim : _g.opt_u ? &p->sbuf.st_atim : &p->sbuf.st_mtim;
		if (S_ISBLK(p->sbuf.st_mode) || S_ISCHR(p->sbuf.st_mode))
			printf("%s %u %s %s %8lu %s %s", mode_to_str(p->sbuf.st_mode), (unsigned)p->sbuf.st_nlink,
			uid_to_str(p->sbuf.st_uid), gid_to_str(p->sbuf.st_gid), (unsigned)p->sbuf.st_rdev,
			time_to_str(tmspec->tv_sec), p->filename);
		else
			printf("%s %u %s %s %8lu %s %s", mode_to_str(p->sbuf.st_mode), p->sbuf.st_nlink,
			uid_to_str(p->sbuf.st_uid), gid_to_str(p->sbuf.st_gid), (unsigned long)p->sbuf.st_size,
			time_to_str(tmspec->tv_sec), p->filename);

		if (_g.opt_F && (!S_ISLNK(p->sbuf.st_mode) ||_g.opt_L))
			printf("%s", file_class_char(_g.opt_L ? &sbuf : &p->sbuf));
		if (!_g.opt_L && S_ISLNK(p->sbuf.st_mode)) {
			ssize_t len = readlink(p->pathname, linkbuf, PATH_MAX);
			if (len != -1) {
				linkbuf[len] = '\0';
				printf(" -> %s", linkbuf);
			}
		}
		putchar('\n');
	} else {
		printf("%s", p->filename);
		if (_g.opt_F)
			printf("%s", file_class_char(&p->sbuf));
		putchar('\n');
	}
}

#define	ABS(x) ((x) < 0 ? -(x) : (x))
/**
 *  the sign of nargs determines nature of paths passed in pathnamev
 *  nargs < 0: primary arguments from main
 *  nargs > 0: +1 is used to signal a recursive dir listing call
 */
int do_ls(char * const pathnamev[], ssize_t nargs)
{
	struct file_list files, *p;
	struct stat sbuf, lsbuf;
	char *pathname, is_mixed_listing=0, is_first_listing=1;
	size_t dir_count = 0, i;

	assert(pathnamev && ABS(nargs) > 0);
	memset(&files, 0, sizeof(files));
for (pathname=pathnamev[i=0]; i<ABS(nargs); pathname=pathnamev[++i]) {

	if (lstat(pathname, &lsbuf) == -1 || stat(pathname, &sbuf) == -1) {
		fprintf(stderr, "%s: stat '%s': %s\n", _g.exename, pathname, strerror(errno));
		return EXIT_FAILURE;
	}

	if (ABS(nargs) > 1 && !(is_mixed_listing || (S_ISDIR(lsbuf.st_mode) || S_ISLNK(lsbuf.st_mode) && S_ISDIR(sbuf.st_mode) && (!_g.opt_F || _g.opt_L))))
		is_mixed_listing = 1;

	/* Enumerate */
	if (nargs == 1 && (S_ISDIR(lsbuf.st_mode) || S_ISLNK(lsbuf.st_mode) && S_ISDIR(sbuf.st_mode) && _g.opt_L)) {	
		DIR *dirp;
		struct dirent *dp;
		char *pathbuf = malloc(PATH_MAX+1);
		struct stat esbuf;

		assert(pathbuf != NULL);
		files.sbuf = sbuf;
		if ((dirp = opendir(pathname)) == NULL) {
			fprintf(stderr, "%s: opendir '%s': %s\n", _g.exename, pathname, strerror(errno));
			return EXIT_FAILURE;
		} else
			file_list_add(_g.listed_dirs, NULL, NULL, &sbuf);
	
		do {
			errno = 0;
			if ((dp = readdir(dirp)) == NULL)
				continue;
			snprintf(pathbuf, PATH_MAX+1, "%s/%s", pathname, dp->d_name);
			if (lstat(pathbuf, &esbuf) == -1) {
				fprintf(stderr, "%s: lstat '%s': %s\n", _g.exename, pathbuf, strerror(errno));
				continue;
			}

			if (*dp->d_name == '.' && !(_g.opt_a || _g.opt_A))
				continue;
			if (!(strcmp(dp->d_name, ".") && strcmp(dp->d_name, "..")) && !_g.opt_a)
				continue;
			file_list_add(&files, dp->d_name, pathbuf, &esbuf);
			dir_count++;
		} while (dp != NULL);

		free(pathbuf);
		closedir(dirp);
	} else
		file_list_add(&files, pathname, pathname, &lsbuf);
} /* for(p in pathnamev) */


	/* Sort */
	if (_g.opt_S)
		file_list_sort(&files, SORTER_SIZE);
	else if (_g.opt_t)
		file_list_sort(&files, SORTER_TIME);
	else
		file_list_sort(&files, SORTER_COLL);

	/* Output */
	if (nargs == 1 && S_ISDIR(files.sbuf.st_mode) && _g.opt_l)
		/* non-standard: total of entries in directory, POSIX is number of blocks */
		printf("total %zu\n", dir_count);

	for (p = files.next; p ; p = p->next)
		if (nargs > 0 || !S_ISDIR(p->sbuf.st_mode)) {
			list_file(p);
			is_first_listing = 0;
		}

	/** Recurse if needed (-R or explicit command line args) */
	if (_g.opt_R || nargs < 0) {
		/* The _g.listed_dirs structure need not maintain pathnames nor be sorted,
		 * just stat info for matching already-listed directories by inode number */
		if (!_g.listed_dirs) {
			assert((_g.listed_dirs = malloc(sizeof(struct file_list))) != NULL);
			memset(_g.listed_dirs, 0, sizeof(*_g.listed_dirs));
		}
		for (p = files.next; p; p = p->next) {
			stat(p->pathname, &sbuf);
			if (nargs < 0 && S_ISDIR(sbuf.st_mode) || \
				  nargs == 1 && S_ISDIR(p->sbuf.st_mode) && strcmp(p->filename, ".") && strcmp(p->filename, "..") || \
				  S_ISLNK(p->sbuf.st_mode) && S_ISDIR(sbuf.st_mode) && _g.opt_L) {
				if (S_ISLNK(p->sbuf.st_mode)) {
					struct file_list *dp;
					static char *linkbuf = NULL;
					ssize_t len;
					if (!linkbuf) assert((linkbuf = malloc(PATH_MAX+1)) != NULL);
					for (dp=_g.listed_dirs->next; dp != NULL && dp->sbuf.st_ino != sbuf.st_ino; dp = dp->next)
						;
					if ((len = readlink(p->pathname, linkbuf, PATH_MAX)) != -1 && (linkbuf[len]='\0')=='\0' \
							&& (strcmp(linkbuf, ".") == 0) || dp != NULL) {
						fflush(stdout);
						fprintf(stderr, "%s: skipping redundant softlink '%s' -> %s\n",
								_g.exename, p->pathname, linkbuf);
						fflush(stderr);
						continue;
					}
				}
				if (ABS(nargs) > 1 && is_mixed_listing || !is_first_listing)
					putchar('\n');
				if (_g.opt_R || ABS(nargs) > 1)
					printf("%s:\n", p->pathname);
				do_ls(&p->pathname, 1);
				is_first_listing = 0;
			}
		}
	}

	file_list_free(&files);
	return EXIT_SUCCESS;
}

int main(int argc, char *argv[])
{
	int opt, retv=EXIT_SUCCESS;
	
	_g.exename = argv[0];
	_g.usage_str = "[-acilrtuAFRLS] FILE...";
	while ((opt = getopt(argc, argv, "acilrtuAFRLS")) != -1) {
		switch (opt) {
		case 'a': _g.opt_a = 1; break;
		case 'c': _g.opt_c = 1; _g.opt_u = 0; break;
		case 'i': _g.opt_i = 1; break;
		case 'l': _g.opt_l = 1; break;
		case 'r': _g.opt_r = 1; break;
		case 't': _g.opt_t = 1; break;
		case 'u': _g.opt_u = 1; _g.opt_c = 0; break;
		case 'A': _g.opt_A = 1; break;
		case 'F': _g.opt_F = 1; break;
		case 'R': _g.opt_R = 1; break;
		case 'L': _g.opt_L = 1; break;
		case 'S': _g.opt_S = 1; break;
		default:
			usage(NULL);
			exit(EXIT_FAILURE);
		}
	}

	if (optind >= argc) {
		char *cwd = ".";
		retv = do_ls(&cwd, -1);
	} else
		retv = do_ls(&argv[optind], -(argc-optind));
	return retv;
}
