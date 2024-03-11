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

## Commits
Commits should be formatted following the [Conventional Commits][conventional-commits] specification.
The project loosely follows the spec, only using the `<type>: <description` in the commit message header.

Though there's no hard rule as to what types are allowed, the ones generally used are:

1. `fix`: a commit of the type fix patches a bug in your codebase.
2. `feat`: a commit of the type feat introduces a new feature to the codebase.
3. `refactor`: publicly-visible refactors 
4. `test`: changes strictly to tests with no accompanying changes to API.
5. `build`: changes to build (e.g. CMakeLists.txt) or Github Workflows config. 
6. `chore`: cleanup or refactoring that don't modify public API.
7. `misc`: anything else.

[conventional-commits]: https://www.conventionalcommits.org/en/v1.0.0/

