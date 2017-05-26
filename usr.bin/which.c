
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <dirent.h>
#include <limits.h>
#include <sys/stat.h>

struct path_entry {
	char *pathname;
	struct path_entry *next;
} path_entries;

void path_addentry(struct path_entry *head, char *pathname)
{
	struct path_entry *p = head;
	struct stat sbuf;

	assert(head != NULL && pathname != NULL);

	/* silently drop bogus pathnames and non-directories */
	if (stat(pathname, &sbuf) != 0 || !S_ISDIR(sbuf.st_mode))
		return;
	while (p->next)
		p = p->next;
	assert(p->next = malloc(sizeof(struct path_entry)));
	assert(p->next->pathname = strdup(pathname));
	p->next->next = NULL;
}

void path_decompose(struct path_entry *head, char *env_path)
{
	char *p;

	assert(head != NULL && env_path != NULL);
	memset(head, sizeof(*head), 0);
	for (p = strtok(env_path, ":"); p; p = strtok(NULL, ":"))
		path_addentry(head, p);
}


char *path_lookup(struct path_entry *head, const char *filename)
{
	char *pathname = malloc(PATH_MAX+1);
	struct path_entry *p = head;
	struct stat sbuf;

	assert(head && filename && pathname);

	while (p = p->next) {
		snprintf(pathname, PATH_MAX+1, "%s/%s", p->pathname, filename);
		if (stat(pathname, &sbuf) == 0 && S_ISREG(sbuf.st_mode) && (sbuf.st_mode & 0111))
			return pathname;
	}
	free(pathname);
	return NULL;
}


int main(int argc, char *argv[])
{
	char *env_path = getenv("PATH");
	int retv = EXIT_SUCCESS;

	if (!env_path)
		exit(EXIT_FAILURE);
	path_decompose(&path_entries, env_path);
	while (--argc) {
		char *filename = *++argv, *which;
		if (which = path_lookup(&path_entries, filename)) {
			printf("%s\n", which);
			free(which);
		} else retv |= EXIT_FAILURE;
	}

	return retv;
}

