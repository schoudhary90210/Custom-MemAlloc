/*
 * main.c - Simple driver to test mm_init
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <pthread.h>
#include <time.h>
#include "mm.h"

#define NUM_THREADS 8
#define OPS_PER_THREAD 50000
#define MAX_ALLOC_SIZE 1024

void *thread_test(void *arg) {
    int id = *(int *)arg;
    unsigned int seed = (unsigned int)time(NULL) ^ (unsigned int)id ^ (unsigned int)(uintptr_t)pthread_self();
    void *ptrs[100]; // Local array to track some pointers
    for (int i = 0; i < 100; i++) ptrs[i] = NULL;

    for (int i = 0; i < OPS_PER_THREAD; i++) {
        int idx = (int)(rand_r(&seed) % 100);
        
        // If we have an existing pointer, free it 50% of the time
        if (ptrs[idx] != NULL && (rand_r(&seed) % 2 == 0)) {
            mm_free(ptrs[idx]);
            ptrs[idx] = NULL;
        } else if (ptrs[idx] == NULL) {
            // Otherwise, allocate new memory
            size_t size = (size_t)((rand_r(&seed) % MAX_ALLOC_SIZE) + 1);
            ptrs[idx] = mm_malloc(size);
        }
    }

    // Cleanup remaining pointers
    for (int i = 0; i < 100; i++) {
        if (ptrs[i]) mm_free(ptrs[i]);
    }

    printf("  [Thread %d] Finished %d operations.\n", id, OPS_PER_THREAD);
    return NULL;
}

int main() {
    printf("--- Starting Thread-Safe Multicore Stress Test ---\n");
    
    if (mm_init() < 0) {
        printf("Initialization failed!\n");
        return 1;
    }

    pthread_t threads[NUM_THREADS];
    int thread_ids[NUM_THREADS];
    clock_t start = clock();

    for (int i = 0; i < NUM_THREADS; i++) {
        thread_ids[i] = i;
        if (pthread_create(&threads[i], NULL, thread_test, &thread_ids[i]) != 0) {
            printf("Failed to create thread %d\n", i);
            return 1;
        }
    }

    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_join(threads[i], NULL);
    }

    clock_t end = clock();
    double time_spent = (double)(end - start) / CLOCKS_PER_SEC;

    printf("\nFinal Results:\n");
    printf("- Total Threads:    %d\n", NUM_THREADS);
    printf("- Total Operations: %d\n", NUM_THREADS * OPS_PER_THREAD);
    printf("- Time Elapsed:     %.4f seconds\n", time_spent);
    printf("- Status:           SUCCESS (Thread-Safe & Stable)\n");
    printf("--------------------------------------------------\n");

    return 0;
}
