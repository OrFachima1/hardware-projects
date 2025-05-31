/**
 * @file main_memory.h
 * @brief Implementation of the main system memory
 *
 * This memory module implements:
 * - 2^20 words of storage
 * - Support for block transfers (4 words per block)
 * - 16-cycle initial response delay
 * - Support for MESI coherency protocol
 */

#ifndef MAIN_MEMORY_H
#define MAIN_MEMORY_H

#include <stdint.h>
#include <stdbool.h>
#include "bus_system.h"

 /* Memory Configuration */
#define MEMORY_SIZE (1 << 20)  ///< Total memory size in words
#define RESPONSE_DELAY 14      ///< Initial delay cycles before response
#define WORDS_IN_BLOCK 4       ///< Words per cache block

/**
 * @brief Main memory structure
 */
typedef struct {
    uint32_t data[MEMORY_SIZE];  ///< Memory array

    /* Response State */
    bool waiting_to_respond;     ///< Currently counting down to respond
    uint32_t wait_cycles;        ///< Cycles left before first response
    uint32_t block_addr;         ///< Base address of block being transferred
    uint32_t words_to_send;      ///< Words remaining in current block
} main_memory_t;

/**
 * @brief Initialize main memory
 * @param mem Pointer to memory structure
 *
 * Sets all memory locations to 0
 */
void memory_init(main_memory_t* mem);

/**
 * @brief Load memory contents from file
 * @param mem Pointer to memory structure
 * @param filename Name of file containing memory data
 *
 * File format: One 32-bit word per line in hex format
 */
void memory_load(main_memory_t* mem, const char* filename);

/**
 * @brief Save memory contents to file
 * @param mem Pointer to memory structure
 * @param filename Name of file to save memory data
 *
 * File format: One 32-bit word per line in hex format
 */
void memory_save(main_memory_t* mem, const char* filename);

/**
 * @brief Update memory state each clock cycle
 * @param mem Pointer to memory structure
 * @param bus Pointer to system bus
 *
 * This function:
 * 1. Handles FLUSH commands from caches
 * 2. Processes read requests after delay
 * 3. Sends block data word by word
 */
void memory_clock(main_memory_t* mem, bus_system_t* bus);

#endif