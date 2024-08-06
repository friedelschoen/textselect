#include "arg.h"

#include <fcntl.h>
#include <ncurses.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#define READBUFFER 512
#define BUFFERGROW 512

#define NORETURN __attribute__((noreturn))


void          buffer_grow(void);
char*         buffer_getline(size_t line);
void          die(const char* message);
void          drawscreen(void);
void          execute(char** argv);
void          handlescreen(void);
void          help(void);
void          loadfile(const char* filename);
void          printselected(int fd);
void          runcommand_pipe(char** argv);
void          runcommand_xargs(int argc, char** argv);
void          runcommand_xlines(int argc, char** argv);
void          runcommand_xreplace(int argc, char** argv);
NORETURN void usage(int exitcode);

char*  argv0           = NULL;
char*  buffer          = NULL;
size_t buffer_size     = 0;
size_t buffer_alloc    = 0;
int    buffer_lines    = 0;
int    current_line    = 0;
int    head_line       = 0;
int    height          = 0;
bool*  selected        = NULL;
bool   selected_invert = false;

void buffer_grow(void) {
	char* newbuffer;
	if ((newbuffer = realloc(buffer, buffer_alloc += BUFFERGROW)) == NULL) {
		die("allocating buffer");
	}
	buffer = newbuffer;
}

char* buffer_getline(size_t line) {
	size_t current = 0;
	if (line == 0) return buffer;
	for (size_t i = 0; i < buffer_size; i++) {
		if (buffer[i] == '\0') {
			current++;
			if (current == line) return &buffer[i + 1];
		}
	}
	return NULL;
}

void die(const char* message) {
	perror(message);
	exit(EXIT_FAILURE);
}

void drawscreen(void) {
	werase(stdscr);
	for (int i = 0; i < height && (head_line + i) < buffer_lines; i++) {
		if ((head_line + i) == current_line) wattron(stdscr, A_REVERSE);
		if (selected[head_line + i] != selected_invert) wattron(stdscr, A_BOLD);
		mvwprintw(stdscr, i, 0, "%s", buffer_getline(head_line + i));
		wattroff(stdscr, A_REVERSE | A_BOLD);
	}
	wrefresh(stdscr);
}

void execute(char** argv) {
	pid_t pid;

	if ((pid = fork()) == -1)
		die("fork");

	if (pid == 0) {
		execvp(*argv, argv);
		die("execvp");
	}
	wait(NULL);
}

void handlescreen(void) {
	bool quit = false;

	initscr();
	cbreak();
	noecho();
	keypad(stdscr, TRUE);

	height = getmaxy(stdscr);

	drawscreen();

	while (!quit) {
		switch (getch()) {
			case KEY_UP:
			case KEY_LEFT:
				if (current_line > 0) {
					(current_line)--;
					if (current_line < head_line)
						head_line--;
				}
				break;
			case KEY_DOWN:
			case KEY_RIGHT:
				if (current_line < buffer_lines - 1) {
					(current_line)++;
					if (current_line >= head_line + height)
						head_line++;
				}
				break;
			case 'v':
				selected_invert = !selected_invert;
				break;
			case ' ':
				selected[current_line] = !selected[current_line];
				break;
			case '\n':    // Use '\n' for ENTER key
			case 'q':
				quit = true;
		}
		drawscreen();
	}

	endwin();
}

void help(void) {
	fprintf(stderr,
	        "Usage: %s [-hvx] [-o output] <input> [command [args...]]\n"
	        "Interactively select lines from a text file and optionally execute a command with the selected lines.\n"
	        "\n"
	        "Options:\n"
	        "  -h              Display this help message and exit\n"
	        "  -v              Invert the selection of lines\n"
	        "  -x              Call command with selected lines as argument\n"
	        "  -o output       Specify an output file to save the selected lines\n"
	        "\n"
	        "Navigation and selection keys:\n"
	        "  UP, LEFT        Move the cursor up\n"
	        "  DOWN, RIGHT     Move the cursor down\n"
	        "  v               Invert the selection of lines\n"
	        "  SPACE           Select or deselect the current line\n"
	        "  ENTER, q        Quit the selection interface\n"
	        "\n"
	        "Examples:\n"
	        "  textselect -o output.txt input.txt\n"
	        "  textselect input.txt sort\n",
	        argv0);
}

void loadfile(const char* filename) {
	static char readbuf[READBUFFER];
	ssize_t     nread;
	int         fd;

	if ((fd = open(filename, O_RDONLY)) == -1) die("Failed to open file");

	while ((nread = read(fd, readbuf, sizeof(readbuf))) > 0) {
		for (ssize_t i = 0; i < nread; i++) {
			if (buffer_size == buffer_alloc) buffer_grow();

			if (readbuf[i] == '\n') {
				if (buffer[buffer_size - 1] != '\0') {
					buffer[buffer_size++] = '\0';
					buffer_lines++;
				}
			} else {
				buffer[buffer_size++] = readbuf[i];
			}
		}
	}

	buffer[buffer_size++] = '\0';
	if (fd > 2) close(fd);

	selected = calloc(buffer_lines, sizeof(bool));
	if (selected == NULL) die("allocating selected");
}

