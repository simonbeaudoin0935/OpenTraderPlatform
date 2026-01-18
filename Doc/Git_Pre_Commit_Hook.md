# Git Pre-Commit Hook for Code Formatting

## Overview

This repository includes a pre-commit hook that automatically checks C++ code formatting using Uncrustify before each commit. The hook ensures code consistency and quality by:

- **Code Formatting**: Validates that all C++ files follow the project's coding standards defined in `.uncrustify.cfg`
- **Trailing Whitespace**: Detects and prevents commits with trailing whitespace
- **Immediate Feedback**: Provides clear error messages and suggestions for fixing issues

## Installation

### Prerequisites

First, ensure Uncrustify is installed on your system:

**Ubuntu/Debian:**
```bash
sudo apt-get install uncrustify
```

**macOS:**
```bash
brew install uncrustify
```

**Arch Linux:**
```bash
sudo pacman -S uncrustify
```

### Install the Hook

From the repository root directory, run:

```bash
./Utils/install-git-hooks.sh
```

The script will:
1. Check if Uncrustify is installed
2. Copy the pre-commit hook to `.git/hooks/`
3. Make it executable
4. Confirm successful installation

## How It Works

When you run `git commit`, the pre-commit hook automatically:

1. **Identifies Staged Files**: Finds all staged C++ files (`.cpp`, `.cc`, `.cxx`, `.h`, `.hpp`)
2. **Checks Trailing Whitespace**: Scans for lines ending with whitespace
3. **Validates Formatting**: Runs Uncrustify to check if files match the project's coding style
4. **Reports Issues**: If problems are found, the commit is aborted with detailed error messages
5. **Proceeds with Commit**: If all checks pass, the commit continues normally

## Fixing Formatting Issues

If the hook detects formatting issues, it will prevent the commit and provide guidance.

### Fixing Code Formatting

To format a single file:
```bash
uncrustify -c .uncrustify.cfg --no-backup --replace path/to/file.cpp
```

To format all staged C++ files:
```bash
git diff --cached --name-only --diff-filter=ACM | grep -E '\.(cpp|cc|cxx|h|hpp)$' | xargs uncrustify -c .uncrustify.cfg --no-backup --replace
```

After formatting, stage the changes again:
```bash
git add path/to/file.cpp
git commit
```

### Removing Trailing Whitespace

To remove trailing whitespace from a file:
```bash
sed -i 's/[[:space:]]*$//' path/to/file.cpp
```

Or use your editor's built-in feature:
- **VS Code**: "Files: Trim Trailing Whitespace" (can be enabled on save)
- **Vim**: `:% s/\s\+$//e`
- **Emacs**: `M-x delete-trailing-whitespace`

## Bypassing the Hook (Not Recommended)

In rare cases where you need to commit without running the hook:

```bash
git commit --no-verify
```

**Warning**: Only use this when absolutely necessary, as it bypasses important code quality checks.

## Uncrustify Configuration

The project's code formatting rules are defined in `.uncrustify.cfg` at the repository root. Key conventions include:

- **Indentation**: 4 spaces (no tabs)
- **Line Endings**: Unix (LF)
- **Line Width**: 120 characters preferred
- **Brace Style**: K&R style (opening brace on same line)
- **Pointer/Reference**: `Type* ptr` and `Type& ref` (space before `*` and `&`)
- **Access Specifiers**: Aligned to class/struct level
- **Namespace**: No indentation for namespace content

## Troubleshooting

### Hook Not Running

If the hook doesn't run when you commit:
1. Check if it's executable: `ls -la .git/hooks/pre-commit`
2. If not: `chmod +x .git/hooks/pre-commit`
3. Verify it exists: `cat .git/hooks/pre-commit`

### Uncrustify Not Found

If you get "uncrustify: command not found":
1. Install Uncrustify (see Prerequisites above)
2. Verify installation: `uncrustify --version`
3. Ensure it's in your PATH

### False Positives

If Uncrustify suggests changes you disagree with:
1. Review the `.uncrustify.cfg` configuration
2. Discuss with the team if rules should be adjusted
3. Update `.uncrustify.cfg` if a consensus is reached

## Integration with CI/CD

The CI/CD pipeline includes automated formatting checks to ensure all commits meet standards, even if developers bypass the pre-commit hook.

A formatting check job runs on every pull request and push to main in `.github/workflows/build.yml`:

```yaml
check-formatting:
  runs-on: ubuntu-24.04
  steps:
  - uses: actions/checkout@v4
  - name: Install uncrustify
    run: sudo apt-get install -y uncrustify
  - name: Check code formatting
    run: |
      FILES=$(find Src Tests -type f \( -name "*.cpp" -o -name "*.h" -o -name "*.hpp" \))
      for file in $FILES; do
        uncrustify -c .uncrustify.cfg --check "$file" || exit 1
      done
```

This ensures that:
- All PRs are checked for formatting compliance
- Merges to main maintain code quality standards
- Team members are notified of formatting issues in CI

## Benefits

- **Consistency**: Ensures all code follows the same formatting rules
- **Code Review**: Reduces formatting-related discussions in code reviews
- **Quality**: Catches common issues like trailing whitespace early
- **Automation**: Saves time by automating manual formatting tasks
- **Team Standards**: Enforces team-wide coding conventions

## See Also

- [Uncrustify Documentation](http://uncrustify.sourceforge.net/)
- [Project README](../README.md)
- [Code Style Guidelines](../README.md#code-style)
