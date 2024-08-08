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


struct line {
	char *content;
	int   length;
	bool  selected;
};


static char *argv0           = NULL;
static bool  selected_invert = false;
static bool  keep_empty      = false;

static void die(const char *message) {
	fprintf(stderr, "error: %s: %s\n", message, strerror(errno));
	exit(EXIT_FAILURE);
}

static void help(void) {
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

static void usage(int exitcode) {
	fprintf(stderr, USAGE, argv0);
	exit(exitcode);
}

static void drawscreen(int height, int current_line, int head_line, struct line *lines, int lines_count) {
	int width = getmaxx(stdscr);

	werase(stdscr);
	for (int i = 0; i < height && i < lines_count - head_line; i++) {
		if ((head_line + i) == current_line)
			wattron(stdscr, A_REVERSE);

		if (lines[head_line + i].selected != selected_invert)
			wattron(stdscr, A_BOLD);

		if (lines[head_line + i].length > width) {
			mvwprintw(stdscr, i, 0, "%.*s...", width - 3, lines[head_line + i].content);
		} else {
			mvwprintw(stdscr, i, 0, "%s", lines[head_line + i].content);
		}

		wattroff(stdscr, A_REVERSE | A_BOLD);
	}

	wrefresh(stdscr);
}

static void handlescreen(struct line *lines, int lines_count) {
	bool quit         = false;
	int  height       = 0;
	int  current_line = 0;
	int  head_line    = 0;

	initscr();
	cbreak();
	noecho();
	keypad(stdscr, TRUE);

	height = getmaxy(stdscr);
	drawscreen(height, current_line, head_line, lines, lines_count);

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
				if (current_line < lines_count - 1) {
					(current_line)++;
					if (current_line >= head_line + height)
						head_line++;
				}
				break;
			case 'v':
				selected_invert = !selected_invert;
				break;
			case ' ':
				lines[current_line].selected = !lines[current_line].selected;
				break;
			case '\n':    // Use '\n' for ENTER key
			case 'q':
				quit = true;
		}
		drawscreen(height, current_line, head_line, lines, lines_count);
	}

	endwin();
}

static size_t loadfile(const char *filename, char **buffer, int *lines) {
	static char readbuf[READBUFFER];
	ssize_t     nread;
	size_t      alloc = 0, size = 0;
	int         fd;

	*buffer = NULL;
	*lines  = 1;

	if ((fd = open(filename, O_RDONLY)) == -1)
		die("unable to open input-file");

	while ((nread = read(fd, readbuf, sizeof(readbuf))) > 0) {
		for (ssize_t i = 0; i < nread; i++) {
			if (size == alloc) {
				if ((*buffer = realloc(*buffer, alloc += BUFFERGROW)) == NULL) {
					die("unable to allocate buffer");
				}
			}

			if (readbuf[i] == '\n') {
				(*buffer)[size++] = '\0';
				(*lines)++;
			} else {
				(*buffer)[size++] = readbuf[i];
			}
		}
	}
	(*buffer)[size++] = '\0';
	(*lines)++;
	close(fd);

	return size;
}

static int splitbuffer(char *buffer, size_t size, int maxlines, struct line **lines) {
	int count = 0, start = 0;

	(*lines) = calloc(maxlines, sizeof(struct line));
	if (*lines == NULL)
		die("unable to allocate line-mapping");

	(*lines)[count].content    = buffer;
	(*lines)[count++].selected = false;
	for (size_t i = 0; i < size; i++) {
		if (buffer[i] == '\0' && (keep_empty || buffer[i - 1] != '\0')) {
			(*lines)[count - 1].length = i - start;
			(*lines)[count].content    = &buffer[i + 1];
			(*lines)[count++].selected = false;
			start                      = i + 1;
		}
	}
	(*lines)[count - 1].length = size - start - 1;

	return count;
}

static void printselected(int fd, bool print0, struct line *lines, int lines_count) {
	for (int i = 0; i < lines_count; i++) {
		if (lines[i].selected != selected_invert && *lines[i].content != '\0') {    // is selected AND it's not empty
			write(fd, lines[i].content, lines[i].length);
			write(fd, print0 ? "" : "\n", 1);
		}
	}
}

static pid_t runcommand(char **argv, int *destfd) {
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

	*destfd = pipefd[1];
	return pid;
}

int main(int argc, char *argv[]) {
	char        *buffer, *input, *output = NULL;
	bool         print0 = false;
	int          lines_count, cmdfd;
	struct line *lines;
	size_t       buffer_size;

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

	input = argv[0];
	SHIFT;
	buffer_size = loadfile(input, &buffer, &lines_count);
	lines_count = splitbuffer(buffer, buffer_size, lines_count, &lines);

	handlescreen(lines, lines_count);

	if (output != NULL) {
		int fd;

		fd = open(output, O_WRONLY | O_TRUNC | O_CREAT, 0664);
		if (fd == -1)
			die("unable to open output-file");

		printselected(fd, print0, lines, lines_count);
	}

	if (argc == 0) {
		printselected(STDOUT_FILENO, print0, lines, lines_count);
	} else {
		pid_t pid = runcommand(argv, &cmdfd);
		printselected(cmdfd, print0, lines, lines_count);
		close(cmdfd);
		waitpid(pid, NULL, 0);
	}

	free(buffer);
	free(lines);

	return 0;
}