void printselected(int fd) {
	size_t current = 0;
	if (selected[0] != selected_invert)
		dprintf(fd, "%s\n", buffer);

	for (size_t i = 0; i < buffer_size; i++) {
		if (buffer[i] == '\0') {
			current++;
			if (selected[current] != selected_invert)
				dprintf(fd, "%s\n", &buffer[i + 1]);
		}
	}
}

void runcommand_pipe(char** argv) {
	int   pipefd[2];
	pid_t pid;

	if (pipe(pipefd) == -1) die("pipe");

	if ((pid = fork()) == -1) die("fork");

	if (pid == 0) {                       // Child process
		close(pipefd[1]);                 // Close write end of the pipe
		dup2(pipefd[0], STDIN_FILENO);    // Redirect stdin to read end of the pipe
		execvp(argv[0], argv);
		die("execvp");       // If execvp fails
	} else {                 // Parent process
		close(pipefd[0]);    // Close read end of the pipe
		printselected(pipefd[1]);
		close(pipefd[1]);    // Close write end after writing
		wait(NULL);          // Wait for the child process to finish
	}
}

void runcommand_xargs(int argc, char** argv) {
	char** newargv;
	int    chosencount = argc + 1;    // + NULL
	for (int i = 0; i < buffer_lines; i++)
		if (selected[i] != selected_invert)
			chosencount++;

	newargv = alloca(chosencount * sizeof(char*));
	int newargc;
	for (newargc = 0; newargc < argc; newargc++)
		newargv[newargc] = argv[newargc];

	size_t current = 0;
	if (selected[0] != selected_invert)
		newargv[newargc++] = buffer;

	for (size_t i = 0; i < buffer_size; i++) {
		if (buffer[i] == '\0') {
			current++;
			if (selected[current] != selected_invert)
				newargv[newargc++] = &buffer[i + 1];
		}
	}

	newargv[newargc++] = NULL;

	execute(newargv);
}

void runcommand_xlines(int argc, char** argv) {
	char** newargv;
	newargv = alloca((argc + 2) * sizeof(char*));

	for (int i = 0; i < argc; i++)
		newargv[i] = argv[i];

	newargv[argc + 1] = NULL;

	size_t current = 0;
	if (selected[0] != selected_invert) {
		newargv[argc] = buffer;
		execute(newargv);
	}
	for (size_t i = 0; i < buffer_size; i++) {
		if (buffer[i] == '\0') {
			current++;
			if (selected[current] != selected_invert) {
				newargv[argc] = &buffer[i + 1];
				execute(newargv);
			}
		}
	}
}

void runcommand_xreplace(int argc, char** argv) {
	char** newargv;
	newargv       = alloca((argc + 1) * sizeof(char*));
	newargv[argc] = NULL;

	size_t current = 0;
	if (selected[0] != selected_invert) {
		for (int i = 0; i < argc; i++)
			newargv[i] = !strcmp(argv[i], "{}") ? buffer : argv[i];
		execute(newargv);
	}
	for (size_t i = 0; i < buffer_size; i++) {
		if (buffer[i] == '\0') {
			current++;
			if (selected[current] != selected_invert) {
				for (int j = 0; j < argc; j++)
					newargv[j] = !strcmp(argv[j], "{}") ? &buffer[i + 1] : argv[j];
				execute(newargv);
			}
		}
	}
}

NORETURN void usage(int exitcode) {
	fprintf(stderr, "Usage: %s [-hvx] [-o output] <input> [command ...]\n", argv0);
	exit(exitcode);
}

int main(int argc, char* argv[]) {
	char* output   = NULL;
	bool  xargs    = false;
	bool  xlines   = false;
	bool  xreplace = false;

	argv0 = argv[0];
	ARGBEGIN
	switch (OPT) {
		case 'h':
			help();
			exit(0);
		case 'v':
			selected_invert = true;
			break;
		case 'x':
			if (xreplace || xlines) {
				fprintf(stderr, "error: -x is mutually exclusive with -i and -l\n");
				exit(EXIT_FAILURE);
			}
			xargs = true;
			break;
		case 'i':
			if (xargs || xlines) {
				fprintf(stderr, "error: -i is mutually exclusive with -x and -l\n");
				exit(EXIT_FAILURE);
			}
			xreplace = true;
			break;
		case 'l':
			if (xreplace || xargs) {
				fprintf(stderr, "error: -l is mutually exclusive with -i and -x\n");
				exit(EXIT_FAILURE);
			}
			xlines = true;
			break;
		case 'o':
			output = EARGF(usage(1));
			break;
		default:
			fprintf(stderr, "error: unknown option '-%c'\n", OPT);
			usage(1);
	}
	ARGEND;

	if (argc == 0) {
		fprintf(stderr, "error: missing input\n");
		usage(1);
	}

	loadfile(argv[0]);
	SHIFT;

	handlescreen();

	if (output != NULL) {
		int fd;

		fd = open(output, O_WRONLY | O_TRUNC | O_CREAT, 0664);
		if (fd == -1) die("Failed to open file");

		printselected(fd);
	}

	if (argc == 0) {
		printselected(STDOUT_FILENO);
	} else if (xargs) {
		runcommand_xargs(argc, argv);
	} else if (xlines) {
		runcommand_xlines(argc, argv);
	} else if (xreplace) {
		runcommand_xreplace(argc, argv);
	} else {
		runcommand_pipe(argv);
	}

	free(buffer);
	free(selected);

	return 0;
}
