/**
 * @file bus_system.c
 * @brief Implementation of bus system functionality for MESI protocol
 */

#include "bus_system.h"

#define WORDS_IN_BLOCK 4  ///< Number of words per cache block

void bus_init(bus_system_t* bus) {
    // Initialize bus lines
    bus->bus_origid = 0;
    bus->bus_cmd = BUS_NO_CMD;
    bus->bus_addr = 0;
    bus->bus_data = 0;
    bus->bus_shared.D = 0;
    bus->bus_shared.Q = 0;
    bus->busy = false;
    bus->new_request = false;
    bus->flush_count = 0;
    bus->last_granted = 3;  // Start with core 3 as last granted
    bus->global_cycles = 0;
    bus->delay_in_progress = false;
    bus->delay_cycles = 0;
    bus->pending_cmd = BUS_NO_CMD;
    bus->pending_origid = 0;
    bus->pending_addr = 0;
    bus->pending_data = 0;

    bus->new_request = false;
    for (int i = 0; i < 4; i++) {
        bus->bus_request[i] = false;
        bus->bus_cmd_in[i] = BUS_NO_CMD;
        bus->bus_addr_in[i] = 0;
        bus->bus_data_in[i] = 0;
    }
}

void bus_request(bus_system_t* bus, int core_id, bus_cmd_t cmd, uint32_t addr, uint32_t data) {
    // Validate core ID
    if (core_id < 0 || core_id > 4) return;

    // Store request in core's request buffer
    bus->bus_request[core_id] = true;
    bus->bus_cmd_in[core_id] = cmd;
    bus->bus_addr_in[core_id] = addr;
    bus->bus_data_in[core_id] = data;
}

void bus_set_shared(bus_system_t* bus) {
    bus->bus_shared.D = 1;
}

void bus_clock(bus_system_t* bus) {
    // If in delay for RD/RDX request
    if (bus->delay_in_progress) {
        bus->delay_cycles--;
        if (bus->delay_cycles == 0) {
            // Delay complete - start actual transaction
            bus->delay_in_progress = false;
            bus->bus_cmd = bus->pending_cmd;
            bus->bus_origid = bus->pending_origid;
            bus->bus_addr = bus->pending_addr;
            bus->pending_addr = bus->pending_addr - bus->pending_addr % 4;
            bus->bus_data = bus->pending_data;
            bus->busy = true;
            bus->flush_count = 0;
            bus->new_request = true;
        }
        return;
    }

    // First priority: Handle FLUSH requests
    for (int current = 0; current < 5; current++) {
        if (bus->bus_request[current] && bus->bus_cmd_in[current] == BUS_FLUSH) {
            // Only process FLUSH if it's for the current block or bus is free
            if (bus->pending_addr != bus->bus_addr_in[current] && bus->busy) {
                continue;
            }
            bus->pending_addr++;

            // Process FLUSH immediately
            bus->bus_origid = current;
            bus->bus_cmd = BUS_FLUSH;
            bus->bus_addr = bus->bus_addr_in[current];
            bus->bus_data = bus->bus_data_in[current];
            bus->bus_request[current] = false;

            // Update block flush status
            if (bus->busy) {
                bus->flush_count++;
                if (bus->flush_count == WORDS_IN_BLOCK) {
                    bus->busy = false;
                    bus->bus_shared.D = 0;
                    bus->flush_count = 0;
                }
            }
            bus->new_request = true;
            return;
        }
    }

    // If bus is busy, can't handle new non-FLUSH requests
    if (bus->busy) {
        return;
    }

    // Handle non-FLUSH requests with round-robin arbitration
    int checked = 0;
    int current = (bus->last_granted + 1) % 4;

    while (checked < 4) {
        if (bus->bus_request[current] && bus->bus_cmd_in[current] != BUS_FLUSH) {
            // Start delay for new request
            bus->delay_in_progress = true;
            bus->delay_cycles = 1;  // Use original delay value
            bus->pending_cmd = bus->bus_cmd_in[current];
            bus->pending_origid = current;
            bus->pending_addr = bus->bus_addr_in[current];
            bus->pending_data = bus->bus_data_in[current];
            bus->bus_request[current] = false;
            bus->last_granted = current;
            return;
        }
        current = (current + 1) % 4;
        checked++;
    }

    // No requests - bus goes idle
    bus->bus_cmd = BUS_NO_CMD;
    bus->new_request = false;
}