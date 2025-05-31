/**
 * @file core.h
 * @brief Implementation of a pipelined processor core
 *
 * This core implements:
 * - 5-stage pipeline (Fetch, Decode, Execute, Memory, Writeback)
 * - 16 32-bit registers (R0-R15)
 * - Private instruction memory (1024 words)
 * - Private data cache with MESI coherency
 * - Support for data hazards and pipeline stalls
 */

#ifndef CORE_H
#define CORE_H

#include "pipeline_regs.h"
#include "cache.h"
#include "alu.h"

 /**
  * @brief Main processor core structure
  */
typedef struct {
    /* Pipeline Components */
    Pipeline_Regs pipe;    ///< Pipeline registers between stages
    Register registers[16]; ///< Register file (R0-R15)
    Register pc;           ///< Program counter 

    /* Memory Components */
    uint32_t imem[1024];   ///< Private instruction memory
    cache_t cache;         ///< Private data cache

    /* Core State */
    bool halted;                 ///< Core has reached halt instruction
    int core_id;                ///< Core identifier (0-3)
    bool pc_updated_by_branch;  ///< PC was modified by branch instruction

    /* Performance Counters */
    int cycles;          ///< Total execution cycles
    int instructions;    ///< Total instructions executed
    int decode_stalls;   ///< Stalls due to data hazards
    int mem_stalls;      ///< Stalls due to cache misses
} core_t;

/* Core Initialization and Control */
/**
 * @brief Initialize a processor core
 * @param core Pointer to core structure
 * @param id Core identifier (0-3)
 */
void core_init(core_t* core, int id);

/**
 * @brief Perform one clock cycle of core execution
 * @param core Pointer to core structure
 * @param bus Pointer to system bus
 */
void core_clock(core_t* core, bus_system_t* bus);

/**
 * @brief Load instruction memory from file
 * @param core Pointer to core structure
 * @param filename Name of file containing instructions
 */
void core_load_imem(core_t* core, const char* filename);

/* Pipeline Stage Functions */
/**
 * @brief Instruction fetch stage
 * @param core Pointer to core structure
 *
 * Fetches next instruction from IMEM and updates PC
 */
void core_fetch(core_t* core);

/**
 * @brief Instruction decode stage
 * @param core Pointer to core structure
 *
 * Decodes instruction, reads registers, and handles data hazards
 */
void core_decode(core_t* core);

/**
 * @brief Execute stage
 * @param core Pointer to core structure
 *
 * Performs ALU operations and address calculations
 */
void core_execute(core_t* core);

/**
 * @brief Memory stage
 * @param core Pointer to core structure
 * @param bus Pointer to system bus
 *
 * Handles memory operations through cache
 */
void core_memory(core_t* core, bus_system_t* bus);

/**
 * @brief Writeback stage
 * @param core Pointer to core structure
 *
 * Writes results back to register file
 */
void core_writeback(core_t* core);

/* Pipeline Control */
/**
 * @brief Check if pipeline is empty
 * @param pipe Pointer to pipeline registers
 * @return true if all stages contain NOPs
 */
bool pipeline_is_empty(Pipeline_Regs* pipe);

/**
 * @brief Execute one cycle of all pipeline stages
 * @param core Pointer to core structure
 * @param bus Pointer to system bus
 */
void core_run_pipeline(core_t* core, bus_system_t* bus);

#endif