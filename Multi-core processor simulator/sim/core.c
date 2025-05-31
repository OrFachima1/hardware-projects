// core.c
#include "core.h"
#include <stdio.h>


void core_init(core_t* core, int id) {
    core->core_id = id;
    register_init(&core->pc);
    register_set_next(&core->pc, 0);
    core->halted = false;
    pipeline_regs_init(&core->pipe);
    cache_init(&core->cache, id);

    for (int i = 0; i < 16; i++) {
        register_init(&core->registers[i]);
    }
    register_set_next(&core->registers[0], 0);  // R0 always 0

    core->cycles = 0;
    core->instructions = 0;
    core->decode_stalls = 0;
    core->mem_stalls = 0;
    core->pc_updated_by_branch = false;
}

/**
 * @brief Writeback stage of the pipeline
 *
 * Responsibilities:
 * 1. Write results back to register file
 * 2. Skip writes to R0 and R1
 */
void core_writeback(core_t* core) {
    // Skip if stage contains NOP
    if (core->pipe.mem_wb.pc.Q == -1) {
        return;
    }

    // Write result to register file if:
    // 1. Write is enabled
    // 2. Target is not R0 (constant 0)
    // 3. Target is not R1 (immediate value)
    if (core->pipe.mem_wb.write_reg &&
        core->pipe.mem_wb.rd.Q > 1) {  // Combines both R0 and R1 checks
        register_set_next(&core->registers[core->pipe.mem_wb.rd.Q],
            core->pipe.mem_wb.write_data.Q);
    }
}

/**
 * @brief Handle pipeline stall on cache miss
 */
static inline void handle_cache_miss(core_t* core) {
    core->mem_stalls++;
    for (int i = 0; i <= 2; i++) {  // Stall IF, ID, EX stages
        pipeline_stall_stage(&core->pipe, i);
    }
    core->pc.enable = false;
}

/**
 * @brief Memory stage of the pipeline
 * @param core Pointer to core structure
 * @param bus Pointer to bus system
 *
 * Handles:
 * - Memory read operations (lw)
 * - Memory write operations (sw)
 * - Pipeline stalls on cache misses
 * - Forwarding results to writeback stage
 */
void core_memory(core_t* core, bus_system_t* bus) {
    // Skip if stage contains NOP
    if (core->pipe.ex_mem.pc.Q == -1) {
        register_set_next(&core->pipe.mem_wb.pc, -1);
        return;
    }

    // Default to NOP for next stage
    register_set_next(&core->pipe.mem_wb.pc, -1);

    bool ready = true;
    uint32_t store_data = core->pipe.ex_mem.rd.Q;

    // Handle memory operations
    if (core->pipe.ex_mem.is_mem_read || core->pipe.ex_mem.is_mem_write) {
        if (core->pipe.ex_mem.is_mem_read) {
            uint32_t data;
            cache_read(&core->cache, bus,
                core->pipe.ex_mem.mem_addr.Q, &data, &ready);
            if (ready) {
                register_set_next(&core->pipe.mem_wb.write_data, data);
            }
        }
        else {
            cache_write(&core->cache, bus,
                core->pipe.ex_mem.mem_addr.Q,
                store_data, &ready);
        }

        if (!ready) {
            handle_cache_miss(core);
            return;
        }
    }

    // Forward to WB stage (no stall)
    register_set_next(&core->pipe.mem_wb.pc, core->pipe.ex_mem.pc.Q);
    register_set_next(&core->pipe.mem_wb.rd, core->pipe.ex_mem.rd.Q);
    register_set_next(&core->pipe.mem_wb.write_data,
        core->pipe.ex_mem.is_mem_read ?
        core->pipe.mem_wb.write_data.D :  // Keep the data from cache read
        core->pipe.ex_mem.alu_result.Q);  // Use ALU result for non-read ops

    // Set control signals once
    core->pipe.mem_wb.write_reg = core->pipe.ex_mem.write_reg;
}

