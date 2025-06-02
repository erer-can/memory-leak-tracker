
# Understanding Memory Leaks in C

Memory management is a critical aspect of C programming. Unlike some higher-level languages, C gives the programmer direct control over allocating and freeing memory on the heap. This power comes with responsibility: failing to manage dynamic memory correctly can lead to memory leaks, a class of bugs that can cause poor performance and application crashes.

This document provides a **detailed** overview of memory leaks in C:

- What a memory leak is  
- Why leaks happen (common and subtle causes)  
- How leaks affect program behavior and system resources  
- Techniques and tools for detecting memory leaks  
- Best practices to prevent leaks  
- Supplementary resources for further study  

---

## What Is a Memory Leak?

A **memory leak** occurs when a program allocates memory on the heap (via `malloc`, `calloc`, or `realloc`) but does not release it (via `free`) before losing the last reference to that block. Once the pointer to allocated memory is overwritten or goes out of scope without calling `free`, the program can no longer reclaim that memory. Over time, repeated leaks accumulate, causing the process’s memory footprint to grow.

```c
char* ptr = malloc(100);  // allocate 100 bytes
// ... do something with ptr ...
// Forgot to call free(ptr);
```

In the above example, `ptr` points to 100 bytes on the heap. If `ptr` goes out of scope or is reassigned without calling `free(ptr)`, those 100 bytes remain reserved but unreachable.

---

## Common and Subtle Causes of Memory Leaks

1. **Forgotten `free`**  
   The simplest cause: a missing `free`. It typically happens in small functions or prototypes where memory allocation is quick but the corresponding deallocation is overlooked.

   ```c
   void process_data() {
       int *arr = malloc(50 * sizeof(int));
       if (!arr) return;
       // ... use arr ...
       // oops: forgot free(arr);
   }  // arr goes out of scope here, but memory remains allocated
   ```

2. **Early Return or Error Paths**  
   Functions with multiple exit points often forget to release memory before returning.

   ```c
   char* read_file(const char* filename) {
       FILE* f = fopen(filename, "r");
       if (!f) return NULL;
       char* buffer = malloc(1024);
       if (!buffer) {
           fclose(f);
           return NULL;
       }
       if (ferror(f)) {
           // forgot to free buffer before returning
           return NULL;
       }
       // ... read data ...
       free(buffer);
       fclose(f);
       return data_copy;
   }
   ```

3. **Lost Pointer / Overwriting Pointers**  
   Assigning a new pointer value to a variable without freeing its old memory “loses” the reference to the original block.

   ```c
   char *str = malloc(20);
   strcpy(str, "hello");
   str = malloc(30);  // previous 20-byte allocation is lost (leaked)
   ```

4. **Realloc Mishandling**  
   Using `realloc` incorrectly can leak memory if the result is not assigned carefully.

   ```c
   char *buf = malloc(50);
   char *temp = realloc(buf, 100);
   if (!temp) {
       // realloc failed, buf is still valid, but if we do buf = temp;
       // we lose the original allocation
       // should handle error before overwriting buf
   }
   buf = temp;
   ```

5. **Circular Data Structures**  
   Leaks can occur when freeing linked structures that point to each other, especially if not all pointers are traversed.

   ```c
   typedef struct Node {
       int* data;
       struct Node* next;
   } Node;

   Node *head = malloc(sizeof(Node));
   head->data = malloc(100);
   head->next = NULL;
   // When freeing head:
   free(head);  // forgot to free head->data first
   // data block is leaked
   ```

6. **Library or Third-Party Code**  
   Sometimes, leaks come from using libraries that allocate memory internally. Ensure documentation conventions require you to call cleanup functions.

7. **Static Global Allocations**  
   In long-running services, static/global pointers that accumulate allocations (e.g., caching) can leak over time if not bounded.

   ```c
   static char *log_cache[1000];
   void add_log_entry(const char* entry) {
       char *copy = strdup(entry);
       log_cache[index++] = copy;  // never freed until shutdown
   }
   ```

8. **Event-Driven or Callback-Oriented Code**  
   In GUI/event loops, objects may be created for events and not freed if callbacks are not invoked.

9. **Thread-Local Leaks**  
   Multi-threaded code that allocates memory on thread stacks or creates thread-local caches can leak if cleanup is tied to thread exit but threads persist.

---

## How Memory Leaks Affect Programs

- **Increased Memory Footprint**  
  Even small leaks (a few bytes per loop) can accumulate in high‐frequency allocation code paths, eventually exhausting system memory.

- **Performance Degradation**  
  As more memory is consumed, the OS may swap or perform garbage collection-like overhead (in other languages), causing slowdown.

- **Unpredictable Crashes**  
  When memory is exhausted, `malloc` may fail or the process may be killed by the OS. Crashes often occur far from the original leak site, making debugging hard.

