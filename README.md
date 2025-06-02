
# Memory-Leak-Tracker

A small C library and educational resource for detecting memory leaks, double-free/invalid-free, and tracking heap usage. Includes:

- A **wrapper** around `malloc`/`calloc`/`realloc`/`free` that records allocations at runtime.
- A **demo program** showing proper frees, leaks, and invalid/double frees.
- A **run script** (`run.sh`) to inject the tracker header, build via `make`, run the executable, and display the leak report.
- Detailed **documentation** on why leaks happen and how to use the wrapper.

---

## What’s Inside?

- `src/`  
  - `leak_tracker.h` & `leak_tracker.c`: the tracking library.  
- `examples/`  
  - `demo_allocs.c`: a minimal demo that intentionally leaks memory and does invalid/double frees.  
- `run.sh`  
  - Shell script that:
    1. Looks for `<your_file>.c` in `examples/` or `src/`
    2. Prepends `#include "leak_tracker.h"`
    3. Creates `main.c` in the root
    4. Invokes `make` to compile
    5. Runs `leak_test_exec` to show leak diagnostics
    6. Cleans up all generated files afterward  
- `Makefile`  
  - Builds `main.c` + `src/leak_tracker.c` into `leak_test_exec` when you run `make`.  
- `docs/`  
  - `memory_leaks.md`: conceptual overview of memory leaks (what they are, why they happen).  
  - `using_wrapper.md`: step-by-step on how to build and use the leak tracker with `run.sh`.

---

## Quick Start

1. **Clone or download** this repo:

   ```bash
   git clone https://github.com/erer-can/memory-leak-tracker
   cd memory-leak-tracker
   ```

2. **Run the demo**:

   ```bash
   chmod +x run.sh
   ./run.sh examples/demo_allocs.c
   ```

   You should see output from `demo_allocs.c`, followed by warnings (if any) and a final memory-leak report.

3. **Use in your own project**:

   - Copy `src/leak_tracker.h` and `src/leak_tracker.c` into your C project.  
   - Place your C file (e.g., `my_program.c`) in `examples/` or `src/`.  
   - Run `./run.sh my_program.c`; it will automatically:
     1. Inject `#include "leak_tracker.h"`
     2. Build via `make`
     3. Run the executable with leak diagnostics
     4. Clean up generated files

---

##  Why Memory Leaks Matter

See [`docs/memory_leaks.md`](docs/memory_leaks.md) for an in-depth explanation of what memory leaks are, common causes, and how they can crash or degrade long-running services.  

---

## How the Wrapper Works

1. The script `run.sh` prepends `#include "leak_tracker.h"` to your source, creating `main.c`.  
2. The `Makefile` compiles:
   - `main.c` → `main.o`  
   - `src/leak_tracker.c` → `src/leak_tracker.o`  
   - Links into `leak_test_exec`.  
3. At runtime, `leak_tracker.h` redefines:
   ```c
   #define malloc(sz)    my_malloc((sz), __FILE__, __LINE__)
   #define calloc(nm, s) my_calloc((nm), (s), __FILE__, __LINE__)
   #define realloc(p, s) my_realloc((p), (s), __FILE__, __LINE__)
   #define free(p)       my_free((p), __FILE__, __LINE__)
   ```
   so each allocation or free is recorded along with file/line metadata.  
4. If you call `free` on an untracked pointer, the wrapper prints a warning.  
5. On program exit, an `atexit()` handler walks the list of active allocations—anything still unfreed is printed in the final leak report.  

For full details, see [`docs/using_wrapper.md`](docs/using_wrapper.md).

---

## Build & Run

This repo uses a simple script called `run.sh` to automate everything.

To use the memory leak tracker on an example program:

```bash
chmod +x run.sh
./run.sh demo_allocs.c
```

This script will:

- Automatically inject the tracking header  
- Build the project using `make`  
- Run the program and show leak diagnostics  
- Clean up all generated files afterward

---

## Note

This repository was developed as a teaching aid for the CMPE230 Systems Programming course at Boğaziçi University.
  
Feel free to explore the code for educational purposes.

---
