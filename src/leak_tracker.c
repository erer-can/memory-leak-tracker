/*
 * leak_tracker.c
 *
 * A memory‐leak detector wrapper for malloc/free family.
 * Now tracks double‐free vs invalid‐free separately.
 */

 #include <stdio.h>
 #include <stdlib.h>
 #include <string.h>
 #include "leak_tracker.h"
 
 /* Undefine macros so we can call the real malloc/free here */
 #ifdef malloc
 #undef malloc
 #endif
 #ifdef calloc
 #undef calloc
 #endif
 #ifdef realloc
 #undef realloc
 #endif
 #ifdef free
 #undef free
 #endif
 
 /* ----- Allocation tracking ----- */
 
 /* Node for active allocations */
 typedef struct AllocInfo {
     void*               ptr;    // pointer returned by malloc/calloc/realloc
     size_t              size;   // size of that allocation
     const char*         file;   // file where it was allocated
     int                 line;   // line where it was allocated
     struct AllocInfo*   next;   // next node in list
 } AllocInfo;
 
 /* Node for freed pointers */
 typedef struct FreedInfo {
     void*               ptr;    // pointer that was freed
     struct FreedInfo*   next;   // next freed node
 } FreedInfo;
 
 static AllocInfo*  head_allocs      = NULL;  // active allocations
 static FreedInfo*  head_freed       = NULL;  // pointers already freed
 
 /* Counters */
 static size_t total_alloc_calls      = 0;
 static size_t total_free_calls       = 0;
 static size_t total_bytes_allocated  = 0;
 static size_t total_bytes_freed      = 0;
 static size_t invalid_free_count     = 0;
 static size_t double_free_count      = 0;
 
 /* atexit handler registration flag */
 static int    atexit_registered = 0;
 
 /* Forward declarations */
 static void   register_leak_report(void);
 static void   leak_report(void);
 static void   record_allocation(void* ptr, size_t size, const char* file, int line);
 static int    remove_allocation_node(void* ptr, size_t* out_size);
 static int    is_in_freed_list(void* ptr);
 static void   add_to_freed_list(void* ptr);
 
 static void register_leak_report(void) {
     if (!atexit_registered) {
         atexit(leak_report);
         atexit_registered = 1;
     }
 }
 
 /* Insert a new allocation record */
 static void record_allocation(void* ptr, size_t size, const char* file, int line) {
     AllocInfo* node = (AllocInfo*)malloc(sizeof(AllocInfo));
     if (!node) {
         fprintf(stderr, "leak_tracker: failed to allocate tracking node\n");
         return;
     }
     node->ptr   = ptr;
     node->size  = size;
     node->file  = file;
     node->line  = line;
     node->next  = head_allocs;
     head_allocs = node;
 
     total_alloc_calls++;
     total_bytes_allocated += size;
 }
 
 /*
  * Remove an allocation record for 'ptr'.
  * If found, pop it from the active list, set *out_size = its size, free the node, return 1.
  * If not found, return 0.
  */
 static int remove_allocation_node(void* ptr, size_t* out_size) {
     AllocInfo* prev = NULL;
     AllocInfo* cur  = head_allocs;
     while (cur) {
         if (cur->ptr == ptr) {
             *out_size = cur->size;
             if (prev) {
                 prev->next = cur->next;
             } else {
                 head_allocs = cur->next;
             }
             free(cur);
             return 1;
         }
         prev = cur;
         cur  = cur->next;
     }
     return 0;
 }
 
 /* Check if ptr is already in the freed list */
 static int is_in_freed_list(void* ptr) {
     FreedInfo* cur = head_freed;
     while (cur) {
         if (cur->ptr == ptr) {
             return 1;
         }
         cur = cur->next;
     }
     return 0;
 }
 
 /* Add ptr to freed list (so future frees can be detected as double‐free) */
 static void add_to_freed_list(void* ptr) {
     FreedInfo* node = (FreedInfo*)malloc(sizeof(FreedInfo));
     if (!node) {
         fprintf(stderr, "leak_tracker: failed to allocate FreedInfo\n");
         return;
     }
     node->ptr  = ptr;
     node->next = head_freed;
     head_freed = node;
 }
 
 /* The atexit handler prints summary + any leaked blocks */
 static void leak_report(void) {
     size_t leaked_blocks = 0;
     size_t leaked_bytes  = 0;
     AllocInfo* curr = head_allocs;
 
     printf("\n===== Memory Leak Report =====\n");
     printf("Total malloc/calloc/realloc calls: %zu\n", total_alloc_calls);
     printf("Total free calls:                  %zu\n", total_free_calls);
     printf("Total bytes allocated:             %zu\n", total_bytes_allocated);
     printf("Total bytes freed:                 %zu\n", total_bytes_freed);
     printf("Double‐free attempts:              %zu\n", double_free_count);
     printf("Invalid free attempts:             %zu\n", invalid_free_count);
 
     if (!curr) {
         printf("No leaks detected!\n");
     } else {
         printf("\nLeaked blocks:\n");
         while (curr) {
             leaked_blocks++;
             leaked_bytes += curr->size;
             printf("  Leak at %p: %zu bytes (allocated at %s:%d)\n",
                    curr->ptr, curr->size, curr->file, curr->line);
             curr = curr->next;
         }
         printf("\nSummary: %zu block(s) leaked, total %zu byte(s) unfreed.\n",
                leaked_blocks, leaked_bytes);
     }
     printf("===== End of Report =====\n");
 }
 
 /* -------------------------------------------------------------------
  * my_malloc
  * -------------------------------------------------------------------
  */
 void* my_malloc(size_t size, const char* file, int line) {
     if (!atexit_registered) {
         register_leak_report();
     }
     void* ptr = malloc(size);
     if (!ptr) {
         fprintf(stderr, "leak_tracker: malloc(%zu) failed at %s:%d\n", size, file, line);
         return NULL;
     }
     record_allocation(ptr, size, file, line);
     return ptr;
 }
 
 /* -------------------------------------------------------------------
  * my_calloc
  * -------------------------------------------------------------------
  */
 void* my_calloc(size_t nmemb, size_t size, const char* file, int line) {
     if (!atexit_registered) {
         register_leak_report();
     }
     void* ptr = calloc(nmemb, size);
     if (!ptr) {
         fprintf(stderr, "leak_tracker: calloc(%zu,%zu) failed at %s:%d\n",
                 nmemb, size, file, line);
         return NULL;
     }
     record_allocation(ptr, nmemb * size, file, line);
     return ptr;
 }
 
 /* -------------------------------------------------------------------
  * my_realloc
  * -------------------------------------------------------------------
  */
 void* my_realloc(void* ptr, size_t size, const char* file, int line) {
     if (!atexit_registered) {
         register_leak_report();
     }
 
     if (ptr == NULL) {
         // Behaves like malloc(size)
         void* newptr = malloc(size);
         if (!newptr) {
             fprintf(stderr, "leak_tracker: realloc(NULL,%zu) failed at %s:%d\n",
                     size, file, line);
             return NULL;
         }
         record_allocation(newptr, size, file, line);
         return newptr;
     }
 
     if (size == 0) {
         // Behaves like free(ptr)
         my_free(ptr, file, line);
         return NULL;
     }
 
     // Check if ptr is in active allocations
     size_t old_size = 0;
     int found = remove_allocation_node(ptr, &old_size);
     if (!found) {
         // ptr not found in active list → either double‐free or invalid free
         if (is_in_freed_list(ptr)) {
             double_free_count++;
             fprintf(stderr,
                     "leak_tracker WARNING: double-free of pointer %p at %s:%d\n",
                     ptr, file, line);
         } else {
             invalid_free_count++;
             fprintf(stderr,
                     "leak_tracker WARNING: realloc on untracked pointer %p at %s:%d\n",
                     ptr, file, line);
         }
         // Still attempt real realloc (though pointer is suspect)
         return realloc(ptr, size);
     }
 
     // Perform real realloc
     void* newptr = realloc(ptr, size);
     if (!newptr) {
         fprintf(stderr, "leak_tracker: realloc(%p,%zu) failed at %s:%d\n",
                 ptr, size, file, line);
         // Since old ptr was removed, we lost that record. User must handle manually.
         return NULL;
     }
 
     // Record the new allocation and move old ptr into freed list
     add_to_freed_list(ptr);
     record_allocation(newptr, size, file, line);
     total_bytes_freed += old_size;
     return newptr;
 }
 
 /* -------------------------------------------------------------------
  * my_free
  * -------------------------------------------------------------------
  */
 void my_free(void* ptr, const char* file, int line) {
     total_free_calls++;
 
     if (ptr == NULL) {
         return;  // free(NULL) is no-op
     }
 
     // Try to remove from active allocations
     size_t block_size = 0;
     int found = remove_allocation_node(ptr, &block_size);
     if (found) {
         // Valid free: record bytes freed and add to freed list
         total_bytes_freed += block_size;
         add_to_freed_list(ptr);
         free(ptr);
     } else {
         // Not in active list → either double-free or invalid free
         if (is_in_freed_list(ptr)) {
             double_free_count++;
             fprintf(stderr,
                     "leak_tracker WARNING: double-free of pointer %p at %s:%d\n",
                     ptr, file, line);
         } else {
             invalid_free_count++;
             fprintf(stderr,
                     "leak_tracker WARNING: free of untracked pointer %p at %s:%d\n",
                     ptr, file, line);
         }
         // Do not call real free on invalid pointers
     }
 }
 