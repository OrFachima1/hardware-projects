/**
 * @file cache.c
 * @brief Implementation of cache functionality with MESI protocol
 */

#include "cache.h"

 /* Address Manipulation Functions */

uint32_t get_tag(uint32_t addr) {
    return addr >> TAG_SHIFT;
}

uint32_t get_index(uint32_t addr) {
    return (addr >> INDEX_SHIFT) & INDEX_MASK;
}

uint32_t get_block_offset(uint32_t addr) {
    return addr & BLOCK_OFFSET_MASK;
}

uint32_t get_block_addr(uint32_t addr) {
    return addr & ~BLOCK_OFFSET_MASK;
}

/* Core Cache Functions */

void cache_init(cache_t* cache, int core_id) {
    // Set core ID
    cache->cache_id = core_id;

    // Initialize TSRAM - all entries invalid
    for (int i = 0; i < NUM_SETS; i++) {
        cache->tsram[i].state = MESI_I;
        cache->tsram[i].tag = 0;
    }

    // Initialize DSRAM to zero
    for (int i = 0; i < CACHE_SIZE; i++) {
        cache->dsram[i] = 0;
    }

    // Initialize bus transaction state
    cache->is_mine = false;
    cache->waiting_for_bus = false;
    cache->sending_flush = false;

    // Initialize performance counters
    cache->read_hit = 0;
    cache->write_hit = 0;
    cache->read_miss = 0;
    cache->write_miss = 0;

    // Initialize block replacement state
    cache->need_to_clean_first = false;
    cache->words_left = -1;
}

void cache_read(cache_t* cache, bus_system_t* bus, uint32_t addr, uint32_t* data, bool* ready) {
    // Cannot process new request if busy with bus
    if (cache->waiting_for_bus || cache->sending_flush) {
        *ready = false;
        return;
    }

    // Extract address components
    uint32_t tag = get_tag(addr);
    uint32_t index = get_index(addr);
    uint32_t offset = get_block_offset(addr);

    // Check for cache hit
    if (cache->tsram[index].tag == tag && cache->tsram[index].state != MESI_I) {
        // Cache hit - return data immediately
        *data = cache->dsram[index * BLOCK_SIZE + offset];
        *ready = true;
        cache->read_hit++;
        cache->is_mine = false;
        return;
    }

    // Cache miss - handle replacement if necessary
    if (cache->tsram[index].state == MESI_M && !cache->need_to_clean_first) {
        // Need to write back modified block first
        cache->need_to_clean_first = true;
        cache->words_left = 0;
        uint32_t block_addr = (cache->tsram[index].tag << TAG_SHIFT) | (index << INDEX_SHIFT);
        *ready = false;
        cache->read_miss++;
        // Start flush of first word
        bus_request(bus, cache->cache_id, BUS_FLUSH,
            block_addr + cache->words_left - 1,
            cache->dsram[index * BLOCK_SIZE + cache->words_left++]);
    }
    else if (cache->need_to_clean_first) {
        // Continue flushing remaining words
        uint32_t block_addr = (cache->tsram[index].tag << TAG_SHIFT) | (index << INDEX_SHIFT);
        bus_request(bus, cache->cache_id, BUS_FLUSH,
            block_addr + cache->words_left - 1,
            cache->dsram[index * BLOCK_SIZE + cache->words_left++]);

        if (cache->words_left == BLOCK_SIZE) {
            // Finished flushing
            cache->need_to_clean_first = false;
            cache->tsram[index].state = MESI_I;
        }
        *ready = false;
        cache->read_miss++;
    }
    else {
        // Standard cache miss - request block
        cache->waiting_for_bus = true;
        cache->waiting_addr = addr;
        cache->is_write_request = false;
        *ready = false;
        cache->read_miss++;
        bus_request(bus, cache->cache_id, BUS_RD, addr, 0);
        cache->read_hit--; // Adjust for initial increment
    }
}

