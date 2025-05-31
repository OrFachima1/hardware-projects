/**
 * @file bus_system.h
 * @brief Implementation of the system bus with MESI coherency protocol support
 *
 * This bus system implements:
 * - Support for 4 processor cores plus main memory
 * - MESI coherency protocol commands (BusRd, BusRdX, Flush)
 * - Round-robin arbitration for bus access
 * - Shared line for cache-to-cache transfers
 */

#ifndef BUS_SYSTEM_H
#define BUS_SYSTEM_H

#include <stdint.h>
#include <stdbool.h>
#include "register.h"

 /**
  * @brief Bus commands for MESI protocol
  */
typedef enum {
    BUS_NO_CMD = 0,  ///< No command/bus idle
    BUS_RD = 1,      ///< Read request (S or E state)
    BUS_RDX = 2,     ///< Exclusive read request (M state)
    BUS_FLUSH = 3    ///< Write modified data back
} bus_cmd_t;

/**
 * @brief Main bus system structure
 */
typedef struct {
    /* Bus Command Lines */
    uint8_t bus_origid;      ///< Transaction originator (0-3: cores, 4: memory)
    bus_cmd_t bus_cmd;       ///< Current bus command
    uint32_t bus_addr;       ///< 20-bit address bus
    uint32_t bus_data;       ///< 32-bit data bus
    Register bus_shared;     ///< Shared line for cache-to-cache transfer
    bool new_request;        ///< Indicates new bus transaction

    /* System State */
    int global_cycles;       ///< Global clock counter
    bool delay_in_progress;  ///< Initial delay for bus operations
    int delay_cycles;        ///< Remaining delay cycles

    /* Request Lines (per core + memory) */
    bool bus_request[5];     ///< Bus request signals
    bus_cmd_t bus_cmd_in[5]; ///< Requested commands
    uint32_t bus_addr_in[5]; ///< Requested addresses
    uint32_t bus_data_in[5]; ///< Data to transfer

    /* Bus Control State */
    bool busy;              ///< Bus is processing a transaction
    uint32_t flush_count;   ///< Number of words flushed in current block
    uint8_t last_granted;   ///< Last core granted for round-robin

    /* Pending Transaction */
    bus_cmd_t pending_cmd;   ///< Command waiting for delay
    uint8_t pending_origid;  ///< Originator of pending command
    uint32_t pending_addr;   ///< Address of pending transaction
    uint32_t pending_data;   ///< Data for pending transaction
} bus_system_t;

/**
 * @brief Initialize the bus system
 * @param bus Pointer to bus system structure
 */
void bus_init(bus_system_t* bus);

/**
 * @brief Request bus access for a transaction
 * @param bus Pointer to bus system
 * @param core_id Requesting core (0-3) or memory (4)
 * @param cmd Bus command to execute
 * @param addr Target address
 * @param data Data to transfer (for writes)
 */
void bus_request(bus_system_t* bus, int core_id, bus_cmd_t cmd, uint32_t addr, uint32_t data);

/**
 * @brief Set shared line to indicate cache-to-cache transfer
 * @param bus Pointer to bus system
 */
void bus_set_shared(bus_system_t* bus);

/**
 * @brief Update bus state each clock cycle
 * @param bus Pointer to bus system
 *
 * This function:
 * 1. Handles FLUSH commands with highest priority
 * 2. Processes initial delay for new transactions
 * 3. Performs round-robin arbitration for new requests
 * 4. Updates bus state and signals
 */
void bus_clock(bus_system_t* bus);

#endif