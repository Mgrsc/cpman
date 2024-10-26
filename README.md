# cpman (Compose Project Manager)

cpman is a command-line tool for managing Docker Compose or Podman Compose projects. It helps users easily stop, start, or update multiple compose projects.

## Features

- Automatically finds and manages multiple compose files
- Supports Docker Compose and Podman Compose
- Can stop, start, or update all found compose projects
- Provides both interactive menu and command-line argument usage
- Supports specifying search paths and operation modes
- Can exclude specific files or directories

## System Requirements

- Linux operating system
- GCC compiler
- Make tool
- Docker or Podman (and corresponding compose tools)

## Installation

1. Clone this repository:
```
   git clone https://github.com/yourusername/cpman.git
   cd cpman
   ```

2. Compile the program:
   ```
   make
   ```

3. Install the program (optional, requires root privileges):
   ```
   sudo make install
   ```

   Installs to `/usr/local/bin` by default. To install to a different directory, use:
   ```
   sudo make install INSTALL_DIR=/your/preferred/path
   ```

## Usage

### Command-line Arguments

   ```
cpman [OPTIONS]

Options:
  -p PATH    Specifies the path to search for compose files
  -m MODE    Specifies the operation mode: 1 (stop), 2 (start), 3 (update, default)
  -e PATTERN Excludes files or directories matching PATTERN
  --help     Displays help information
```

### Examples

1. Search the current directory and update all compose projects:
```
   cpman
   ```

2. Search a specific directory and stop all compose projects:
   ```
   cpman -p /path/to/projects -m 1
   ```

3. Search a specific directory and start all compose projects:
   ```
   cpman -p /path/to/projects -m 2
   ```

4. Search and update all compose projects, but exclude files or directories containing "test":
   ```
   cpman -e "test"
   ```

5. Search a specific directory and start all compose projects, but exclude files or directories containing "dev":
   ```
   cpman -p /path/to/projects -m 2 -e "dev"
   ```

### Interactive Menu

If no operation mode is specified, cpman will display an interactive menu allowing the user to choose the desired action.

## Notes

- The program ignores directories containing "ignore"
- Ensure you have sufficient permissions to manage Docker or Podman
- The update operation will first attempt to pull new images and only restart services if there are updates
- The exclusion pattern (-e) uses simple string matching and will exclude all files and directories that contain the specified string in their path

## Uninstallation

If you installed the program using `make install`, you can uninstall it using the following command:

   ```
sudo make uninstall
```

## Configuration File

cpman does not use a configuration file. All settings are specified via command-line arguments.

## Troubleshooting

If you encounter problems:
1. Ensure you have the correct permissions to access compose files and execute Docker/Podman commands
2. Check your PATH environment variable to ensure that docker-compose or podman-compose can be found
3. Use the `--help` option to see all available command-line arguments

## Contributing

Bug reports and pull requests are welcome. For major changes, please open an issue first to discuss what you would like to change.

## Version History

- 1.1.0 - Added the ability to exclude files/directories
- 1.0.0 - Initial release

## Author

[Your Name] - [Your Email]

## License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details