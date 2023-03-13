#!/usr/bin/env python3

import argparse
import subprocess
import sys


def clean(args, extra=[]):
    return subprocess.run("rm -rf build/*", shell=True)


def gen(args, extra=[]):
    if 'clean' in args and args.clean:
        clean(args).check_returncode()

    flags = []
    if 'debug' in args and args.debug:
        flags += ["-D", "CMAKE_BUILD_TYPE=Debug"]

    if 'log' in args and args.log:
        flags += ["-D", "DMT_DEBUG=ON"]

    cmd = ["cmake", "-S", ".", "-B", "build/", "-D", "DMT_BUILD_TESTS=ON",
           "-D", "CMAKE_EXPORT_COMPILE_COMMANDS=1"]
    cmd += flags

    cmd = " ".join(cmd)
    print(f"Generating: '{cmd}'")
    return subprocess.run(cmd, shell=True)


def build(args, extra=[]):
    if 'clean' in args and args.clean:
        clean(args).check_returncode()

    if 'gen' in args and args.gen:
        gen(args).check_returncode()

    return subprocess.run("make -C build", shell=True)


def run(args, extra=[]):
    if 'build' in args and args.build:
        build(args).check_returncode()
    cmd = "./build/tests/dmt-tests"
    cmd += " " + " ".join(extra)
    print(f"Running '{cmd}'")
    return subprocess.run(cmd, shell=True)


def main():
    parser = argparse.ArgumentParser()
    subparsers = parser.add_subparsers(
        help="Select command to run", required=True, dest="cmd")

    clean_parser = subparsers.add_parser('clean', help="Clean the project")

    gen_parser = subparsers.add_parser(
        'gen', help="Generate Makefiles into build/")
    gen_parser.add_argument(
        '--clean',
        default=False,
        action=argparse.BooleanOptionalAction,
        help="Clean directory before generating")
    gen_parser.add_argument(
        '--debug',
        default=False,
        action=argparse.BooleanOptionalAction,
        help="Generate debug mode files")
    gen_parser.add_argument(
        '--log',
        default=False,
        action=argparse.BooleanOptionalAction,
        help="Output debug log statements")

    build_parser = subparsers.add_parser(
        'build', help="Build the project into build/")
    build_parser.add_argument(
        '--clean',
        default=False,
        action=argparse.BooleanOptionalAction,
        help="Clean directory before building")
    build_parser.add_argument(
        '--gen',
        default=False,
        action=argparse.BooleanOptionalAction,
        help="Generate before building")

    run_parser = subparsers.add_parser('run', help="Run the tests")
    run_parser.add_argument(
        '--build',
        default=False,
        action=argparse.BooleanOptionalAction,
        help="Build before running")

    args = sys.argv
    extra = []
    if "--" in args:
        break_point = sys.argv.index("--")
        args = sys.argv[1:break_point]  # Discard invoking command
        # Discard "--" and get remaining elements
        extra = sys.argv[break_point + 1:]
    else:
        args = sys.argv[1:]  # Discard invoking command

    parsed_args = parser.parse_args(args)

    try:
        if parsed_args.cmd == "clean":
            clean(parsed_args, extra)
        elif parsed_args.cmd == "gen":
            gen(parsed_args, extra)
        elif parsed_args.cmd == "build":
            build(parsed_args, extra)
        elif parsed_args.cmd == "run":
            run(parsed_args, extra)
        else:
            parser.exit(1)
    except subprocess.CalledProcessError as e:
        print(f"\nCommand '{e.cmd}' failed with return code: {e.returncode}")


if __name__ == "__main__":
    main()
