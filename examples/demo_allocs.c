// demo_allocs.c
//
// A minimal demo for the leak detector wrapper. It does:
//   1) malloc + free  (correct)
//   2) malloc        (leaked on purpose)
//   3) malloc + free + free  (double‐free warning)
//   4) free on a stack address (invalid free)

#include <stdio.h>
#include <string.h>

int main(void) {
    printf("=== leak_detector demo start ===\n\n");

    // 1) Proper malloc + free
    int *arr = (int*)malloc(5 * sizeof(int));
    if (arr) {
        for (int i = 0; i < 5; i++) {
            arr[i] = i * 10;
        }
        free(arr);
    }

    // 2) Intentional leak
    char *leaked = (char*)malloc(20);
    if (leaked) {
        strcpy(leaked, "I am a leak!");
        // (never freed)
    }

    // 3) Double‐free scenario
    float *nums = (float*)malloc(3 * sizeof(float));
    if (nums) {
        nums[0] = 1.1f;
        nums[1] = 2.2f;
        nums[2] = 3.3f;
        free(nums);
        free(nums);  // second free → should trigger a warning
    }

    // 4) Invalid free (freeing stack memory)
    int stack_var = 42;
    free(&stack_var);  // not from malloc → invalid free warning

    printf("\n=== leak_detector demo end (program will now exit) ===\n");
    return 0;
}