- **Difficult Debugging**  
  Leaks do not cause immediate failures; they silently accumulate. Identifying exactly which allocation was not freed requires careful tracing.

- **Resource Starvation**  
  On embedded or resource-constrained systems, even modest leaks can lead to total system failure or watchdog resets.

---

## Techniques to Detect Memory Leaks

### 1. Instrumentation / Custom Wrappers (Manual Tracking)

Wrap allocation functions to record metadata (pointer address, size, file, line):

```c
void* my_malloc(size_t sz, const char* file, int line) {
    void* ptr = malloc(sz);
    if (ptr) record_allocation(ptr, sz, file, line);
    return ptr;
}
void my_free(void* ptr, const char* file, int line) {
    if (!ptr) return;
    if (!remove_allocation_record(ptr)) {
        fprintf(stderr, "WARNING: invalid free %p at %s:%d\n", ptr, file, line);
        return;
    }
    free(ptr);
}
#define malloc(sz)    my_malloc(sz, __FILE__, __LINE__)
#define free(ptr)     my_free(ptr, __FILE__, __LINE__)
```

At program exit (via `atexit()`), report all unfreed blocks.

- **Pros**:
  - Portable to any platform.
  - No external tools needed.
  - You see file/line for each leaked allocation.
- **Cons**:
  - Runtime overhead for tracking.
  - Doesn’t detect memory corruption (just leaks).

### 2. Valgrind (Linux/macOS)

```
valgrind --tool=memcheck --leak-check=full ./your_program
```

- **Reports**:
  - Number of leaked bytes and blocks.
  - Stack trace for each leaked allocation.
  - Definitely/possibly lost blocks classification.

**Interpreting Valgrind Output**

- **Definitely lost**: Memory that was allocated but not freed.  
- **Indirectly lost**: Memory pointed to by a definitely lost block.  
- **Possibly lost**: Memory that might be leaked but could be pointed to somewhere.  
- **Still reachable**: Memory still accessible at exit (not strictly a leak but may indicate missing frees).  

### 3. AddressSanitizer (ASan)

Compile with:

```bash
gcc -g -fsanitize=address -o myprog myprog.c
./myprog
```

- **Features**:
  - Detects out-of-bounds access, use-after-free, and memory leaks.
  - Fast instrumented builds by Clang/GCC.
  - Reports exact code locations.

### 4. Leak Sanitizer (LSan)

Often enabled via `-fsanitize=leak` alongside ASan:

```bash
gcc -g -fsanitize=leak -o myprog myprog.c
./myprog
```

- Similar to ASan’s leak detection, but focused on leaks.

### 5. Static Analysis Tools

- **clang-tidy**, **cppcheck**, **Coverity**, or **PVS-Studio** can detect common leak patterns at compile time.
- They inspect control flow and can highlight missing `free` calls.

### 6. Profilers / Runtime Monitors

- OS‐level tools (e.g., `top`, `htop`) can show increasing resident memory for long-running processes, hinting at leaks.
- Custom logging of heap usage via calls to `mallinfo()` (glibc) or equivalent.

---

## Best Practices to Prevent Leaks

1. **Pair Every `malloc/free`**  
   For every allocation, ensure a corresponding free exists on all code paths—including error returns.

2. **Use Ownership Conventions**  
   Define clear ownership rules: which function “owns” a pointer and is responsible for freeing it.

3. **Modularize with Resource Management**  
   - Write “init” and “destroy” functions for modules that allocate resources.  
   - E.g., `Widget *create_widget(); void destroy_widget(Widget*);`.

4. **Realloc Safely**  
   Always use a temporary pointer:

   ```c
   char *tmp = realloc(buf, new_size);
   if (!tmp) {
       // handle error, buf still valid
   } else {
       buf = tmp;
   }
   ```

5. **Avoid Global Allocations**  
   If you must use globals, ensure they are freed at program termination or have a cleanup routine.

6. **RAII Patterns in C++**  
   In C++, use smart pointers (`std::unique_ptr`, `std::shared_ptr`) to automate freeing.

7. **Test with Leak Detection in CI**  
   Integrate Valgrind or AddressSanitizer checks in automated tests to catch leaks early.

8. **Limit Dynamic Allocations**  
   - Use stack allocation when possible (`int arr[50];` instead of `malloc`).  
   - Use fixed-size object pools or arenas for specialized workloads.

9. **Document Memory Ownership**  
   Clearly comment which functions allocate and free which resources.

10. **Review and Code Audits**  
    Peer review for allocation/free logic, especially in complex functions handling many resources.

---

## Further Reading

- **Valgrind Documentation**  
  https://valgrind.org/docs/manual/mc-manual.html  
- **AddressSanitizer Wiki**  
  https://github.com/google/sanitizers/wiki/AddressSanitizer  
- **GNU C Library Manual: Memory Allocation**  
  https://www.gnu.org/software/libc/manual/html_node/Memory-Allocation.html  

---
