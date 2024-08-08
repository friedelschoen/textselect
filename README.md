# textselect

`textselect` is a command-line utility that allows users to interactively select lines from a text file and optionally execute a command with the selected lines. This can be particularly useful for filtering input before processing it with other tools or scripts.

## Features

- Interactively select lines from a text file using a curses-based interface.
- Save selected lines to an output file.
- Execute commands with the selected lines as input.

## Installation

To build `textselect` from source, you'll need a C compiler and the `ncurses` library. Clone the repository and run the following commands:

```sh
git clone https://github.com/friedelschoen/textselect.git
cd textselect
make
make PREFIX=... install
```

## Usage

```sh
textselect [-hnv0] [-o output] <input> [command [args...]]
```

### Options

- `-h`: Display the help message and exit.
- `-n`: Keep empty lines which are not selectable.
- `-o output`: Specify an output file to save the selected lines.
- `-v`: Invert the selection of lines.
- `-0`: Print selected lines delimited by a NUL-character.

### Navigation and Selection Keys

- `UP, LEFT`: Move the cursor up.
- `DOWN, RIGHT`: Move the cursor down.
- `v`: Invert the selection of lines.
- `SPACE`: Select or deselect the current line.
- `ENTER, q`: Quit the selection interface.

### Examples

```sh
# most simple example, select couple lines from a text-file and print it to the terminal afterwards
textselect input.txt

# select couple lines from a text-file and save it to a text-file
textselect -o output.txt

# select couple lines from a text-file and pass these to `lolcat` for some funny output
textselect input.txt lolcat

# select couple lines from a command and print it to the terminal afterwards (choosing from installed packages in Void Linux)
textselect <(xbps-query -l)

# select couple lines from a command and execute command with lines as arguments (removing unnecessary packages in Void Linux)
textselect <(xbps-query -m) xargs xbps-remove
```

## License

This project is licensed under the zlib License. See the [LICENSE](LICENSE) file for details.

## Contributing

Contributions are welcome! Please open an issue or submit a pull request on GitHub.
