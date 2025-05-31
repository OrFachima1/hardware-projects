/**
 * @file main.c
 * @brief Main simulation control for multi-core processor
 *
 * Implements a cycle-accurate simulator for a 4-core processor system with:
 * - Shared memory architecture
 * - MESI cache coherency
 * - Pipelined cores
 * - Trace generation
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include "core.h"
#include "bus_system.h"
#include "main_memory.h"

 /* Helper Functions */

 /**
  * @brief Format PC value for trace output
  * @param buffer Output buffer
  * @param pc Program counter value
  */
void format_pc(char* buffer, int pc) {
    if (pc == -1) {
        sprintf(buffer, "---");
    }
    else {
        sprintf(buffer, "%03X", pc);
    }
}

/**
 * @brief Write core execution trace to file
 * @param trace_file Output trace file
 * @param core Processor core
 */
void write_core_trace(FILE* trace_file, core_t* core) {
    char fetch[4], decode[4], execute[4], mem[4], wb[4];

    // Format pipeline stage PCs
    format_pc(fetch, core->pc.Q);
    format_pc(decode, core->pipe.if_id.pc.Q);
    format_pc(execute, core->pipe.id_ex.pc.Q);
    format_pc(mem, core->pipe.ex_mem.pc.Q);
    format_pc(wb, core->pipe.mem_wb.pc.Q);

    // Write formatted trace line
    fprintf(trace_file, "%d %s %s %s %s %s",
        core->cycles, fetch, decode, execute, mem, wb);

    // Write register values
    for (int r = 2; r < 16; r++) {
        fprintf(trace_file, " %08X", register_get_value(&core->registers[r]));
    }
    fprintf(trace_file, "\n");
}

/* File I/O Functions */

/**
 * @brief Load instruction memory files for all cores
 * @param cores Array of processor cores
 * @param files Array of IMEM filenames
 * @return true if successful, false on error
 */
bool load_imem_files(core_t cores[4], const char* files[4]) {
    for (int i = 0; i < 4; i++) {
        FILE* f = fopen(files[i], "r");
        if (!f) {
            printf("Error: Failed to open IMEM file %s\n", files[i]);
            return false;
        }

        int addr = 0;
        uint32_t value;
        while (addr < 1024 && fscanf(f, "%x", &value) == 1) {
            cores[i].imem[addr++] = value;
        }
        fclose(f);
    }
    return true;
}

/**
 * @brief Save final register states
 * @param cores Array of processor cores
 * @param files Array of output filenames
 * @return true if successful, false on error
 */
bool save_register_states(core_t cores[4], const char* files[4]) {
    for (int i = 0; i < 4; i++) {
        FILE* f = fopen(files[i], "w");
        if (!f) {
            printf("Error: Failed to open register output file %s\n", files[i]);
            return false;
        }

        for (int r = 2; r < 16; r++) {
            fprintf(f, "%08X\n", register_get_value(&cores[i].registers[r]));
        }
        fclose(f);
    }
    return true;
}

/**
 * @brief Save cache states (DSRAM and TSRAM)
 * @param cores Array of processor cores
 * @param dsram_files Array of DSRAM output filenames
 * @param tsram_files Array of TSRAM output filenames
 * @return true if successful, false on error
 */
bool save_cache_states(core_t cores[4], const char* dsram_files[4], const char* tsram_files[4]) {
    for (int i = 0; i < 4; i++) {
        // Save DSRAM
        FILE* dsram = fopen(dsram_files[i], "w");
        if (!dsram) {
            printf("Error: Failed to open DSRAM output file %s\n", dsram_files[i]);
            return false;
        }

        for (int j = 0; j < CACHE_SIZE; j++) {
            fprintf(dsram, "%08X\n", cores[i].cache.dsram[j]);
        }
        fclose(dsram);

        // Save TSRAM
        FILE* tsram = fopen(tsram_files[i], "w");
        if (!tsram) {
            printf("Error: Failed to open TSRAM output file %s\n", tsram_files[i]);
            return false;
        }

        for (int j = 0; j < NUM_SETS; j++) {
            uint32_t entry = (cores[i].cache.tsram[j].state << 12) |
                cores[i].cache.tsram[j].tag;
            fprintf(tsram, "%08X\n", entry);
        }
        fclose(tsram);
    }
    return true;
}

/**
 * @brief Save execution statistics
 * @param cores Array of processor cores
 * @param files Array of statistics output filenames
 * @return true if successful, false on error
 */