void cache_write(cache_t* cache, bus_system_t* bus, uint32_t addr, uint32_t data, bool* ready) {
    // Cannot process new request if busy with bus
    if (cache->waiting_for_bus || cache->sending_flush) {
        *ready = false;
        return;
    }

    // Extract address components
    uint32_t tag = get_tag(addr);
    uint32_t index = get_index(addr);
    uint32_t offset = get_block_offset(addr);

    // Check for cache hit with correct tag
    if (cache->tsram[index].tag == tag) {
        switch (cache->tsram[index].state) {
        case MESI_M:
        case MESI_E:
            // Can write directly in Modified or Exclusive state
            cache->dsram[index * BLOCK_SIZE + offset] = data;
            cache->tsram[index].state = MESI_M;
            *ready = true;
            cache->write_hit++;
            cache->is_mine = false;
            break;

        case MESI_S:
            // Need exclusive access - request upgrade
            cache->waiting_for_bus = true;
            cache->waiting_addr = addr;
            cache->is_write_request = true;
            cache->write_data = data;
            *ready = false;
            cache->write_miss++;
            bus_request(bus, cache->cache_id, BUS_RDX, addr, 0);
            bus_set_shared(bus);
            cache->write_hit--;
            break;

        case MESI_I:
            // Need to fetch block first
            cache->waiting_for_bus = true;
            cache->waiting_addr = addr;
            cache->is_write_request = true;
            cache->write_data = data;
            *ready = false;
            cache->write_miss++;
            bus_request(bus, cache->cache_id, BUS_RDX, addr, 0);
            cache->write_hit--;
            break;
        }
        return;
    }

    // Cache miss - handle replacement if necessary
    if (cache->tsram[index].state == MESI_M && !cache->need_to_clean_first) {
        // Need to write back modified block first
        cache->need_to_clean_first = true;
        cache->words_left = 0;
        uint32_t block_addr = (cache->tsram[index].tag << TAG_SHIFT) | (index << INDEX_SHIFT);
        *ready = false;
        cache->write_miss++;
        bus_request(bus, cache->cache_id, BUS_FLUSH,
            block_addr + cache->words_left - 1,
            cache->dsram[index * BLOCK_SIZE + cache->words_left++]);
    }
    else if (cache->need_to_clean_first) {
        // Continue flushing remaining words
        uint32_t block_addr = (cache->tsram[index].tag << TAG_SHIFT) | (index << INDEX_SHIFT);
        bus_request(bus, cache->cache_id, BUS_FLUSH,
            block_addr + cache->words_left - 1,
            cache->dsram[index * BLOCK_SIZE + cache->words_left++]);

        if (cache->words_left == BLOCK_SIZE) {
            cache->need_to_clean_first = false;
            *ready = false;
            cache->write_miss++;
        }
    }
    else {
        // Standard cache miss - request block with exclusive access
        cache->waiting_for_bus = true;
        cache->waiting_addr = addr;
        cache->is_write_request = true;
        cache->write_data = data;
        *ready = false;
        cache->write_miss++;
        bus_request(bus, cache->cache_id, BUS_RDX, addr, 0);
        cache->write_hit--;
    }
}

