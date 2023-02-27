#!/usr/bin/env python3

import argparse
import subprocess

def clean(args):
  subprocess.run("rm -rf build/*", shell=True)

def gen(args):
  if 'clean' in args and args.clean:
      clean(args)

  flags = ""
  if 'debug' in args and args.debug:
    flags += "-D DMT_DEBUG=ON -D CMAKE_BUILD_TYPE=Debug"

  if flags:
    print (f"Generating with flags: '{flags}'")
  cmd = "cmake -S . -B build/ -D DMT_BUILD_TESTS=ON"
  cmd += " " + flags
  subprocess.run(cmd, shell=True)

def build(args):
  if 'clean' in args and args.clean:
    clean(args)

  if 'gen' in args and args.gen:
    gen(args)

  subprocess.run("make -C build", shell=True)

def run(args):
  if 'build' in args and args.build:
    build(args)

  subprocess.run("./build/tests/dmt-tests", shell=True)

def main():
  parser = argparse.ArgumentParser()
  subparsers = parser.add_subparsers(help="Select command to run", required=True, dest="cmd")

  clean_parser = subparsers.add_parser('clean', help="Clean the project")

  gen_parser = subparsers.add_parser('gen', help="Generate Makefiles into build/")
  gen_parser.add_argument('--clean', default=False, action=argparse.BooleanOptionalAction, help="Clean directory before generating")
  gen_parser.add_argument('--debug', default=False, action=argparse.BooleanOptionalAction, help="Generate debug mode files")

  build_parser = subparsers.add_parser('build', help="Build the project into build/")
  build_parser.add_argument('--clean', default=False, action=argparse.BooleanOptionalAction, help="Clean directory before building")
  build_parser.add_argument('--gen', default=False, action=argparse.BooleanOptionalAction, help="Generate before building")

  run_parser = subparsers.add_parser('run', help="Run the tests")
  run_parser.add_argument('--build', default=False, action=argparse.BooleanOptionalAction, help="Build before running")

  args = parser.parse_args()

  if args.cmd == "clean":
    clean(args)
  elif args.cmd == "gen":
      gen(args)
  elif args.cmd == "build":
    build(args)
  elif args.cmd == "run":
    run(args)
  else:
    parser.exit(1)

if __name__ == "__main__" :
  main()