/**
 * @brief Execute stage of the pipeline
 * @param core Pointer to core structure
 *
 * Performs ALU operations and address calculations for memory instructions.
 * State changes:
 * - Updates EX/MEM pipeline registers
 * - Sets control signals for memory operations
 */
void core_execute(core_t* core) {
    // Skip if stage contains NOP
    if (core->pipe.id_ex.pc.Q == -1) {
        register_set_next(&core->pipe.ex_mem.pc, -1);
        return;
    }

    uint32_t result = 0;
    uint8_t op = core->pipe.id_ex.opcode.Q;

    // Execute operation based on opcode
    if (op <= 8) {  // ALU operations (add, sub, and, or, xor, mul, sll, sra, srl)
        result = alu_execute(op, core->pipe.id_ex.rs_value.Q, core->pipe.id_ex.rt_value.Q);
    }
    else if (op == 16 || op == 17) {  // Memory operations (lw, sw)
        result = core->pipe.id_ex.rs_value.Q + core->pipe.id_ex.rt_value.Q;
    }

    // Forward results to MEM stage
    register_set_next(&core->pipe.ex_mem.pc, core->pipe.id_ex.pc.Q);
    register_set_next(&core->pipe.ex_mem.alu_result, result);
    register_set_next(&core->pipe.ex_mem.rd, core->pipe.id_ex.rd.Q);

    // Set control signals
    core->pipe.ex_mem.is_mem_read = (op == 16);   // lw
    core->pipe.ex_mem.is_mem_write = (op == 17);  // sw
    core->pipe.ex_mem.write_reg = (op <= 15);     // All non-memory operations write to register

    // For store operations, save the data to be stored
    if (core->pipe.ex_mem.is_mem_write) {
        register_set_next(&core->pipe.ex_mem.mem_write_data, core->pipe.id_ex.rd.Q);
    }

    register_set_next(&core->pipe.ex_mem.mem_addr, result);
    core->pipe.ex_mem.write_reg = core->pipe.id_ex.write_reg;
}

/**
 * @brief Check if register is involved in hazard
 * @param reg Register to check
 * @param dest Destination register to compare against
 * @return true if hazard exists
 */
static bool is_hazard_reg(uint8_t reg, uint8_t dest) {
    return (reg != 0 && reg != 1 && reg == dest);
}

/**
 * @brief Check for data hazards with a pipeline stage
 * @param rs Source register 1
 * @param rt Source register 2
 * @param rd Destination register (for sw)
 * @param opcode Current instruction opcode
 * @param stage_rd Destination register in pipeline stage
 * @param stage_write_reg Whether stage writes to register
 * @param stage_valid Whether stage contains valid instruction
 * @return true if hazard exists
 */
static bool check_hazard_stage(uint8_t rs, uint8_t rt, uint8_t rd, uint8_t opcode,
    uint8_t stage_rd, bool stage_write_reg, bool stage_valid) {
    if (!stage_write_reg || !stage_valid) return false;

    if (is_hazard_reg(rs, stage_rd) || is_hazard_reg(rt, stage_rd)) return true;

    // Additional check for sw instruction
    if (opcode == 17 && is_hazard_reg(rd, stage_rd)) return true;

    return false;
}

/**
 * @brief Check for all data hazards in pipeline
 */
static bool check_data_hazards(core_t* core, uint8_t opcode, uint8_t rs, uint8_t rt, uint8_t rd) {
    // Only check for hazards in relevant instructions
    if ((opcode > 14) && (opcode != 16) && (opcode != 17)) return false;

    // Check each pipeline stage
    if (check_hazard_stage(rs, rt, rd, opcode,
        core->pipe.id_ex.rd.Q,
        core->pipe.id_ex.write_reg,
        core->pipe.id_ex.pc.Q != -1)) return true;

    if (check_hazard_stage(rs, rt, rd, opcode,
        core->pipe.ex_mem.rd.Q,
        core->pipe.ex_mem.write_reg,
        core->pipe.ex_mem.pc.Q != -1)) return true;

    if (check_hazard_stage(rs, rt, rd, opcode,
        core->pipe.mem_wb.rd.Q,
        core->pipe.mem_wb.write_reg,
        core->pipe.mem_wb.pc.Q != -1)) return true;

    return false;
}
/**
 * @brief Evaluate branch condition
 */
