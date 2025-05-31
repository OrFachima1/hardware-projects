/**
 * @file cache.h
 * @brief Header file for cache implementation with MESI coherency protocol
 */

#ifndef CACHE_H
#define CACHE_H

#include <stdint.h>
#include <stdbool.h>
#include "bus_system.h"

 /* Cache Configuration Constants */
#define CACHE_SIZE 256        ///< Total cache size in words
#define BLOCK_SIZE 4         ///< Number of words per block
#define NUM_SETS 64          ///< Number of sets (CACHE_SIZE/BLOCK_SIZE)
#define TAG_SHIFT 8          ///< Bit position for tag extraction
#define INDEX_SHIFT 2        ///< Bit position for index extraction
#define INDEX_MASK 0x3F      ///< Mask for extracting index bits
#define BLOCK_OFFSET_MASK 0x3 ///< Mask for extracting block offset bits

/**
 * @brief MESI protocol states for cache coherency
 */
typedef enum {
    MESI_I = 0,  ///< Invalid: Block not present or invalid
    MESI_S = 1,  ///< Shared: Block present and potentially in other caches
    MESI_E = 2,  ///< Exclusive: Block present only in this cache
    MESI_M = 3   ///< Modified: Block modified in this cache only
} mesi_state_t;

/**
 * @brief Tag and state storage (TSRAM) entry structure
 */
typedef struct {
    uint32_t tag;         ///< Tag bits from address
    mesi_state_t state;   ///< MESI protocol state
} tsram_entry_t;

/**
 * @brief Main cache structure
 */
typedef struct {
    /* Core Memory Components */
    uint32_t dsram[CACHE_SIZE];     ///< Data storage array
    tsram_entry_t tsram[NUM_SETS];  ///< Tag and state array
    int cache_id;                   ///< Core ID this cache belongs to

    /* Bus Transaction State */
    bool waiting_for_bus;           ///< Waiting for bus response
    uint32_t waiting_addr;          ///< Address of pending request
    bool is_write_request;          ///< True if pending request is write
    uint32_t write_data;            ///< Data to write after bus response
    bool is_mine;                   ///< Current bus transaction belongs to this cache

    /* Block Replacement State */
    bool sending_flush;             ///< Currently sending flush command
    uint32_t flush_block_addr;      ///< Base address of block being flushed
    int words_left_to_flush;        ///< Remaining words to flush
    bool need_to_clean_first;       ///< Block needs cleaning before replacement
    int words_left;                 ///< Counter for block cleaning

    /* Performance Monitoring */
    int read_hit;                   ///< Number of read hits
    int write_hit;                  ///< Number of write hits
    int read_miss;                  ///< Number of read misses
    int write_miss;                 ///< Number of write misses
} cache_t;

/* Core Functions */

/**
 * @brief Initialize cache structure
 * @param cache Pointer to cache structure
 * @param core_id ID of core this cache belongs to
 */
void cache_init(cache_t* cache, int core_id);

/**
 * @brief Process a read request to the cache
 * @param cache Pointer to cache structure
 * @param bus Pointer to bus system
 * @param addr Memory address to read
 * @param data Pointer to store read data
 * @param ready Set true if read completes, false if cache miss
 */
void cache_read(cache_t* cache, bus_system_t* bus, uint32_t addr, uint32_t* data, bool* ready);

/**
 * @brief Process a write request to the cache
 * @param cache Pointer to cache structure
 * @param bus Pointer to bus system
 * @param addr Memory address to write
 * @param data Data to write
 * @param ready Set true if write completes, false if cache miss
 */
void cache_write(cache_t* cache, bus_system_t* bus, uint32_t addr, uint32_t data, bool* ready);

/* Cache Coherency Functions */

/**
 * @brief Monitor bus for coherency operations
 * @param cache Pointer to cache structure
 * @param bus Pointer to bus system
 */
void cache_snoop(cache_t* cache, bus_system_t* bus);

/**
 * @brief Process responses received on bus
 * @param cache Pointer to cache structure
 * @param bus Pointer to bus system
 */
void cache_handle_bus_response(cache_t* cache, bus_system_t* bus);

/**
 * @brief Update cache state each clock cycle
 * @param cache Pointer to cache structure
 * @param bus Pointer to bus system
 */
void cache_clock(cache_t* cache, bus_system_t* bus);

/* Address Manipulation Functions */

/**
 * @brief Extract tag from memory address
 * @param addr Full memory address
 * @return Tag portion of address
 */
uint32_t get_tag(uint32_t addr);

/**
 * @brief Extract set index from memory address
 * @param addr Full memory address
 * @return Cache set index
 */
uint32_t get_index(uint32_t addr);

/**
 * @brief Extract block offset from memory address
 * @param addr Full memory address
 * @return Offset within cache block
 */
uint32_t get_block_offset(uint32_t addr);

/**
 * @brief Get block-aligned address
 * @param addr Full memory address
 * @return Address aligned to block boundary
 */
uint32_t get_block_addr(uint32_t addr);

#endif /* CACHE_H */
