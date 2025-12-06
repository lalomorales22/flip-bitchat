#pragma once

#include <furi.h>

/**
 * @brief Memory optimization utilities
 */

/**
 * @brief Log current memory usage
 */
void memory_log_usage(const char* tag);

/**
 * @brief Get free heap size
 * @return Free heap size in bytes
 */
size_t memory_get_free_heap(void);

/**
 * @brief Get total heap size
 * @return Total heap size in bytes
 */
size_t memory_get_total_heap(void);

/**
 * @brief Check if memory is critically low
 * @return true if memory is below threshold
 */
bool memory_is_low(void);

/**
 * @brief Memory pool for packet allocation
 */
typedef struct MemoryPool MemoryPool;

/**
 * @brief Create a memory pool
 * @param block_size Size of each block
 * @param block_count Number of blocks
 * @return Memory pool or NULL on error
 */
MemoryPool* memory_pool_create(size_t block_size, size_t block_count);

/**
 * @brief Free a memory pool
 * @param pool Memory pool
 */
void memory_pool_free(MemoryPool* pool);

/**
 * @brief Allocate a block from the pool
 * @param pool Memory pool
 * @return Pointer to block or NULL if pool is full
 */
void* memory_pool_alloc(MemoryPool* pool);

/**
 * @brief Return a block to the pool
 * @param pool Memory pool
 * @param block Block to return
 */
void memory_pool_dealloc(MemoryPool* pool, void* block);
