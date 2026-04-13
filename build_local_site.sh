#!/bin/bash
set -e

# Optional --game filter: build_local_site.sh --game Bugdom2-Android
GAME_FILTER=""
while [[ $# -gt 0 ]]; do
    case "$1" in
        --game)
            GAME_FILTER="$2"
            shift 2
            ;;
        *)
            echo "Unknown argument: $1" >&2
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
echo "2. Building all WASM ports"
echo "========================================="
# Get the list of games from the ports.py script
GAMES=$(python3 -c "
import json
import subprocess
out = subprocess.check_output(['python3', 'scripts/ports.py', 'matrix', 'wasm'])
matrix = json.loads(out)
for item in matrix['include']:
    print(item['name'])
")

# Create a clean site directory
rm -rf site
mkdir -p site

for GAME in $GAMES; do
    if [ -n "$GAME_FILTER" ] && [ "$GAME" != "$GAME_FILTER" ]; then
        echo "Skipping $GAME (--game filter active)"
        continue
    fi
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
