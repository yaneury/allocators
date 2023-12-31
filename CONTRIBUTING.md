# Contributing

Follow the instructions below to build, run, and test the project.

## Requirements

This project requires CMake, CTest and a modern C++ compiler supporting at least
C++20.

## Building

This project uses CMake Presets to make building and testing the library more convenient:

To configure the project in debug mode, run:

```sh
cmake --preset debug
```

Then, to build it (can skip previous command):

```sh
cmake --build --preset debug
```

Lastly, tests can be run likeso:

```sh
ctest --preset test
```