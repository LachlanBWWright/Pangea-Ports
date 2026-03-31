#!/bin/bash
set -e

# ── Parse arguments ──────────────────────────────────────────────────────
# --game <name>   Build only the specified game (case-insensitive match
#                  against the game names from ports.py).
#                  When omitted, all games are built.
FILTER_GAME=""
while [[ $# -gt 0 ]]; do
    case "$1" in
        --game)
            FILTER_GAME="$2"
            shift 2
            ;;
        *)
            echo "Unknown argument: $1"
            echo "Usage: $0 [--game <GameName>]"
            exit 1
            ;;
    esac
done

echo "========================================="
echo "1. Setting up Emscripten SDK"
echo "========================================="
if [ ! -d ".emsdk" ]; then
    echo "Cloning Emscripten SDK..."
    git clone --depth=1 https://github.com/emscripten-core/emsdk.git .emsdk
fi

cd .emsdk
./emsdk install latest
./emsdk activate latest
source ./emsdk_env.sh
cd ..

echo "========================================="
echo "2. Building WASM ports"
echo "========================================="
# Get the list of games from the ports.py script
ALL_GAMES=$(python3 -c "
import json
import subprocess
out = subprocess.check_output(['python3', 'scripts/ports.py', 'matrix', 'wasm'])
matrix = json.loads(out)
for item in matrix['include']:
    print(item['name'])
")

# Filter to a single game if --game was provided
if [ -n "$FILTER_GAME" ]; then
    MATCHED=""
    for G in $ALL_GAMES; do
        # Case-insensitive comparison
        if [ "${G,,}" = "${FILTER_GAME,,}" ]; then
            MATCHED="$G"
            break
        fi
    done
    if [ -z "$MATCHED" ]; then
        echo "ERROR: Game '$FILTER_GAME' not found. Available games:"
        for G in $ALL_GAMES; do echo "  - $G"; done
        exit 1
    fi
    GAMES="$MATCHED"
    echo "Building single game: $GAMES"
else
    GAMES="$ALL_GAMES"
    echo "Building all games"
fi

# Create a clean site directory
rm -rf site
mkdir -p site

for GAME in $GAMES; do
    echo "-----------------------------------------"
    echo " Building $GAME..."
    echo "-----------------------------------------"
    python3 scripts/ports.py run --game "$GAME" --task wasm-build
    
    echo "Staging $GAME..."
    python3 scripts/ports.py run --game "$GAME" --task stage-wasm --dest "site/$GAME"
done

echo "========================================="
echo "3. Assembling the Site"
echo "========================================="
cp docs/index.html site/index.html
touch site/.nojekyll

echo "========================================="
echo "4. Running the site locally"
echo "========================================="
echo "Serving on http://localhost:8000"
echo "Press Ctrl+C to stop."
python3 -m http.server 8000 --directory site
