#ifndef LEAK_TRACKER_H
#define LEAK_TRACKER_H

#include <stddef.h>

/*
 * Public prototypes for our custom allocation functions.
 * Each wrapper takes the same arguments as the real allocator,
 * plus file/line for reporting.
 */
void* my_malloc(size_t size, const char* file, int line);
void* my_calloc(size_t nmemb, size_t size, const char* file, int line);
void* my_realloc(void* ptr, size_t size, const char* file, int line);
void  my_free(void* ptr, const char* file, int line);

/*
 * Macros to replace the standard functions with our wrappers.
 * __FILE__ and __LINE__ are captured automatically.
 */
#define malloc(sz)    my_malloc((sz), __FILE__, __LINE__)
#define calloc(nm, s) my_calloc((nm), (s), __FILE__, __LINE__)
#define realloc(p, s) my_realloc((p), (s), __FILE__, __LINE__)
#define free(p)       my_free((p), __FILE__, __LINE__)

#endif // LEAK_TRACKER_H
