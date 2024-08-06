# textselect

`textselect` is a command-line utility that allows users to interactively select lines from a text file and optionally execute a command with the selected lines. This can be particularly useful for filtering input before processing it with other tools or scripts.

## Features

- Interactively select lines from a text file using a curses-based interface.
- Save selected lines to an output file.
- Execute commands with the selected lines as arguments.
- Flexible options for command execution, including passing selected lines as arguments, replacing placeholders in commands, or executing commands for each selected line.

## Installation

To build `textselect` from source, you'll need a C compiler and the `ncurses` library. Clone the repository and run the following commands:

```sh
git clone https://github.com/friedelschoen/textselect.git
cd textselect
make
```

## Usage

```sh
textselect [-hvxil] [-o output] <input> [command [args...]]
```

### Options

- `-h`: Display the help message and exit.
- `-v`: Invert the selection of lines.
- `-x`: Call command with selected lines as arguments (mutually exclusive with `-i` and `-l`).
- `-i`: Replace occurrences of `{}` in the command with each selected line, one at a time (mutually exclusive with `-x` and `-l`).
- `-l`: Execute the command once for each selected line (mutually exclusive with `-x` and `-i`).
- `-o output`: Specify an output file to save the selected lines.

### Navigation and Selection Keys

- `UP, LEFT`: Move the cursor up.
- `DOWN, RIGHT`: Move the cursor down.
- `v`: Invert the selection of lines.
- `SPACE`: Select or deselect the current line.
- `ENTER, q`: Quit the selection interface.

### Examples

```sh
# Interactively select lines from input.txt and save the selected lines to output.txt
textselect -o output.txt input.txt

# Interactively select lines from input.txt and pass the selected lines as arguments to the sort command
textselect input.txt sort

# Interactively select lines from input.txt and execute the echo command for each selected line, replacing {} with the selected line
textselect -i input.txt echo {}

# Interactively select lines from input.txt and execute the echo command for each selected line
textselect -l input.txt echo
```

## License

This project is licensed under the MIT License. See the [LICENSE](LICENSE) file for details.

## Contributing

Contributions are welcome! Please open an issue or submit a pull request on GitHub.
