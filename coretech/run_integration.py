#!/usr/bin/env python3

"""Run a Python traffic streamer against a FIRE STORM executable.

The application is started first, followed by the Python streamer. Arguments after ``--`` are passed unchanged to the
FIRE STORM application executable.

Usage:
    run_integration.py --app EXECUTABLE --streamer SCRIPT [-- APP_ARGS...]

Command-line arguments:
    --app EXECUTABLE:
        Path to the application executable

    --streamer SCRIPT:
        Path ot the Python traffic-streaming script

    --:
        Ends the runner arguments. Every subsequent argument is passed unchanged to the application executable.

Exit status:
    Returns the streamer's exit code if the streamer fails. Otherwise, returns the application's exit code. An unhandled
    error or timeout also produces a nonzero exit status.
"""

import argparse
import subprocess
import sys


def main() -> int:
    parser = argparse.ArgumentParser()

    parser.add_argument("--app", required=True)
    parser.add_argument("--streamer", required=True)

    runner_args = sys.argv[1:]
    app_args: list[str] = []

    if "--" in runner_args:
        separator = runner_args.index("--")
        app_args = runner_args[separator + 1:]
        runner_args = runner_args[:separator]

    args = parser.parse_args(runner_args)
    app = subprocess.Popen([args.app, *app_args])

    try:
        streamer = subprocess.run([sys.executable, args.streamer, "--quiet"], check=False)

        if streamer.returncode != 0:
            return streamer.returncode

        return app.wait(timeout=30)

    finally:
        if app.poll() is None:
            app.terminate()
            try:
                app.wait(timeout=5)
            except subprocess.TimeoutExpired:
                app.kill()
                app.wait()


if __name__ == "__main__":
    raise SystemExit(main())
