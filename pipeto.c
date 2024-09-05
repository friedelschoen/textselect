#include "arg.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#define USAGE "Usage: %s [-h] [-d delimiter] <inputcmd> {delimiter} <outputcmd>\n"

#define NORETURN  __attribute__((noreturn))
#define MAX(a, b) ((a) > (b) ? (a) : (b))


static char       *argv0     = NULL;
static const char *delimiter = "+";

static void die(const char *message) {
	fprintf(stderr, "error: %s: %s\n", message, strerror(errno));
	exit(EXIT_FAILURE);
}

static void help(void) {
	fprintf(stderr,
	        USAGE "Pipe output of command to another without a shell.\n"
	              "\n"
	              "Options:\n"
	              "  -h              Display this help message and exit\n"
	              "  -d delimeter    Split commands by demiliter (default: %s)\n"
	              "\n"
	              "Examples:\n"
	              "  pipeto xbps-query -l + wc -l\n"
	              "  pipeto find -name 'myfile' + xargs rm\n",
	        argv0, delimiter);
}

static void usage(int exitcode) {
	fprintf(stderr, USAGE, argv0);
	exit(exitcode);
}

static void runcommand(char **inputcmd, char **outputcmd) {
	int   pipefd[2];    // pipefd[0] is for reading, pipefd[1] is for writing
	pid_t pid1, pid2;

	if (pipe(pipefd) == -1) {
		perror("pipe");
		exit(EXIT_FAILURE);
	}

	// First child process to run inputcmd
	if ((pid1 = fork()) < 0) {
		perror("fork");
		exit(EXIT_FAILURE);
	}

	if (pid1 == 0) {
		// Child process 1: will exec inputcmd
		close(pipefd[0]);                  // Close unused read end
		dup2(pipefd[1], STDOUT_FILENO);    // Redirect stdout to pipe write end
		close(pipefd[1]);                  // Close write end after dup

		execvp(inputcmd[0], inputcmd);    // Replace child process with inputcmd
		die("unable to execute input command");
	}

	// Second child process to run outputcmd
	if ((pid2 = fork()) < 0) {
		perror("fork");
		exit(EXIT_FAILURE);
	}

	if (pid2 == 0) {
		// Child process 2: will exec outputcmd
		close(pipefd[1]);                 // Close unused write end
		dup2(pipefd[0], STDIN_FILENO);    // Redirect stdin to pipe read end
		close(pipefd[0]);                 // Close read end after dup

		execvp(outputcmd[0], outputcmd);    // Replace child process with outputcmd
		die("unable to execute output command");
	}

	// Parent process
	close(pipefd[0]);    // Close both ends of the pipe in the parent
	close(pipefd[1]);

	// Wait for both children to finish
	waitpid(pid1, NULL, 0);
	waitpid(pid2, NULL, 0);
}

int main(int argc, char *argv[]) {
	argv0 = argv[0];
	ARGBEGIN
	switch (OPT) {
		case 'h':
			help();
			exit(0);
		case 'd':
			delimiter = EARGF(usage(1));
			break;
		default:
			fprintf(stderr, "error: unknown option '-%c'\n", OPT);
			usage(1);
	}
	ARGEND;

	if (argc == 0) {
		fprintf(stderr, "error: missing command\n");
		usage(1);
	}

	char **outputcmd = NULL;

	for (int i = 0; i < argc; i++) {
		if (!strcmp(argv[i], delimiter)) {
			if (outputcmd) {
				fprintf(stderr, "error: delimiter occured more than once\n");
				exit(EXIT_FAILURE);
			}
			argv[i]   = NULL;
			outputcmd = &argv[i + 1];
		}
	}

	runcommand(argv, outputcmd);

	return 0;
}
