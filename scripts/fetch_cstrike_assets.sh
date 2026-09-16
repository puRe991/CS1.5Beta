#!/usr/bin/env bash
#
# Downloads the original Counter-Strike Beta "cstrike" asset directory
# (maps, models, sounds, textures) from the Ch0wW/counterstrike-betas
# preservation archive, for LOCAL testing of this engine only.
#
# These assets are proprietary Valve/Sierra game data. This project does
# not ship or redistribute them (see README.md) - this script fetches a
# copy onto your own machine instead, outside of git tracking.
#
# Usage:
#   ./scripts/fetch_cstrike_assets.sh [target-dir] [branch]
#
# Defaults:
#   target-dir = assets/cstrike (relative to repo root, gitignored)
#   branch     = cs15_retail
#
set -euo pipefail

REPO_URL="https://github.com/Ch0wW/counterstrike-betas.git"
BRANCH="${2:-cs15_retail}"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
TARGET_DIR="${1:-$REPO_ROOT/assets/cstrike}"

if [ -e "$TARGET_DIR" ]; then
    echo "Target directory already exists: $TARGET_DIR"
    echo "Remove it first if you want a fresh fetch."
    exit 1
fi

WORK_DIR="$(mktemp -d)"
trap 'rm -rf "$WORK_DIR"' EXIT

echo "Fetching cstrike/ from $REPO_URL (branch: $BRANCH) ..."

git clone --depth 1 --branch "$BRANCH" --filter=blob:none --sparse "$REPO_URL" "$WORK_DIR/repo"
git -C "$WORK_DIR/repo" sparse-checkout set cstrike

if [ ! -d "$WORK_DIR/repo/cstrike" ]; then
    echo "error: cstrike/ not found on branch '$BRANCH' of $REPO_URL" >&2
    exit 1
fi

mkdir -p "$(dirname "$TARGET_DIR")"
mv "$WORK_DIR/repo/cstrike" "$TARGET_DIR"

echo "Done. Assets placed in: $TARGET_DIR"
echo "Run the engine against them, e.g.:"
echo "  ./cs15engine <map.bsp> \"$TARGET_DIR\""
