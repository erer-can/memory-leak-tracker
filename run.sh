#!/bin/bash
# ========== run.sh ==========
# Usage: ./run.sh <your_source_file.c>
#
# Steps:
#   1. Search only in examples/ or src/ (never current directory).
#   2. Copy found file into a temporary "main.c.tmp" in root, prepend #include.
#   3. Rename "main.c.tmp" to main.c.
#   4. Run 'make clean && make' to compile main.c + src/leak_tracker.c.
#   5. Execute ./leak_test_exec.
#   6. Clean up all artifacts (main.c, *.o, leak_test_exec).

set -e

if [ $# -ne 1 ]; then
    echo "Usage: $0 <your_source_file.c>"
    exit 1
fi

INPUT_NAME="$1"
INPUT_PATH=""

# 1) Look only in examples/ or src/
if [ -f "examples/$INPUT_NAME" ]; then
    INPUT_PATH="examples/$INPUT_NAME"
elif [ -f "src/$INPUT_NAME" ]; then
    INPUT_PATH="src/$INPUT_NAME"
else
    echo "Error: '$INPUT_NAME' not found in examples/ or src/"
    exit 1
fi

# 2) Verify leak_tracker files exist
if [ ! -f "src/leak_tracker.c" ] || [ ! -f "src/leak_tracker.h" ]; then
    echo "Error: src/leak_tracker.c or src/leak_tracker.h is missing."
    exit 1
fi

# 3) Create a temporary file "main.c.tmp" in repo root
TMP_MAIN="main.c.tmp"

# 4) Prepend the include, then append the original file’s content
{
    echo '#include "leak_tracker.h"'
    echo
    cat "$INPUT_PATH"
} > "$TMP_MAIN"

# 5) Rename "main.c.tmp" to "main.c" (overwriting if it existed)
mv "$TMP_MAIN" main.c

# 6) Compile via Makefile
echo "Compiling via Makefile..."
make

# 7) Run the resulting executable
echo
echo "Running './leak_test_exec'..."
echo
./leak_test_exec
echo

# 8) Clean up all generated files
echo "leaning up generated files..."
make clean > /dev/null 2>&1

echo "Done."