bool save_statistics(core_t cores[4], const char* files[4]) {
    for (int i = 0; i < 4; i++) {
        FILE* f = fopen(files[i], "w");
        if (!f) {
            printf("Error: Failed to open statistics file %s\n", files[i]);
            return false;
        }

        fprintf(f, "cycles %d\n", cores[i].cycles);
        fprintf(f, "instructions %d\n", cores[i].instructions);
        fprintf(f, "read_hit %d\n", cores[i].cache.read_hit);
        fprintf(f, "write_hit %d\n", cores[i].cache.write_hit);
        fprintf(f, "read_miss %d\n", cores[i].cache.read_miss);
        fprintf(f, "write_miss %d\n", cores[i].cache.write_miss);
        fprintf(f, "decode_stall %d\n", cores[i].decode_stalls);
        fprintf(f, "mem_stall %d\n", cores[i].mem_stalls);

        fclose(f);
    }
    return true;
}

int main(int argc, char* argv[]) {
    // Define default filenames if not provided as arguments
    const char* default_files[] = {
        "imem0.txt", "imem1.txt", "imem2.txt", "imem3.txt",     // IMEM (0-3)
        "memin.txt", "memout.txt",                               // Memory (4-5)
        "regout0.txt", "regout1.txt", "regout2.txt", "regout3.txt", // Registers (6-9)
        "core0trace.txt", "core1trace.txt", "core2trace.txt", "core3trace.txt", // Traces (10-13)
        "bustrace.txt",                                          // Bus trace (14)
        "dsram0.txt", "dsram1.txt", "dsram2.txt", "dsram3.txt", // DSRAM (15-18)
        "tsram0.txt", "tsram1.txt", "tsram2.txt", "tsram3.txt", // TSRAM (19-22)
        "stats0.txt", "stats1.txt", "stats2.txt", "stats3.txt"  // Statistics (23-26)
    };

    const char** files = (argc == 28) ? (const char**)(argv + 1) : default_files;

    // Open trace files
    FILE* core_trace_files[4];
    for (int i = 0; i < 4; i++) {
        core_trace_files[i] = fopen(files[i + 10], "w");
        if (!core_trace_files[i]) {
            printf("Error: Failed to open core trace file %s\n", files[i + 10]);
            return 1;
        }
    }

    FILE* bus_trace = fopen(files[14], "w");
    if (!bus_trace) {
        printf("Error: Failed to open bus trace file %s\n", files[14]);
        return 1;
    }

    // Initialize system components
    bus_system_t* bus = (bus_system_t*)malloc(sizeof(bus_system_t));
    main_memory_t* mem = (main_memory_t*)malloc(sizeof(main_memory_t));
    core_t* cores = (core_t*)malloc(4 * sizeof(core_t));

    if (!bus || !mem || !cores) {
        printf("Error: Memory allocation failed\n");
        return 1;
    }

    // Initialize components
    bus_init(bus);
    memory_init(mem);
    memory_load(mem, files[4]);

    for (int i = 0; i < 4; i++) {
        core_init(&cores[i], i);
    }

    if (!load_imem_files(cores, files)) {
        return 1;
    }

    // Main simulation loop
    bool all_done;
    do {
        // 1. Memory checks bus and responds
        memory_clock(mem, bus);

        // 2. Update bus state
        bus_clock(bus);

        // 3. Run cache operations
        for (int i = 0; i < 4; i++) {
            cache_snoop(&cores[i].cache, bus);
            cache_handle_bus_response(&cores[i].cache, bus);
            cache_clock(&cores[i].cache, bus);
        }

        // 4. Run cores and log traces
        for (int i = 0; i < 4; i++) {
            if (!cores[i].halted || !pipeline_is_empty(&cores[i].pipe)) {
                write_core_trace(core_trace_files[i], &cores[i]);
            }
            core_clock(&cores[i], bus);
        }

        // Log bus activity
        if (bus->bus_cmd != BUS_NO_CMD && bus->new_request) {
            fprintf(bus_trace, "%d %d %d %05X %08X %d\n",
                bus->global_cycles,
                bus->bus_origid,
                bus->bus_cmd,
                bus->bus_addr,
                bus->bus_data,
                bus->bus_shared.Q);
            bus->new_request = false;
        }

        bus->bus_shared.Q = bus->bus_shared.D;
        bus->global_cycles++;

        // Check if all cores are done
        all_done = true;
        for (int i = 0; i < 4; i++) {
            all_done &= cores[i].halted && pipeline_is_empty(&cores[i].pipe);
        }
    } while (!all_done);

    // Save final states
    memory_save(mem, files[5]);
    save_register_states(cores, files + 6);
    save_cache_states(cores, files + 15, files + 19);
    save_statistics(cores, files + 23);

    // Cleanup
    for (int i = 0; i < 4; i++) {
        fclose(core_trace_files[i]);
    }
    fclose(bus_trace);

    free(cores);
    free(mem);
    free(bus);

    return 0;
}