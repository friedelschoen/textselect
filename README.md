# textselect

`textselect` is a command-line utility for interactively selecting lines from a text file using an ncurses interface. Selected lines can be saved to a file or passed as arguments to a command.

## Features

- **Interactive Selection**: Navigate through lines of a text file and select or deselect lines using an ncurses interface.
- **Customizable Output**: Save selected lines to a file or pass them as arguments to another command.
- **Invert Selection**: Toggle the inversion of selection to easily change which lines are selected.
- **Use with `xargs`**: Pass selected lines as arguments to a command.

## Installation

To build and install `textselect`, follow these steps:

1. **Clone the Repository**:

    ```sh
    git clone https://github.com/friedelschoen/textselect
    cd textselect
    ```

2. **Compile the Program**:

    ```sh
    make
    ```

3. **Install the Program** (optional):

    ```sh
    sudo make install
    ```

## Usage

```sh
textselect [-hvx] [-o output] <input> [command [args...]]
```

### Options

- `-h`  
  Display help information and exit.

- `-v`  
  Invert the selection of lines.

- `-x`  
  Pass the selected lines as arguments to the specified command.

- `-o output`  
  Specify an output file to save the selected lines. If not specified, the selected lines are printed to stdout.

### Navigation and Selection Keys

- `UP`, `LEFT`  
  Move the cursor up.

- `DOWN`, `RIGHT`  
  Move the cursor down.

- `v`  
  Invert the selection of lines.

- `SPACE`  
  Select or deselect the current line.

- `ENTER`, `q`  
  Quit the selection interface.

### Examples

- **Select lines from `input.txt` and save them to `output.txt`**:

    ```sh
    textselect -o output.txt input.txt
    ```

- **Select lines from `input.txt` and pass them as arguments to the `sort` command**:

    ```sh
    textselect input.txt sort
    ```

- **Select lines from `input.txt` and pass them as arguments to a command using `xargs`**:

    ```sh
    textselect -x input.txt ls
    ```

## Contributing

Contributions are welcome! Please open an issue or submit a pull request with any improvements or fixes.

## License

`textselect` is licensed under the Zlib License. See the [LICENSE](LICENSE) file for details.
