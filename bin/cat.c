
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>


int main(int argc, char *argv[])
{
	int fd, retv = 0;
	char buf[BUFSIZ], *fn = "-";

	if (argc == 1)
		goto no_args;
	while (--argc > 0) {
		fn = *++argv;
no_args:
		fd = strcmp(fn, "-") ? open(fn, O_RDONLY) : STDIN_FILENO;
		if (fd == -1) {
			fprintf(stderr, "%s: open '%s' failed: %s\n", argv[0], fn, strerror(errno));
			retv |= EXIT_FAILURE;
		} else {
			ssize_t nread;
			while ((nread = read(fd, buf, sizeof(buf))) > 0)
				write(STDOUT_FILENO, buf, nread);
			if (nread == -1) {
				fprintf(stderr, "%s: read from '%s' failed: %s\n", argv[0], fn, strerror(errno));
				retv |= EXIT_FAILURE;
			}
			if (fd != STDIN_FILENO)
				close(fd);
		}
	}

	return retv;
}

