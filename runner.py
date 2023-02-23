#!/usr/bin/env python3

import argparse
import subprocess

def clean(args):
  subprocess.run("rm -rf build/*", shell=True)

def build(args):
  if args.clean:
      clean(args)

  subprocess.run("cmake -S . -B build/ -D DMT_BUILD_TESTS=ON", shell=True)
  subprocess.run("make -C build", shell=True)

def run(args):
  subprocess.run("./build/tests/dmt-tests", shell=True)

def main():
  parser = argparse.ArgumentParser()
  subparsers = parser.add_subparsers(help="Select command to run", required=True, dest="cmd")

  clean_parser = subparsers.add_parser('clean', help="Clean the project")

  build_parser = subparsers.add_parser('build', help="Build the project into build/")
  build_parser.add_argument('clean', action="store_true", help="Clean directory before building")

  run_parser = subparsers.add_parser('run', help="Run the tests")

  args = parser.parse_args()

  if args.cmd == "clean":
    clean(args)
  elif args.cmd == "build":
    build(args)
  elif args.cmd == "run":
    run(args)
  else:
    parser.exit(1)

if __name__ == "__main__" :
  main()