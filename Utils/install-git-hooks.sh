#!/bin/bash
#
# Install Git hooks for L2Trader repository
# This script sets up the pre-commit hook for code formatting
#

# Colors for output
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
RED='\033[0;31m'
NC='\033[0m' # No Color

# Get the root directory of the git repository
REPO_ROOT=$(git rev-parse --show-toplevel 2>/dev/null)

if [ -z "$REPO_ROOT" ]; then
    echo -e "${RED}Error: Not in a git repository${NC}"
    exit 1
fi

HOOKS_DIR="${REPO_ROOT}/.git/hooks"
HOOK_SOURCE="${REPO_ROOT}/Utils/git-hooks/pre-commit"
HOOK_DEST="${HOOKS_DIR}/pre-commit"

echo "L2Trader Git Hooks Installer"
echo "=============================="
echo ""

# Check if uncrustify is installed
if ! command -v uncrustify &> /dev/null; then
    echo -e "${YELLOW}Warning: uncrustify is not installed.${NC}"
    echo "The pre-commit hook requires uncrustify to be installed."
    echo ""
    echo "To install uncrustify:"
    echo "  Ubuntu/Debian: sudo apt-get install uncrustify"
    echo "  macOS:         brew install uncrustify"
    echo "  Arch Linux:    sudo pacman -S uncrustify"
    echo ""
    read -p "Continue with installation anyway? (y/N) " -n 1 -r
    echo
    if [[ ! $REPLY =~ ^[Yy]$ ]]; then
        echo "Installation cancelled."
        exit 1
    fi
fi

# Check if hook source exists
if [ ! -f "$HOOK_SOURCE" ]; then
    echo -e "${RED}Error: Hook source file not found at $HOOK_SOURCE${NC}"
    exit 1
fi

# Check if destination hook already exists
if [ -f "$HOOK_DEST" ]; then
    echo -e "${YELLOW}Pre-commit hook already exists.${NC}"
    read -p "Overwrite existing hook? (y/N) " -n 1 -r
    echo
    if [[ ! $REPLY =~ ^[Yy]$ ]]; then
        echo "Installation cancelled."
        exit 0
    fi
fi

# Install the hook
echo "Installing pre-commit hook..."
cp "$HOOK_SOURCE" "$HOOK_DEST"
chmod +x "$HOOK_DEST"

if [ -f "$HOOK_DEST" ] && [ -x "$HOOK_DEST" ]; then
    echo -e "${GREEN}✓ Pre-commit hook installed successfully!${NC}"
    echo ""
    echo "The hook will automatically:"
    echo "  • Check code formatting with Uncrustify"
    echo "  • Detect trailing whitespace"
    echo "  • Prevent commits with formatting issues"
    echo ""
    echo "To temporarily bypass the hook (not recommended):"
    echo "  git commit --no-verify"
    echo ""
else
    echo -e "${RED}✗ Failed to install pre-commit hook${NC}"
    exit 1
fi

exit 0