void cache_snoop(cache_t* cache, bus_system_t* bus) {
    // Ignore our own transactions except flushes
    if (bus->bus_origid == cache->cache_id &&
        bus->bus_cmd != BUS_NO_CMD &&
        bus->bus_cmd != BUS_FLUSH) {
        cache->is_mine = true;
        return;
    }

    // Handle block cleaning state update
    if (cache->need_to_clean_first) {
        if (bus->bus_cmd != BUS_FLUSH || bus->bus_origid != cache->cache_id) {
            cache->words_left--;
        }
    }

    // Extract address components for current bus transaction
    uint32_t index = get_index(bus->bus_addr);
    uint32_t tag = get_tag(bus->bus_addr);

    // Check if we have this block in our cache
    if (cache->tsram[index].tag == tag && cache->tsram[index].state != MESI_I) {
        switch (bus->bus_cmd) {
        case BUS_RD:
            // Handle read request from another cache
            if (cache->tsram[index].state == MESI_M) {
                // We have modified data - need to provide it
                bus_set_shared(bus);
                // Prepare to flush our modified data
                cache->sending_flush = true;
                cache->flush_block_addr = get_block_addr(bus->bus_addr);
                cache->words_left_to_flush = BLOCK_SIZE;
                // Change our state to Shared
                cache->tsram[index].state = MESI_S;
            }
            else if (cache->tsram[index].state == MESI_E) {
                // We have exclusive but unmodified data
                cache->tsram[index].state = MESI_S;
                bus_set_shared(bus);
            }
            else if (cache->tsram[index].state == MESI_S) {
                // Already in shared state - just signal presence
                bus_set_shared(bus);
            }
            break;

        case BUS_RDX:
            // Handle exclusive read request
            if (cache->tsram[index].state == MESI_M) {
                // Need to flush our modified data
                cache->sending_flush = true;
                cache->flush_block_addr = get_block_addr(bus->bus_addr);
                cache->words_left_to_flush = BLOCK_SIZE;
            }
            // Must invalidate our copy
            cache->tsram[index].state = MESI_I;
            break;
        }
    }
}

void cache_handle_bus_response(cache_t* cache, bus_system_t* bus) {
    // Only process responses for our own pending requests
    if (!cache->waiting_for_bus || !cache->is_mine) {
        return;
    }

    // Handle incoming flush data
    if (bus->bus_cmd == BUS_FLUSH) {
        uint32_t index = get_index(bus->bus_addr);
        uint32_t offset = get_block_offset(bus->bus_addr);

        // Store the received word in our cache
        cache->dsram[index * BLOCK_SIZE + offset] = bus->bus_data;

        // Check if this completes the block transfer
        if (offset == BLOCK_SIZE - 1) {
            // Update tag and state
            cache->tsram[index].tag = get_tag(bus->bus_addr);

            if (cache->is_write_request) {
                // For write requests, transition to Modified
                cache->tsram[index].state = MESI_M;
                // Perform the pending write operation
                offset = get_block_offset(cache->waiting_addr);
                cache->dsram[index * BLOCK_SIZE + offset] = cache->write_data;
            }
            else {
                // For read requests, state depends on shared signal
                cache->tsram[index].state = bus->bus_shared.Q == 1 ? MESI_S : MESI_E;
            }

            // Transaction is complete
            cache->waiting_for_bus = false;
        }
    }
}

void cache_clock(cache_t* cache, bus_system_t* bus) {
    // First priority: handle any pending flushes
    if (cache->sending_flush && cache->words_left_to_flush > 0) {
        // Calculate address and data for current word
        uint32_t curr_word = BLOCK_SIZE - cache->words_left_to_flush;
        uint32_t send_addr = cache->flush_block_addr + curr_word;
        uint32_t index = get_index(cache->flush_block_addr);
        uint32_t offset = curr_word;

        // Get data to send
        uint32_t data = cache->dsram[index * BLOCK_SIZE + offset];

        // Send flush command for current word
        bus_request(bus, cache->cache_id, BUS_FLUSH, send_addr, data);

        // Update flush progress
        cache->words_left_to_flush--;
        if (cache->words_left_to_flush == 0) {
            cache->sending_flush = false;
        }
        return;
    }

    // If waiting for bus response, no other actions needed
    if (cache->waiting_for_bus) {
        return;
    }

    // Check if we need to initiate a new bus request
    if (cache->waiting_for_bus) {
        if (cache->is_write_request) {
            // Write requests need exclusive access - use BusRdX
            bus_request(bus, cache->cache_id, BUS_RDX,
                get_block_addr(cache->waiting_addr), 0);
        }
        else {
            // Read requests can use shared access - use BusRd
            bus_request(bus, cache->cache_id, BUS_RD,
                get_block_addr(cache->waiting_addr), 0);
        }
    }
}