#include "arg.h"

#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#define MAXCOMMANDS 8

#define USAGE "Usage: %s [-h] [-d delimiter] <command args...> {delimiter} <command args...> [{delimiter} <command args...> ...]\n"

static char *argv0     = NULL;
static char *delimiter = "+";

static void die(const char *message, ...) {
	va_list va;
	va_start(va, message);
	
	fprintf(stderr, "%s: ", argv0);
	vfprintf(stderr, message, va);
	fprintf(stderr, ": %s\n", strerror(errno));

	va_end(va);
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


static void runcommand(char ***commands, int num_cmds) {
	int   pipefd[2];       // Pipe for communication between processes
	int   prev_fd = -1;    // To store the read end of the previous pipe
	pid_t pid;

	for (int i = 0; i < num_cmds; i++) {
		if (pipe(pipefd) == -1) {
			die("unable to create pipe");
		}

		if ((pid = fork()) < 0) {
			die("unable create new process");
		}

		if (pid == 0) {                         // Child process
			if (prev_fd != -1) {                // If there's a previous pipe
				dup2(prev_fd, STDIN_FILENO);    // Redirect input from previous pipe
				close(prev_fd);
			}
			if (i != num_cmds - 1) {               // If this is not the last command
				dup2(pipefd[1], STDOUT_FILENO);    // Redirect output to the current pipe
			}
			close(pipefd[0]);    // Close read end of the current pipe
			close(pipefd[1]);    // Close write end of the current pipe

			execvp(*commands[i], commands[i]);    // Execute the command
			die("unable to execute command %s", *commands[i]);
		}

		// Parent process
		close(pipefd[1]);    // Close write end of the current pipe
		if (prev_fd != -1) {
			close(prev_fd);    // Close the previous pipe's read end
		}
		prev_fd = pipefd[0];    // Save the read end of the current pipe
	}

	// Close any remaining pipe descriptors
	if (prev_fd != -1) {
		close(prev_fd);
	}

	// Wait for all children to finish
	for (int i = 0; i < num_cmds; i++) {
		wait(NULL);
	}
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

	char **commands[MAXCOMMANDS];
	int    ncommands      = 0;
	commands[ncommands++] = argv;
	for (int i = 0; i < argc; i++) {
		if (!strcmp(argv[i], delimiter)) {
			argv[i] = NULL;

			commands[ncommands++] = &argv[i + 1];    // Start of the next command
		}
	}

	runcommand(commands, ncommands);

	return 0;
}