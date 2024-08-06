# weakbox

**weakbox** is a tool for Linux designed to create a weak (not secured) container for running programs from another Linux distribution. It is particularly useful for executing glibc-based programs (mostly closed-source software) under systems that are musl-based.

## Features

- Create a container environment for running programs from different Linux distributions.
- Bind mount directories from the host system into the container.
- Map user and group IDs inside the container.
- Customizable root path for the container.
- Option to run commands within the container as `root`.

## Installation

To install **weakbox**, simply clone the repository and compile the source code:

```bash
git clone https://github.com/friedelschoen/weakbox.git
cd weakbox
make
sudo make install # which installs /usr/bin/weakbox and /usr/share/man/man1/weakbox.1
sudo make PREFIX=... install # which installs $PREFIX/bin/weakbox and $PREFIX/share/man/man1/weakbox.1
```

## Usage

Run **weakbox** with the desired options and command to execute within the container:

```bash
weakbox [options] command ...
```

By default `command` is executed, if command is omitted current shell or `/bin/bash` is executed.

### Options

- `-h`: Display usage information.
- `-s`: Run the specified command within the container as root.
- `-v`: Enable verbose mode for debugging purposes.
- `-r path`: Set the root path of the container to `path`. By default the container lays at `$WEAKBOX`.
- `-b source[:target]`: Bind mount the specified source directory to the target directory within the container. Target is relative to `root`.
- `-B source`: Remove a default bind mount from the container.
- `-u uid[:uid]`: Map user IDs inside the container.
- `-g gid[:gid]`: Map group IDs inside the container.

### Default Mounts
- `/dev`: directory containing all devices
- `/home`: home directories of users
- `/proc`: directories containing information about processes
- `/sys`: system directories for various devices
- `/tmp`: temporary directory
- `/run`: temporary directory for daemons and long-running programs
- `/etc/resolv.conf`: nameserver-resolution configuration
- `/etc/passwd`: file containing information about users
- `/etc/group`: file containing information about groups

### Examples

1. Run a program within the container:

```bash
weakbox -s /path/to/program
```

2. Create a container with custom root path and bind mount directories:

```bash
weakbox -r /custom/root -b /host/dir:/dir /path/to/program
```

3. Map user and group IDs inside the container:

```bash
weakbox -u 1000:1000 -g 1000:1000 /path/to/program
```

## Contributing

Contributions are welcome! Feel free to submit bug reports, feature requests, or pull requests through GitHub issues and pull requests.

## License

This project is licensed under the zlib-license. See the [LICENSE](LICENSE) file for details.
