# probe

Directory and file statistics for Linux

## Features

- JSON output
- File metadata (MIME type implemented) (WIP)
- Plain text output (WIP)
- Directory and file statistics (WIP)
- Recursive scanning (planned)
- Real-time monitoring (planned)

## Requirements
* A compiler with support for `c++23` standard
* `conan`

## Building
Install dependencies with `conan`:
```
conan install --build=missing .
```

Source `conanbuild.sh` script to update your shell environment for the build:
```
source ./build/<target>/generators/conanbuild.sh
```

Initialize the build directory using a preset created by `conan` (e.g., `conan-release`):
```
cmake --preset=<preset>
```

Build the project:
```
cmake --build --preset=<preset>
```

## Running

Source `conanrun.sh` script to update your shell environment to run the program correctly:
```
source ./build/<target>/generators/conanrun.sh
```

Run the program:
```
./build/<target>/probe
```

## Examples

Scan a directory and output JSON:

```
probe ~/Downloads
```
