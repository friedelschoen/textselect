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

char*  buffer        = NULL;
size_t buffer_ptr    = 0;
size_t buffer_alloc  = 0;
int    line_count    = 0;
bool*  chosen_lines  = NULL;
bool   invert_chosen = false;
char*  argv0;

/**
 * @brief Handles error messages and exits the program.
 *
 * @param message Error message to print.
 */
void die(const char* message) {
	perror(message);
	exit(EXIT_FAILURE);
}

/**
 * @brief Allocates memory for the buffer, growing it by BUFFERGROW bytes.
 */
void grow_buffer(void) {
	char* new_buf;
	if ((new_buf = realloc(buffer, buffer_alloc += BUFFERGROW)) == NULL) {
		die("allocating buffer");
	}
	buffer = new_buf;
}

/**
 * @brief Loads a file into memory, storing its lines in the buffer.
 *
 * @param filename Name of the file to load, or NULL to read from stdin.
 */
void load_file(const char* filename) {
	static char readbuf[READBUFFER];
	ssize_t     nread;
	int         fd;

	fd = (filename == NULL) ? STDIN_FILENO : open(filename, O_RDONLY);
	if (fd == -1) die("Failed to open file");

	while ((nread = read(fd, readbuf, sizeof(readbuf))) > 0) {
		for (ssize_t i = 0; i < nread; i++) {
			if (buffer_ptr == buffer_alloc) grow_buffer();

			if (readbuf[i] == '\n') {
				if (buffer[buffer_ptr - 1] != '\0') {
					buffer[buffer_ptr++] = '\0';
					line_count++;
				}
			} else {
				buffer[buffer_ptr++] = readbuf[i];
			}
		}
	}

	buffer[buffer_ptr++] = '\0';
	if (fd > 2) close(fd);

	chosen_lines = calloc(line_count, sizeof(bool));
	if (chosen_lines == NULL) die("allocating chosen_lines");
}

/**
 * @brief Retrieves a line from the buffer.
 *
 * @param line Line number to retrieve.
 * @return Pointer to the start of the line.
 */
char* get_line(size_t line) {
	size_t current = 0;
	if (line == 0) return buffer;
	for (size_t i = 0; i < buffer_ptr; i++) {
		if (buffer[i] == '\0') {
			current++;
			if (current == line) return &buffer[i + 1];
		}
	}
	return NULL;
}

/**
 * @brief Prints lines to the ncurses window.
 *
 * @param win ncurses window to print to.
 * @param current_line Line currently selected.
 * @param start_line Line to start printing from.
 * @param max_y Maximum number of lines to print.
 */
void draw_lines(WINDOW* win, int current_line, int start_line, int max_y) {
	werase(win);
	for (int i = 0; i < max_y && (start_line + i) < line_count; i++) {
		if ((start_line + i) == current_line) wattron(win, A_REVERSE);
		if (chosen_lines[start_line + i] != invert_chosen) wattron(win, A_BOLD);
		mvwprintw(win, i, 0, "%s", get_line(start_line + i));
		wattroff(win, A_REVERSE | A_BOLD);
	}
	wrefresh(win);
}

/**
 * @brief Prints the chosen lines to the specified file descriptor.
 *
 * @param fd File descriptor to print to.
 */
void output_chosen_lines_fd(int fd) {
	size_t current = 0;
	if (chosen_lines[0] != invert_chosen)
		dprintf(fd, "%s\n", buffer);

	for (size_t i = 0; i < buffer_ptr; i++) {
		if (buffer[i] == '\0') {
			current++;
			if (chosen_lines[current] != invert_chosen)
				dprintf(fd, "%s\n", &buffer[i + 1]);
		}
	}
}

/**
 * @brief Prints the chosen lines to the specified output file.
 *
 * @param filename Name of the output file.
 */
void output_chosen_lines(char* filename) {
	int fd;

	fd = (filename == NULL) ? STDOUT_FILENO : open(filename, O_WRONLY | O_TRUNC | O_CREAT, 0664);
	if (fd == -1) die("Failed to open file");

	output_chosen_lines_fd(fd);
}

/**
 * @brief Displays usage information and exits the program.
 *
 * @param exitcode Exit code.
 */
NORETURN void usage(int exitcode) {
	fprintf(stderr, "Usage: %s [-hvx] [-o output] <input> [command ...]\n", argv0);
	exit(exitcode);
}

/**
 * @brief Initializes and handles the ncurses window for line selection.
 */
void display_window(void) {
	initscr();
	cbreak();
	noecho();
	keypad(stdscr, TRUE);

	int current_line = 0;
	int start_line   = 0;
	int ch;
	int max_y = getmaxy(stdscr);

	draw_lines(stdscr, current_line, start_line, max_y);

	while ((ch = getch()) != 'q') {
		switch (ch) {
			case KEY_UP:
			case KEY_LEFT:
				if (current_line > 0) {
					(current_line)--;
					if (current_line < start_line)
						start_line--;
				}
				break;
			case KEY_DOWN:
			case KEY_RIGHT:
				if (current_line < line_count - 1) {
					(current_line)++;
					if (current_line >= start_line + max_y)
						start_line++;
				}
				break;
			case 'v':
				invert_chosen = !invert_chosen;
				break;
			case KEY_ENTER:
			case ' ':
				chosen_lines[current_line] = !chosen_lines[current_line];
				break;
		}
		draw_lines(stdscr, current_line, start_line, max_y);
	}

	endwin();
}

/**
 * @brief Executes a command and passes the chosen lines as stdin.
 *
 * @param argv Command and its arguments.
 */
void execute_command(char** argv) {
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
		output_chosen_lines_fd(pipefd[1]);
		close(pipefd[1]);    // Close write end after writing
		wait(NULL);          // Wait for the child process to finish
	}
}

void execute_command_xargs(int argc, char** argv) {
	char** newargv;
	int    chosencount = argc + 1;    // + NULL
	for (int i = 0; i < line_count; i++)
		if (chosen_lines[i] != invert_chosen)
			chosencount++;

	newargv = malloc(chosencount * sizeof(char*));
	int newargc;
	for (newargc = 0; newargc < argc; newargc++)
		newargv[newargc] = argv[newargc];

	size_t current = 0;
	if (chosen_lines[0] != invert_chosen)
		newargv[newargc++] = buffer;

	for (size_t i = 0; i < buffer_ptr; i++) {
		if (buffer[i] == '\0') {
			current++;
			if (chosen_lines[current] != invert_chosen)
				newargv[newargc++] = &buffer[i + 1];
		}
	}

	newargv[newargc++] = NULL;

	pid_t pid;

	if ((pid = fork()) == -1) die("fork");
	if (pid == 0) {
		execvp(newargv[0], newargv);
		die("execvp");
	}
	wait(NULL);
}

int main(int argc, char* argv[]) {
	char* output = NULL;
	bool  xargs  = false;

	argv0 = argv[0];
	ARGBEGIN
	switch (OPT) {
		case 'h':
			usage(0);
		case 'v':
			invert_chosen = true;
			break;
		case 'x':
			xargs = true;
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

	load_file(argv[0]);
	SHIFT;

	display_window();

	if (argc == 0) {
		output_chosen_lines(output);
	} else if (!xargs) {
		execute_command(argv);
	} else {
		execute_command_xargs(argc, argv);
	}

	free(buffer);
	free(chosen_lines);

	return 0;
}
