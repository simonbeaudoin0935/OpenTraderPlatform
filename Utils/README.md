# OpenTraderPlatform Utils

This directory contains developer tools used by the build, Git hooks, and GUI smoke testing.

## install-git-hooks.sh

Installs Git hooks to enforce code quality standards.

### Usage

```bash
./install-git-hooks.sh
```

This script installs the pre-commit hook to `.git/hooks/` and makes it executable.

The pre-commit hook checks C++ formatting with `clang-format`, flags trailing
whitespace in C++ files, and rejects trailing whitespace in YAML files.

See the [contributing guide](../Doc/CONTRIBUTING.md) for repository workflow details.

### Requirements

- `clang-format` (install with `sudo apt-get install clang-format` on Ubuntu/Debian)

## git-hooks/

Contains Git hook scripts that can be installed using `install-git-hooks.sh`.

- **pre-commit**: Validates code formatting before commits

## generate_logging_categories.py

CMake runs this script during configuration to generate the logging-category
header from the C++ sources. It normally does not need to be run manually.

## test-gui.sh

Runs a basic GUI launch and shutdown smoke test using Xvfb and xdotool.

```bash
./Utils/test-gui.sh
```

Requirements: `xvfb`, `xdotool`, `x11-utils`; ImageMagick is optional for
screenshots, which are written to `build/gui-test-screenshots/`.
