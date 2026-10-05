#!/usr/bin/env bash
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$REPO_ROOT"

if grep -RIn "BlockingQueuedConnection\|Qt::BlockingQueuedConnection" Src/FrontEnd; then
    echo "ERROR: BlockingQueuedConnection is not allowed in Src/FrontEnd."
    exit 1
fi

echo "OK: No BlockingQueuedConnection usages found in Src/FrontEnd."
