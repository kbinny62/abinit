
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <getopt.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>


static struct {
	char *exename,
	     *usage_str;
	int opt_i;
} _g;

extern char **environ;


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

int main(int argc, char *argv[])
{
	int opt, retv=EXIT_SUCCESS;
	
	_g.exename = argv[0];
	_g.usage_str = "[-i] [-u VARNAME] [VARNAME=VALUE]... [COMMAND [ARGS]]";
	while ((opt = getopt(argc, argv, "iu:")) != -1) {
		switch (opt) {
		case 'i': _g.opt_i = 1; break;
		case 'u':
			if (unsetenv(optarg) == -1)
				fprintf(stderr, "%s: unsetenv failed: %s\n", _g.exename, strerror(errno));
			break;
		default:
			usage(NULL);
			exit(EXIT_FAILURE);
		}
	}

	if (_g.opt_i)
		*environ = NULL;
	
	for (; optind < argc; optind++) {
		if (strchr(argv[optind], '=') && putenv(argv[optind]) == -1)
			fprintf(stderr, "%s: putenv(\"%s\"): %s\n", _g.exename, argv[optind], strerror(errno));
		else if (!strchr(argv[optind], '='))
			/* command follows */
			break;
	}

	if (optind >= argc) {
		char **p;
		for (p = environ; *p; p++)
			puts(*p);
	} else {
		pid_t pid;
		if ((pid = fork()) == -1) {
			fprintf(stderr, "%s: fork failed: %s\n", _g.exename, strerror(errno));
			exit(EXIT_FAILURE);
		} else if (pid == 0) {
			char **newargv = argv+optind;
			if (execvp(*newargv, newargv) == -1)
				fprintf(stderr, "%s: execvp failed: %s\n", _g.exename, strerror(errno));
			exit(EXIT_FAILURE);
		} else {
			int status;
			int wait_flags = WUNTRACED;
#ifdef	WCONTINUED
			wait_flags |= WCONTINUED;
#endif
			do {
				if (waitpid(pid, &status, wait_flags) != pid) {
					fprintf(stderr, "%s: waitpid failed: %s\n", _g.exename, strerror(errno));
					exit(EXIT_FAILURE);
				} else if (WIFEXITED(status))
					retv = WEXITSTATUS(status);
				else if (WIFSIGNALED(status))
					retv = -1;
				else /* stopped, continued */
					continue;
			} while (!WIFEXITED(status) && !WIFSTOPPED(status));
		}
	}

	return retv;
}

