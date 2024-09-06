#include "arg.h"

#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#define MAXCOMMANDS 8

#define USAGE \
	"Usage: %s [-hv] [-d delimiter] <command args...> {delimiter} <command args...> [{delimiter} <command args...> ...]\n"

struct command {
	char **cmdline;
	pid_t  pid;
};

static char *argv0     = NULL;
static char *delimiter = "+";
static int   verbose   = 0;

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
	              "  -d delimeter    Split commands by demiliter (default: %s)\n"
	              "  -h              Display this help message and exit\n"
	              "  -v              Always print exit-status\n"
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

static struct command *findpid(struct command *commands, int num_cmds, pid_t pid) {
	if (pid <= 0)
		return NULL;
	for (int i = 0; i < num_cmds; i++) {
		if (commands[i].pid == pid)
			return &commands[i];
	}
	return NULL;
}

static void runcommand(struct command *commands, int num_cmds) {
	int             pipefd[2];        // Pipe for communication between processes
	int             previous = -1;    // To store the read end of the previous pipe
	int             stat;
	struct command *current;

	for (int i = 0; i < num_cmds; i++) {
		current = &commands[i];

		if (pipe(pipefd) == -1) {
			die("unable to create pipe");
		}

		if ((current->pid = fork()) < 0) {
			die("unable create new process");
		}

		if (current->pid == 0) {                 // Child process
			if (previous != -1) {                // If there's a previous pipe
				dup2(previous, STDIN_FILENO);    // Redirect input from previous pipe
				close(previous);
			}
			if (i != num_cmds - 1)                 // If this is not the last command
				dup2(pipefd[1], STDOUT_FILENO);    // Redirect output to the current pipe

			close(pipefd[0]);    // Close read end of the current pipe
			close(pipefd[1]);    // Close write end of the current pipe

			execvp(*commands[i].cmdline, commands[i].cmdline);    // Execute the command
			die("unable to execute command '%s'", *commands[i].cmdline);
		}

		// Parent process
		close(pipefd[1]);    // Close write end of the current pipe
		if (previous != -1)
			close(previous);    // Close the previous pipe's read end

		if (verbose)
			fprintf(stderr, "%s: command '%s' started\n", argv0, *commands[i].cmdline);

		previous = pipefd[0];    // Save the read end of the current pipe
	}

	// Close any remaining pipe descriptors
	if (previous != -1)
		close(previous);

	// Wait for all children to finish
	for (int i = 0; i < num_cmds; i++) {
		current = findpid(commands, num_cmds, wait(&stat));
		if (!current)
			continue;

		if (WIFSIGNALED(stat))
			fprintf(stderr, "%s: command '%s' crashed: %s\n", argv0, *current->cmdline, strsignal(WTERMSIG(stat)));
		else if (WEXITSTATUS(stat))
			fprintf(stderr, "%s: command '%s' failed with exit-code %d\n", argv0, *current->cmdline, WEXITSTATUS(stat));
		else if (verbose)
			fprintf(stderr, "%s: command '%s' exited normally\n", argv0, *current->cmdline);
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
		case 'v':
			verbose = 1;
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

	struct command commands[MAXCOMMANDS];
	int            ncommands = 0;

	commands[ncommands].cmdline = argv;
	commands[ncommands++].pid   = -1;
	for (int i = 0; i < argc; i++) {
		if (!strcmp(argv[i], delimiter)) {
			argv[i] = NULL;

			commands[ncommands].cmdline = &argv[i + 1];
			commands[ncommands++].pid   = -1;
		}
	}

	runcommand(commands, ncommands);

	return 0;
}