// pipeline_regs.c
#include "pipeline_regs.h"

void pipeline_regs_init(Pipeline_Regs* regs) {
    // Initialize IF/ID
    register_init(&regs->if_id.pc);
    register_init(&regs->if_id.instruction);
    regs->if_id.valid = false;

    // Initialize ID/EX
    register_init(&regs->id_ex.pc);
    register_init(&regs->id_ex.opcode);
    register_init(&regs->id_ex.rd);
    register_init(&regs->id_ex.rs);
    register_init(&regs->id_ex.rt);
    register_init(&regs->id_ex.rs_value);
    register_init(&regs->id_ex.rt_value);
    register_init(&regs->id_ex.immediate);
    regs->id_ex.valid = false;
    regs->id_ex.is_mem_access = false;

    // Initialize EX/MEM
    register_init(&regs->ex_mem.pc);
    register_init(&regs->ex_mem.alu_result);
    register_init(&regs->ex_mem.rd);
    register_init(&regs->ex_mem.mem_addr);
    register_init(&regs->ex_mem.mem_write_data);
    regs->ex_mem.valid = false;
    regs->ex_mem.is_mem_read = false;
    regs->ex_mem.is_mem_write = false;
    regs->ex_mem.write_reg = false;

    // Initialize MEM/WB
    register_init(&regs->mem_wb.pc);
    register_init(&regs->mem_wb.write_data);
    register_init(&regs->mem_wb.rd);
    regs->mem_wb.valid = false;
    regs->mem_wb.write_reg = false;

    register_set_next(&regs->if_id.pc, -1);
    register_set_next(&regs->id_ex.pc, -1);
    register_set_next(&regs->ex_mem.pc, -1);
    register_set_next(&regs->mem_wb.pc, -1);
    regs->if_id.pc.Q = -1;
    regs->id_ex.pc.Q = -1;
    regs->ex_mem.pc.Q = -1;
    regs->mem_wb.pc.Q = -1;
}

void pipeline_regs_enable_all(Pipeline_Regs* regs) {
    // IF/ID stage
    register_set_enable(&regs->if_id.pc, true);
    register_set_enable(&regs->if_id.instruction, true);

    // ID/EX stage
    register_set_enable(&regs->id_ex.pc, true);
    register_set_enable(&regs->id_ex.opcode, true);
    register_set_enable(&regs->id_ex.rd, true);
    register_set_enable(&regs->id_ex.rs, true);
    register_set_enable(&regs->id_ex.rt, true);
    register_set_enable(&regs->id_ex.rs_value, true);
    register_set_enable(&regs->id_ex.rt_value, true);
    register_set_enable(&regs->id_ex.immediate, true);

    // EX/MEM stage
    register_set_enable(&regs->ex_mem.pc, true);
    register_set_enable(&regs->ex_mem.alu_result, true);
    register_set_enable(&regs->ex_mem.rd, true);
    register_set_enable(&regs->ex_mem.mem_addr, true);
    register_set_enable(&regs->ex_mem.mem_write_data, true);

    // MEM/WB stage
    register_set_enable(&regs->mem_wb.pc, true);
    register_set_enable(&regs->mem_wb.write_data, true);
    register_set_enable(&regs->mem_wb.rd, true);
}

void pipeline_regs_clock_update(Pipeline_Regs* regs) {
    // Update IF/ID
    register_clock_update(&regs->if_id.pc);
    register_clock_update(&regs->if_id.instruction);

    // Update ID/EX
    register_clock_update(&regs->id_ex.pc);
    register_clock_update(&regs->id_ex.opcode);
    register_clock_update(&regs->id_ex.rd);
    register_clock_update(&regs->id_ex.rs);
    register_clock_update(&regs->id_ex.rt);
    register_clock_update(&regs->id_ex.rs_value);
    register_clock_update(&regs->id_ex.rt_value);
    register_clock_update(&regs->id_ex.immediate);

    // Update EX/MEM
    register_clock_update(&regs->ex_mem.pc);
    register_clock_update(&regs->ex_mem.alu_result);
    register_clock_update(&regs->ex_mem.rd);
    register_clock_update(&regs->ex_mem.mem_addr);
    register_clock_update(&regs->ex_mem.mem_write_data);

    // Update MEM/WB
    register_clock_update(&regs->mem_wb.pc);
    register_clock_update(&regs->mem_wb.write_data);
    register_clock_update(&regs->mem_wb.rd);
}

void pipeline_stall_stage(Pipeline_Regs* regs, int stage) {
    switch (stage) {
    case 0: // IF/ID
        register_set_enable(&regs->if_id.pc, false);
        register_set_enable(&regs->if_id.instruction, false);
        break;
    case 1: // ID/EX
        register_set_enable(&regs->id_ex.pc, false);
        register_set_enable(&regs->id_ex.opcode, false);
        register_set_enable(&regs->id_ex.rd, false);
        register_set_enable(&regs->id_ex.rs, false);
        register_set_enable(&regs->id_ex.rt, false);
        register_set_enable(&regs->id_ex.rs_value, false);
        register_set_enable(&regs->id_ex.rt_value, false);
        register_set_enable(&regs->id_ex.immediate, false);
        break;
    case 2: // EX/MEM
        register_set_enable(&regs->ex_mem.pc, false);
        register_set_enable(&regs->ex_mem.alu_result, false);
        register_set_enable(&regs->ex_mem.rd, false);
        register_set_enable(&regs->ex_mem.mem_addr, false);
        register_set_enable(&regs->ex_mem.mem_write_data, false);
        break;
    case 3: // MEM/WB
        register_set_enable(&regs->mem_wb.pc, false);
        register_set_enable(&regs->mem_wb.write_data, false);
        register_set_enable(&regs->mem_wb.rd, false);
        break;
    }
}