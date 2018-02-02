#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

static const char *PS1 = NULL;
static void prompt() {
	fprintf(stdout, "%s", PS1);
}

/** Whitespace trimming and compaction */
char *cleanup_line(char *line) {
	size_t si, di, len = strlen(line);
	char *newline = (char*)malloc(len + 1);

	for (si = di = 0; si < len; si++) {
		if (isblank(line[si])) {
			if (isblank(newline[di]))
				continue;
			else
				newline[di++] = line[si];
		} else {
			newline[di++] = line[si];
		}
	}

	if (newline[di-1] == '\n' || isblank(newline[di-1]))
		di--;
	newline[di] = '\0';
	return newline;
}

void do_exec(int argc, char * const *argv, int is_detached) {
	pid_t pid;

	if ((pid = fork()) == 0) { /* child */
		if (!is_detached) {
			signal(SIGINT, SIG_DFL);
			signal(SIGQUIT, SIG_DFL);
		}
		if (execvp(argv[0], argv) == -1) {
			fprintf(stderr, "execvp(\"%s\") failed: %s\n", argv[0], strerror(errno));
			exit(EXIT_FAILURE);
		}
	} else { /* child */
		if (is_detached) {
			fprintf(stdout, "[%u] %s &\n", (unsigned)pid, argv[0]);
			return;
		} else {
			int status;
			waitpid(pid, &status, 0);
		}
	}
}

int main() {
	char *line = NULL;
	size_t n = 0;

	PS1 = getenv("PS1") ? getenv("PS1") : "$ ";

	signal(SIGINT, SIG_IGN);
	signal(SIGQUIT, SIG_IGN);

	for(prompt(), errno = 0; getline(&line, &n, stdin) != -1; prompt(), errno = 0) {
		int newargc = 1;
		char **newargv = (char**)malloc(sizeof(char*) * newargc);
		char *tok = NULL, *newline = cleanup_line(line);
		int detached_exec = 0;

		while (tok = strtok(tok ? NULL : newline, " \t")) {
			++newargc;
			assert(newargv = (char**)realloc(newargv, sizeof(char*) * newargc));
			newargv[newargc - 2] = strdup(tok);
		}

		newargv[newargc-1] = NULL;

		/* Background process? (cmd ~ /.* &$/) */
		if (newargc > 1 && !strcmp(newargv[newargc-2], "&")) {
			detached_exec = 1;
			free(newargv[--newargc]);
			newargv[newargc-1] = NULL;
		}

		if (newargc > 1) {
			do_exec(newargc, newargv, detached_exec);
		}

		/* free strdup'd argv elements and argv itself */
		while (newargc > 1) {
			free(newargv[newargc-1]);
			newargc--;
		}
		free(newargv);
		free(newline);

		/* Check for any changes with spawned detached 'jobs' */
		do {
			int status;
			pid_t pid = waitpid(-1, &status, WNOHANG);
			if (pid == -1 && errno != ECHILD) {
				fprintf(stderr, "waitpid(-1,...) failed: %s\n", strerror(errno));
			} else if (pid > 0) {
				fprintf(stdout, "[%lu]: terminated, status: %i\n", (unsigned long)pid, status);
			}
		} while(0);
	}

	if (errno != 0) {
		fprintf(stderr, "getline() failed: %s\n", strerror(errno));
		return EXIT_FAILURE;
	}

	return 0;
}
