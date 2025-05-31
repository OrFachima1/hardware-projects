// main_memory.c
#include "main_memory.h"
#include <stdio.h>

void memory_init(main_memory_t* mem) {
    for (int i = 0; i < MEMORY_SIZE; i++) {
        mem->data[i] = 0;
    }
    mem->waiting_to_respond = false;
    mem->wait_cycles = 0;
    mem->block_addr = 0;
    mem->words_to_send = 0;
}

void memory_load(main_memory_t* mem, const char* filename) {
    FILE* f = fopen(filename, "r");
    if (!f) return;

    int addr = 0;
    uint32_t value;
    while (addr < MEMORY_SIZE && fscanf(f, "%x", &value) == 1) {
        mem->data[addr++] = value;
    }

    fclose(f);
}

void memory_save(main_memory_t* mem, const char* filename) {
    FILE* f = fopen(filename, "w");
    if (!f) return;

    for (int i = 0; i < MEMORY_SIZE; i++) {
        fprintf(f, "%08X\n", mem->data[i]);
    }

    fclose(f);
}

void memory_clock(main_memory_t* mem, bus_system_t* bus) {
    // First check if we need to update memory from a FLUSH
    if (bus->bus_cmd == BUS_FLUSH && bus->bus_origid != 4) {
        // Update memory with the flushed data
        mem->data[bus->bus_addr] = bus->bus_data;
      
       printf("Memory update flush from %d : adrress %d to %d\n", bus->bus_origid, bus->bus_addr, bus->bus_data);
        // If we were waiting to respond and someone else is flushing,
        // cancel our response
        if (mem->waiting_to_respond && bus->bus_origid != 4) {  // 4 = MAIN_MEMORY
            mem->waiting_to_respond = false;
            mem->wait_cycles = 0;
            mem->words_to_send = 0;
        }
        return;
    }

    // Handle ongoing response
    if (mem->waiting_to_respond) {
        if (mem->wait_cycles > 0) {
            mem->wait_cycles--;
            return;
        }

        if (mem->words_to_send > 0) {
            // Send next word of block
            uint32_t word_addr = mem->block_addr + (WORDS_IN_BLOCK - mem->words_to_send);
            bus_request(bus, 4, BUS_FLUSH, word_addr, mem->data[word_addr]);
            mem->words_to_send--;

            // If this was the last word, we're done responding
            if (mem->words_to_send == 0) {
                mem->waiting_to_respond = false;
            }
            return;
        }
    }

    // Check for new read requests that need response
    if (bus->bus_cmd == BUS_RD ||
        (bus->bus_cmd == BUS_RDX && bus->bus_data != -1 )) {
        mem->waiting_to_respond = true;
        mem->wait_cycles = RESPONSE_DELAY;
        mem->block_addr = bus->bus_addr & ~(WORDS_IN_BLOCK - 1);  // Align to block
        mem->words_to_send = WORDS_IN_BLOCK;
    }
}