static bool evaluate_branch(uint8_t opcode, uint32_t rs_val, uint32_t rt_val) {
    switch (opcode) {
    case 9:  return rs_val == rt_val;                          // beq
    case 10: return rs_val != rt_val;                          // bne
    case 11: return (int32_t)rs_val < (int32_t)rt_val;        // blt
    case 12: return (int32_t)rs_val > (int32_t)rt_val;        // bgt
    case 13: return (int32_t)rs_val <= (int32_t)rt_val;       // ble
    case 14: return (int32_t)rs_val >= (int32_t)rt_val;       // bge
    default: return false;
    }
}

/**
 * @brief Handle pipeline stall
 */
static void stall_pipeline(core_t* core) {
    register_set_next(&core->pipe.id_ex.pc, -1);
    pipeline_stall_stage(&core->pipe, 0);
    core->pc.enable = false;
    core->decode_stalls++;
}

void core_decode(core_t* core) {
    // Skip if stage contains NOP
    if (core->pipe.if_id.pc.Q == -1) {
        register_set_next(&core->pipe.id_ex.pc, -1);
        return;
    }

    // Decode instruction fields
    uint32_t instruction = core->pipe.if_id.instruction.Q;
    uint8_t opcode = (instruction >> 24) & 0xFF;
    uint8_t rd = (instruction >> 20) & 0xF;
    uint8_t rs = (instruction >> 16) & 0xF;
    uint8_t rt = (instruction >> 12) & 0xF;
    uint16_t immediate = instruction & 0xFFF;

    // Update R1 with sign-extended immediate
    core->registers[1].Q = (int16_t)immediate;

    // Check for data hazards
    if (check_data_hazards(core, opcode, rs, rt, rd)) {
        stall_pipeline(core);
        return;
    }

    // Handle control instructions
    if (opcode >= 9 && opcode <= 14) {  // Branch instructions
        uint32_t rs_val = register_get_value(&core->registers[rs]);
        uint32_t rt_val = register_get_value(&core->registers[rt]);

        if (evaluate_branch(opcode, rs_val, rt_val)) {
            uint32_t target = core->registers[rd].Q & 0x3FF;
            register_set_next(&core->pc, target);
            core->pc_updated_by_branch = true;
        }
    }
    else if (opcode == 15) {  // jal
        register_set_next(&core->registers[15], core->pc.Q + 1);
        register_set_next(&core->pc, rd & 0x3FF);
        core->pc_updated_by_branch = true;
    }
    else if (opcode == 20) {  // halt
        core->halted = true;
    }

    // Forward to EX stage
    register_set_next(&core->pipe.id_ex.pc, core->pipe.if_id.pc.Q);
    register_set_next(&core->pipe.id_ex.opcode, opcode);
    register_set_next(&core->pipe.id_ex.rd, opcode == 17 ? core->registers[rd].Q : rd);
    register_set_next(&core->pipe.id_ex.rs, rs);
    register_set_next(&core->pipe.id_ex.rt, rt);
    register_set_next(&core->pipe.id_ex.rs_value, register_get_value(&core->registers[rs]));
    register_set_next(&core->pipe.id_ex.rt_value, register_get_value(&core->registers[rt]));
    register_set_next(&core->pipe.id_ex.immediate, immediate);

    // Set write_reg for non-branch, non-store instructions
    core->pipe.id_ex.write_reg = (opcode <= 16) && (opcode < 9 || opcode > 14);
}
/**
 * @brief Fetch stage of the pipeline
 *
 * Responsibilities:
 * 1. Fetch next instruction from IMEM
 * 2. Update PC (if not modified by branch)
 * 3. Track instruction count
 */
