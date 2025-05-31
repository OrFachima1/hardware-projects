// pipeline_regs.h
#ifndef PIPELINE_REGS_H
#define PIPELINE_REGS_H

#include <stdint.h>
#include <stdbool.h>
#include "register.h"

// IF/ID Register
typedef struct {
    Register pc;          // Program Counter
    Register instruction; // Full instruction
    bool valid;          // Valid bit
} IF_ID_Reg;

// ID/EX Register
typedef struct {
    Register pc;          // Program Counter
    Register opcode;      // Operation Code
    Register rd;          // Destination register
    Register rs;         // Source register 1
    Register rt;         // Source register 2
    Register rs_value;    // Value of rs
    Register rt_value;    // Value of rt
    Register immediate;   // Immediate field
    bool write_reg;
    bool valid;          // Valid bit
    bool is_mem_access;  // Does this instruction access memory
} ID_EX_Reg;

// EX/MEM Register
typedef struct {
    Register pc;           // Program Counter
    Register alu_result;   // ALU computation result
    Register rd;          // Destination register
    Register mem_addr;    // Memory address (if needed)
    Register mem_write_data; // Data to write to memory
    bool valid;           // Valid bit
    bool is_mem_read;     // Memory read operation
    bool is_mem_write;    // Memory write operation
    bool write_reg;       // Should write to register file
} EX_MEM_Reg;

// MEM/WB Register
typedef struct {
    Register pc;          // Program Counter
    Register write_data;  // Data to write back to register
    Register rd;         // Destination register
    bool valid;          // Valid bit
    bool write_reg;      // Should write to register file
} MEM_WB_Reg;

// Pipeline Registers
typedef struct {
    IF_ID_Reg if_id;
    ID_EX_Reg id_ex;
    EX_MEM_Reg ex_mem;
    MEM_WB_Reg mem_wb;
} Pipeline_Regs;

// Initialize pipeline registers
void pipeline_regs_init(Pipeline_Regs* regs);

// Clock update for all pipeline registers
void pipeline_regs_clock_update(Pipeline_Regs* regs);

// Stall specific pipeline stage
void pipeline_stall_stage(Pipeline_Regs* regs, int stage);

/**
 * @brief Enable all pipeline registers
 *
 * Sets enable signal for all registers in the pipeline to true.
 * This prepares the pipeline for the next clock cycle.
 *
 * @param regs Pointer to pipeline registers structure
 */
void pipeline_regs_enable_all(Pipeline_Regs* regs);

#endif // PIPELINE_REGS_H