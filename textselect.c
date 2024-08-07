#include "arg.h"

#include <errno.h>
#include <fcntl.h>
#include <ncurses.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#define READBUFFER 1024
#define BUFFERGROW 512

#define USAGE "Usage: %s [-hnv0] [-o output] <input> [command ...]\n"

#define NORETURN __attribute__((noreturn))


static void          buffer_grow(void);
static char*         buffer_getline(size_t line);
static void          die(const char* message);
static void          drawscreen(void);
static void          handlescreen(void);
static void          help(void);
static void          loadfile(const char* filename, bool keep_empty);
static void          printselected(int fd, bool print0);
static void          runcommand(char** argv, bool print0);
static NORETURN void usage(int exitcode);

static char*  argv0           = NULL;
static char*  buffer          = NULL;
static size_t buffer_size     = 0;
static size_t buffer_alloc    = 0;
static int    buffer_lines    = 0;
static int    current_line    = 0;
static int    head_line       = 0;
static int    height          = 0;
static bool*  selected        = NULL;
static bool   selected_invert = false;

void buffer_grow(void) {
	char* newbuffer;
	if ((newbuffer = realloc(buffer, buffer_alloc += BUFFERGROW)) == NULL) {
		die("unable to allocate buffer");
	}
	buffer = newbuffer;
}

char* buffer_getline(size_t line) {
	size_t current = 0;
	if (line == 0) return buffer;
	for (size_t i = 0; i < buffer_size; i++) {
		if (buffer[i] == '\0') {
			current++;
			if (current == line)
				return &buffer[i + 1];
		}
	}
	return NULL;
}

void die(const char* message) {
	fprintf(stderr, "error: %s: %s\n", message, strerror(errno));
	exit(EXIT_FAILURE);
}

void drawscreen(void) {
	char* line;
	int width = getmaxx(stdscr);

	werase(stdscr);
	for (int i = 0; i < height && (head_line + i) < buffer_lines; i++) {
		if ((head_line + i) == current_line)
			wattron(stdscr, A_REVERSE);
		if (selected[head_line + i] != selected_invert)
			wattron(stdscr, A_BOLD);

		line = buffer_getline(head_line + i);
		if ((int) strlen(line) > width) {
			mvwprintw(stdscr, i, 0, "%.*s...",  width-3, line);
		} else {
			mvwprintw(stdscr, i, 0, "%s", line);
		}

		wattroff(stdscr, A_REVERSE | A_BOLD);
	}

	wrefresh(stdscr);
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
		height = getmaxy(stdscr);
	
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
	        USAGE
	        "Interactively select lines from a text file and optionally execute a command with the selected lines.\n"
	        "\n"
	        "Options:\n"
	        "  -h              Display this help message and exit\n"
	        "  -v              Invert the selection of lines\n"
	        "  -n              Keep empty lines which are not selectable\n"
	        "  -o output       Specify an output file to save the selected lines\n"
			"  -0              Print selected lines demilited by NUL-character\n"
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

void loadfile(const char* filename, bool keep_empty) {
	static char readbuf[READBUFFER];
	ssize_t     nread;
	int         fd;

	if ((fd = open(filename, O_RDONLY)) == -1) die("unable to open input-file");

	while ((nread = read(fd, readbuf, sizeof(readbuf))) > 0) {
		for (ssize_t i = 0; i < nread; i++) {
			if (buffer_size == buffer_alloc) buffer_grow();

			if (readbuf[i] == '\n') {
				if (keep_empty || buffer[buffer_size - 1] != '\0') {
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
	if (selected == NULL) die("unable to allocate selected-lines");
}

void printselected(int fd, bool print0) {
	size_t current = 0;
	if (selected[0] != selected_invert && buffer[0] != '\0') {    // is selected AND it's not empty
		write(fd, buffer, strlen(buffer));
		write(fd, print0 ? "" : "\n", 1);
	}
	for (size_t i = 0; i < buffer_size; i++) {
		if (buffer[i] == '\0') {
			current++;
			if (selected[current] != selected_invert && buffer[i + 1] != '\0') {    // is selected AND it's not empty
				write(fd, &buffer[i + 1], strlen(&buffer[i + 1]));
				write(fd, print0 ? "" : "\n", 1);
			}
		}
	}
}

void runcommand(char** argv, bool print0) {
	int   pipefd[2];
	pid_t pid;

	if (pipe(pipefd) == -1)
		die("unable to create pipe");

	if ((pid = fork()) == -1)
		die("unable to fork for child process");

	if (pid == 0) {                       // Child process
		close(pipefd[1]);                 // Close write end of the pipe
		dup2(pipefd[0], STDIN_FILENO);    // Redirect stdin to read end of the pipe
		execvp(argv[0], argv);
		die("unable to execute child");    // If execvp fails
	}

	close(pipefd[0]);    // Close read end of the pipe
	printselected(pipefd[1], print0);
	close(pipefd[1]);    // Close write end after writing
	wait(NULL);          // Wait for the child process to finish
}

NORETURN void usage(int exitcode) {
	fprintf(stderr, USAGE, argv0);
	exit(exitcode);
}

int main(int argc, char* argv[]) {
	char* output     = NULL;
	bool  keep_empty = false;
	bool  print0     = false;

	argv0 = argv[0];
	ARGBEGIN
	switch (OPT) {
		case 'h':
			help();
			exit(0);
		case 'v':
			selected_invert = true;
			break;
		case 'n':
			keep_empty = true;
			break;
		case 'o':
			output = EARGF(usage(1));
			break;
		case '0':    // null
			print0 = true;
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

	loadfile(argv[0], keep_empty);
	SHIFT;

	handlescreen();

	if (output != NULL) {
		int fd;

		fd = open(output, O_WRONLY | O_TRUNC | O_CREAT, 0664);
		if (fd == -1)
			die("unable to open output-file");

		printselected(fd, print0);
	}

	if (argc == 0) {
		printselected(STDOUT_FILENO, print0);
	} else {
		runcommand(argv, print0);
	}

	free(buffer);
	free(selected);

	return 0;
}
