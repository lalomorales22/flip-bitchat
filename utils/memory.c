#include "memory.h"
#include <furi.h>

#define TAG "Memory"
#define MEMORY_LOW_THRESHOLD (32 * 1024)  // 32KB threshold

void memory_log_usage(const char* tag) {
    size_t free = memmgr_get_free_heap();
    size_t total = memmgr_get_total_heap();
    size_t used = total - free;
    
    FURI_LOG_I(
        tag ? tag : TAG,
        "Memory: %zu/%zu KB used (%.1f%%), %zu KB free",
        used / 1024,
        total / 1024,
        (float)used * 100.0f / (float)total,
        free / 1024);
}

size_t memory_get_free_heap(void) {
    return memmgr_get_free_heap();
}

size_t memory_get_total_heap(void) {
    return memmgr_get_total_heap();
}

bool memory_is_low(void) {
    return memmgr_get_free_heap() < MEMORY_LOW_THRESHOLD;
}

// Memory pool implementation
typedef struct {
    void* block;
    bool in_use;
} MemoryBlock;

struct MemoryPool {
    MemoryBlock* blocks;
    size_t block_size;
    size_t block_count;
    FuriMutex* mutex;
};

MemoryPool* memory_pool_create(size_t block_size, size_t block_count) {
    MemoryPool* pool = malloc(sizeof(MemoryPool));
    if(!pool) return NULL;
    
    pool->block_size = block_size;
    pool->block_count = block_count;
    pool->mutex = furi_mutex_alloc(FuriMutexTypeNormal);
    
    pool->blocks = malloc(sizeof(MemoryBlock) * block_count);
    if(!pool->blocks) {
        furi_mutex_free(pool->mutex);
        free(pool);
        return NULL;
    }
    
    // Allocate all blocks
    for(size_t i = 0; i < block_count; i++) {
        pool->blocks[i].block = malloc(block_size);
        pool->blocks[i].in_use = false;
        
        if(!pool->blocks[i].block) {
            // Failed to allocate, cleanup
            for(size_t j = 0; j < i; j++) {
                free(pool->blocks[j].block);
            }
            free(pool->blocks);
            furi_mutex_free(pool->mutex);
            free(pool);
            return NULL;
        }
    }
    
    FURI_LOG_I(TAG, "Created memory pool: %zu blocks of %zu bytes", block_count, block_size);
    
    return pool;
}

void memory_pool_free(MemoryPool* pool) {
    if(!pool) return;
    
    furi_mutex_acquire(pool->mutex, FuriWaitForever);
    
    for(size_t i = 0; i < pool->block_count; i++) {
        if(pool->blocks[i].block) {
            free(pool->blocks[i].block);
        }
    }
    
    free(pool->blocks);
    
    furi_mutex_release(pool->mutex);
    furi_mutex_free(pool->mutex);
    
    free(pool);
}

void* memory_pool_alloc(MemoryPool* pool) {
    if(!pool) return NULL;
    
    furi_mutex_acquire(pool->mutex, FuriWaitForever);
    
    void* result = NULL;
    for(size_t i = 0; i < pool->block_count; i++) {
        if(!pool->blocks[i].in_use) {
            pool->blocks[i].in_use = true;
            result = pool->blocks[i].block;
            break;
        }
    }
    
    furi_mutex_release(pool->mutex);
    
    if(!result) {
        FURI_LOG_W(TAG, "Memory pool exhausted");
    }
    
    return result;
}

void memory_pool_dealloc(MemoryPool* pool, void* block) {
    if(!pool || !block) return;
    
    furi_mutex_acquire(pool->mutex, FuriWaitForever);
    
    for(size_t i = 0; i < pool->block_count; i++) {
        if(pool->blocks[i].block == block) {
            pool->blocks[i].in_use = false;
            break;
        }
    }
    
    furi_mutex_release(pool->mutex);
}