void core_fetch(core_t* core) {
    if (core->halted) {
        register_set_next(&core->pipe.if_id.pc, -1);
        core->pc.D = -1;
        return;
    }

    // Count valid instruction fetch
    if (core->pc.enable) {
        core->instructions++;
    }

    const uint32_t curr_pc = core->pc.Q;

    // Fetch and forward instruction in one step
    register_set_next(&core->pipe.if_id.instruction, core->imem[curr_pc]);
    register_set_next(&core->pipe.if_id.pc, curr_pc);

    // Update PC unless modified by branch
    if (!core->pc_updated_by_branch) {
        register_set_next(&core->pc, curr_pc + 1);
    }
    core->pc_updated_by_branch = false;
}

void core_load_imem(core_t* core, const char* filename) {
    FILE* f = fopen(filename, "r");
    if (!f) return;

    int addr = 0;
    uint32_t value;
    while (addr < 1024 && fscanf(f, "%x", &value) == 1) {
        core->imem[addr++] = value;
    }
    fclose(f);
}
void print_core_state(core_t* core) {
    printf("\n=== Core %d State (Cycle %d) ===\n", core->core_id, core->cycles);

    // PC and Halt state
    printf("PC: %08X  Halted: %d\n", core->pc.Q, core->halted);

    // Registers (non-zero only)
    printf("\nRegisters:\n");
    for (int i = 2; i < 16; i++) {
        if (core->registers[i].Q != 0) {
            printf("R%d: %08X  ", i, core->registers[i].Q);
            if ((i - 1) % 4 == 0) printf("\n");
        }
    }

    // Pipeline stages
    printf("\nPipeline:\n");
    printf("IF/ID:  PC=%03X  Inst=%08X  Valid=%d\n",
        core->pipe.if_id.pc.Q,
        core->pipe.if_id.instruction.Q,
        core->pipe.if_id.valid);

    printf("ID/EX:  PC=%03X  Op=%02X  rd=%d  rs=%d  rt=%d  Valid=%d\n",
        core->pipe.id_ex.pc.Q,
        core->pipe.id_ex.opcode.Q,
        core->pipe.id_ex.rd.Q,
        core->pipe.id_ex.rs.Q,
        core->pipe.id_ex.rt.Q,
        core->pipe.id_ex.valid);

    printf("EX/MEM: PC=%03X  Rd=%d  Addr=%08X  Data=%08X  Valid=%d\n",
        core->pipe.ex_mem.pc.Q,
        core->pipe.ex_mem.rd.Q,
        core->pipe.ex_mem.mem_addr.Q,
        core->pipe.ex_mem.mem_write_data.Q,
        core->pipe.ex_mem.valid);

    printf("MEM/WB: PC=%03X  Rd=%d  Data=%08X  Valid=%d\n",
        core->pipe.mem_wb.pc.Q,
        core->pipe.mem_wb.rd.Q,
        core->pipe.mem_wb.write_data.Q,
        core->pipe.mem_wb.valid);

    printf("==========================================\n");
}


/**
 * @brief Update core state for next clock cycle
 *
 * @param core Pointer to core structure
 * @param bus Pointer to bus system
 */
void core_clock(core_t* core, bus_system_t* bus) {
    // Enable all registers for next cycle
    pipeline_regs_enable_all(&core->pipe);
    core->pc.enable = true;

    // Execute pipeline stages
    core_run_pipeline(core, bus);

    // Update all registers
    pipeline_regs_clock_update(&core->pipe);
    register_clock_update(&core->pc);
    for (int i = 0; i < 16; i++) {
        register_clock_update(&core->registers[i]);
    }
}

bool pipeline_is_empty(Pipeline_Regs* pipe) {
    return pipe->if_id.pc.Q == -1 &&
        pipe->id_ex.pc.Q == -1 &&
        pipe->ex_mem.pc.Q == -1 &&
        pipe->mem_wb.pc.Q == -1;
}

void core_run_pipeline(core_t* core, bus_system_t* bus) {
    // Execute pipeline stages in reverse order
    core_writeback(core);
    core_memory(core, bus);
    if (core->pipe.id_ex.rd.enable) {
        core_execute(core);
        core_decode(core);
        core_fetch(core);
    }
    // Update cycle count and statistics
    if (!core->halted || !pipeline_is_empty(&core->pipe)) {
        core->cycles++;
    }
}