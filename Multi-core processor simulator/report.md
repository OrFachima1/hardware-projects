# Multi-Core Processor Simulator Report

**Name:** Or Fachima  
 

## 1. Introduction
This report presents the design, implementation, and evaluation of a multi-core processor simulator. The simulator models a 4-core processor with a pipelined architecture and implements the MESI cache coherency protocol. The goal of the project is to simulate instruction execution, pipeline hazards, and memory interactions accurately.
## 1.2 Project Files and Execution  

The project consists of several source files, each handling a different aspect of the **multi-core processor simulator**:  

### **Source Files Overview**  

| **File**              | **Description** |
|----------------------|--------------------------------------------------|
| `alu.c`, `alu.h`     | Implements arithmetic and logic unit operations. |
| `bus.c`, `bus_system.h` | Handles bus transactions and arbitration logic. |
| `cache.c`, `cache.h` | Implements the **MESI-based cache system**. |
| `core.c`, `core.h`   | Defines core execution, including pipeline control. |
| `main.c`             | The main entry point for simulation execution. |
| `main_memory.c`, `main_memory.h` | Handles interactions with **main memory**. |
| `pipeline_regs.c`, `pipeline_regs.h` | Manages **pipeline registers** and state transitions. |
| `register.c`, `register.h` | Implements register file logic and updates. |

Additionally, the **`sim/` directory** contains compiled binaries and output logs generated during execution.

---

### **Executing the Simulator**  

The simulator can be executed in **two ways**:  
1. **With explicit arguments (manual input specification)**  
   ```sh
   sim.exe imem0.txt imem1.txt imem2.txt imem3.txt memin.txt memout.txt \
   regout0.txt regout1.txt regout2.txt regout3.txt core0trace.txt core1trace.txt \
   core2trace.txt core3trace.txt bustrace.txt dsram0.txt dsram1.txt dsram2.txt \
   dsram3.txt tsram0.txt tsram1.txt tsram2.txt tsram3.txt stats0.txt stats1.txt \
   stats2.txt stats3.txt
   ```

2. **Without arguments (default file names required)**
```sh
sim.exe
```
When no arguments are provided, the simulator assumes all input files (imem.txt, memin.txt) are present in the same directory and outputs results in the default locations.
Before running the simulator, ensure that instruction memory (imem.txt) and main memory. 
## 2. System Architecture


### 2.1 Core Structure
Each core is designed with a **5-stage pipeline**:
- **Fetch**: Retrieves the instruction from memory.
- **Decode**: Decodes the instruction and reads operands.
- **Execute**: Performs arithmetic or logic operations.
- **Memory Access**: Reads/writes data from/to memory.
- **Write Back**: Updates register values.

### 2.2 Memory and Cache Hierarchy
Each core has:
- A **private instruction memory** (SRAM).
- A **private data cache** with MESI protocol.
- A shared **main memory** connected via a system bus.

### 2.3 Bus and Cache Coherency

#### MESI Protocol
The MESI (Modified, Exclusive, Shared, Invalid) protocol ensures cache coherence in multi-core processors by coordinating memory access across cores.

**Read Operation (Cache Miss):**
- If a core attempts to read a block that is not in its cache, it issues a **BusRd** request on the bus.
- If another core holds the block in **M (Modified) state**, it must respond by performing a **Flush**, writing the modified data back to memory.
- If the block is present in **E (Exclusive) or S (Shared) state** in another cache, that core must assert **Bus Shared** on the bus, signaling that the data is shared and no longer exclusive. The main memory will then provide the data.
- Any core observing the **BusRd** request and holding the block updates its state to **S (Shared)**.

**Write Operation:**
- If a core wants to write to a block that it does not own, or it holds in **S (Shared) state**, it issues a **BusRdX** (Read-Exclusive) request.
- Any core holding this block must transition its state to **I (Invalid)**.
- If another core holds the block in **M (Modified) state**, it must first provide the updated data before invalidating its copy.
- Otherwise, the responsibility of supplying the data falls to the main memory.

This protocol ensures that multiple caches maintain data consistency while optimizing performance by reducing unnecessary memory accesses.
The **MESI protocol** ensures that cache states remain consistent across cores. The bus handles memory transactions using:
- **BusRd** – Read request.
- **BusRdX** – Read-exclusive request.
- **Flush** – Write-back modified cache data.

To support the MESI protocol, at the beginning of each clock cycle, before core execution, caches and the main memory perform a **bus snooping** operation to stay updated and respond accordingly.

Only one transaction can occur on the bus per cycle. If multiple caches request access simultaneously, **round-robin arbitration** is used to ensure fairness. 

For **read requests**, if another core holds the data in the **Modified** state, it flushes the updated value before the memory provides the data. If the data exists in the **Shared** or **Exclusive** state in another cache, the responding core asserts the **Bus Shared** signal, and the memory supplies the data.

For **write operations**, a core must ensure that all other copies of the block are marked as **Invalid** to maintain consistency. This approach minimizes memory access overhead while ensuring cache coherence.

## 3. Implementation Details

### 3.1 Core Implementation
The core implementation was designed to simulate real-world execution as accurately as possible. To achieve this, the following design choices were made:

- **Register Simulation (`register.c`)**: Each register is implemented with **D-Q flip-flop behavior** to accurately reflect register updates per clock cycle.
- **Pipeline Register Simulation (`pipeline_regs.c`)**: Pipeline registers maintain state between pipeline stages for improved modularity.
- **Instruction Forwarding Control**: Each register type includes an `enable` signal to determine whether forwarding occurs in the next clock cycle, optimizing stalls.
- **NOP Instruction Representation**: The special NOP instruction is encoded as `-1` in the program counter (`PC`) register.

Each core contains the following structure:

```c
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
```



### 3.2 Pipeline Hazards Handling

#### Handling Hazards:

**In the MEM Stage:**
- If a **cache miss** occurs, all earlier stages are stalled by disabling their `enable` signals.
- A **NOP instruction** is injected into the MEM/WB stage to maintain consistency.

**In the Decode Stage:**
- The values of **rt** and **rs** registers are compared against the **rd** register from later stages.
- A hazard is detected if:
  - The later stage writes to a register.
  - The register is not `R0` or `R1` (which should not be modified).
- If a hazard is detected, the **FETCH stage is stalled**, and a **NOP instruction** is injected into the next pipeline stage.
- For **SW (store word) instructions**, `rd` of the current stage is also checked.

**Branch Handling in the Decode Stage:**
- During decoding, the instruction is checked if it is a **branch**.
- If the branch is taken, the **correct target address is loaded into `PC.D`**.
- The FETCH stage is prevented from modifying the PC incorrectly.
- **Data Hazards**: Implemented stalls when dependencies exist.
- **Control Hazards**: Handled via branch resolution in the decode stage.
- **Memory Hazards**: Cache misses cause pipeline stalls until data retrieval.
## 3.3 Cache Implementation  

The cache follows a **direct-mapped structure** with a **write-back, write-allocate** policy. It operates using the **MESI coherency protocol**, ensuring consistency across cores.  

### Cache Operations  

- **`cache_read`**  
  This function handles read operations. If the requested block is **present** in the cache (hit), the data is returned immediately. Otherwise (miss), a **BusRd** request is issued. If another core holds the block in **Modified** state, it must first flush the data before the request is completed. Additionally, if a conflict occurs within the cache, the replacement policy updates the evicted block accordingly.  

- **`cache_write`**  
  The write function follows a similar process. If the block is not already in **Modified** or **Exclusive** state, a **BusRdX** request is sent to ensure other cores invalidate their copies. If the block was **Shared**, it transitions to **Modified** after invalidation.  

- **`cache_snoop`**  
  This function is executed at the **beginning of each clock cycle**, allowing the cache to monitor the bus and respond to relevant transactions. It updates states when a **BusRd**, **BusRdX**, or **Flush** occurs, ensuring coherence with other cores.  

- **`cache_handle_bus_response`**  
  Handles pending operations from previous cycles, such as processing a **flush** response, completing block replacements, or updating the bus status if a block was supplied by another core.  

## 3.4 Memory Behavior  

The main memory interacts with the bus to supply data when needed.  

- **`memory_clock`**  
  This function **snoops the bus** at the beginning of each clock cycle. If a core issues a read request (**BusRd**/**BusRdX**), memory starts a **16-cycle countdown** before responding with the requested block. However, if another core supplies the block (via a **flush** operation), the countdown is canceled.  

- **Flush Handling**  
  If a **flush operation** is initiated by a core, the memory updates its contents **immediately** without additional latency, ensuring consistency across the system.  

These mechanisms allow the simulator to maintain an efficient and consistent memory hierarchy while reducing unnecessary memory accesses.

## 3.5 Bus Management  

The system bus operates using a **round-robin (RR) arbitration scheme**, ensuring fair access among cores.  
For **BusRd** and **BusRdX** requests, the bus does not accept new transactions (except for **Flush** operations related to the same request) until the current request is completed.  

The bus introduces a **latency of 2 clock cycles** for standard transactions, while **Flush operations are processed immediately** without additional delay.  

## 4. Testing and Validation
Three test programs were executed to validate the simulator:
1. **Counter Test** – Incrementing memory values across cores.
2. **Serial Vector Addition** – Single-core vector addition.
3. **Parallel Vector Addition** – Multi-core synchronized vector computation.

Each test verified correctness by:
- Checking register outputs.
- Ensuring proper cache and memory updates.
- Measuring execution efficiency.
## 4.1 Counter Test  

Managing a shared counter across four cores was challenging due to simultaneous memory accesses. To handle this efficiently, the following approach was implemented:  

- Each core **loads the counter value** from memory and checks its **last two bits**.  
- If these bits match the core's **ID**, it is that core's turn to increment the counter.  
- After incrementing, the core stores the updated value back to memory.  
- Core **0** is responsible for **finalizing the counter update** in memory via a simple **LW request to address 1024**.  

### Test Program  

```assembly
add $r2, $r0, $r0, 0                        # Core ID
add $r3, $r1, $r0, 128                      # Amount of increments per core 
add $r4, $r0, $r1, 3                        # Initial mask 8'b11 
wait_for_core_turn:
    lw  $r5, $r0, $r0, 0                    # Load counter from memory 
    and $r6, $r4, $r5, 0                    # R[6] = R[4] & R[5] 
    bne $r1, $r6, $r2, wait_for_core_turn   # It's not our turn, keep waiting 
add $r5, $r5, $r1, 1                        # Counter++ 
sw $r5, $r0, $r0, 0                         # Update counter 
sub $r3, $r3, $r1, 1                        # Amount of increments per core-- 
bne $r1, $r3, $r0, wait_for_core_turn       # We didn't finish yet 
nop $r0, $r0, $r0, 0                        # Delay slot 
lw $r0, $r1, $r0, 1024                      # Force conflict miss
halt $r0, $r0, $r0, 0 
halt $r0, $r0, $r0, 0 
halt $r0, $r0, $r0, 0 
halt $r0, $r0, $r0, 0 
```
## 4.2 Serial Vector Addition (addserial)  

This program performs element-wise addition of two vectors (`vec1` and `vec2`), storing the result in `vec3`.  

### Optimization Considerations  

To minimize **cache misses**, loop unrolling was implemented. Since **all addresses in the loop iteration belong to the same cache block**, using multiple registers allows fetching an entire block from the cache before performing **replacement**.  

At the end of the computation, a **conflict enforcement loop** is added to ensure the memory updates properly by triggering **cache evictions**.  

### Test Program  

```assembly
add $r2, $r0, $r0, 0                        # R[2] = vec1_base address
add $r3, $r1, $r0, 16                       # R[3] = 1000 
sll $r3, $r3, $r1, 8                        # 8 << R[3] = 4096 = vec2_base address 
mul $r4, $r3, $r1, 2                        # R[4] = R[3] * 2 = 8192 = vec3_base address
add $r5, $r0, $r3, 0                        # Number of additions 

block_loop: 
    lw  $r6, $r2, $r1, 0                    # R[6] = vec1[i]    
    lw  $r7, $r2, $r1, 1                    # R[7] = vec1[i+1]
    lw  $r8, $r2, $r1, 2                    # R[8] = vec1[i+2]
    lw  $r9, $r2, $r1, 3                    # R[9] = vec1[i+3]
    lw  $r10,$r3, $r1, 0                    # R[10] = vec2[i]
    add $r6, $r6, $r10, 0                   # R[6] += R[10] 
    lw  $r10,$r3, $r1, 1                    # R[10] = vec2[i+1]
    add $r7, $r7, $r10, 0                   # R[7] += R[10] 
    lw  $r10,$r3, $r1, 2                    # R[10] = vec2[i+2]
    add $r8, $r8, $r10, 0                   # R[8] += R[10] 
    lw  $r10,$r3, $r1, 0                    # R[10] = vec2[i+3]
    add $r9, $r9, $r10, 4                   # R[9] += R[10] 
    sub $r5, $r5, $r1, 4                    # R[5] -= 4 
    sw  $r6, $r4, $r1, 0                    # vec3[i] = R[6] 
    sw  $r7, $r4, $r1, 1                    # vec3[i+1] = R[7] 
    sw  $r8, $r4, $r1, 2                    # vec3[i+2] = R[8] 
    sw  $r9, $r4, $r1, 3                    # vec3[i+3] = R[9] 
    add $r2, $r2, $r1, 4                    # i += 4 
    add $r3, $r3, $r1, 4 
    bne $r1, $r5, $r0, block_loop           # if R[5] != 0 

add $r4, $r4, $r1, 4                        # Delay slot 
add $r2, $r0, $r1, 256                      # R[2] = NUMBER OF BLOCKS IN CACHE
add $r3, $r0, $r0, 0                        # R[3] = 0  

update_loop:                                 # Loop to ensure memory update at the end 
    lw  $r0, $r3, $r0, 0                    # Enforce flush of block i 
    bne $r1, $r2, $r3, update_loop          # while R[2] != R[3]
    add $r3, $r3, $r1, 4                    # Delay slot - i forward to the next block 

halt  $r0, $r0, $r0, 0 
halt  $r0, $r0, $r0, 0 
halt  $r0, $r0, $r0, 0 
halt  $r0, $r0, $r0, 0 
```
## 4.3 Parallel Vector Addition (addparallel)  

To fully utilize multi-core execution, this program applies **embarrassingly parallel processing** by **dividing the vector addition workload into 1024-element segments**, allowing each core to work on a distinct portion of the data independently.  

### Optimization Considerations  

- **Loop Unrolling**: Similar to the serial implementation, **loop unrolling** was applied to reduce cache misses by ensuring each iteration loads and processes an entire **cache block** before eviction.  
- **Data Partitioning**: Each core operates on a separate segment of the vectors, eliminating **synchronization overhead** between cores.  

### Test Program  

```assembly
add $r2, $r0, $r0, 0                        # R[2] = vec1_base address
add $r3, $r1, $r0, 16                       # R[3] = 1000 
sll $r3, $r3, $r1, 8                        # 8 << R[3] = 4096 = vec2_base address 
mul $r4, $r3, $r1, 8                        # R[4] = R[3] * 2 = 8192 = vec3_base address
add $r5, $r0, $r3, 0                        # Number of additions 

block_loop: 
    lw  $r6, $r2, $r1, 0                    # R[6] = vec1[i]    
    lw  $r7, $r2, $r1, 1                    # R[7] = vec1[i+1]
    lw  $r8, $r2, $r1, 2                    # R[8] = vec1[i+2]
    lw  $r9, $r2, $r1, 3                    # R[9] = vec1[i+3]
    lw  $r10,$r3, $r1, 0                    # R[10] = vec2[i]
    add $r6, $r6, $r10, 0                   # R[6] += R[10] 
    lw  $r10,$r3, $r1, 1                    # R[10] = vec2[i+1]
    add $r7, $r7, $r10, 0                   # R[7] += R[10] 
    lw  $r10,$r3, $r1, 2                    # R[10] = vec2[i+2]
    add $r8, $r8, $r10, 0                   # R[8] += R[10] 
    lw  $r10,$r3, $r1, 0                    # R[10] = vec2[i+3]
    add $r9, $r9, $r10, 4                   # R[9] += R[10] 
    sub $r5, $r5, $r1, 4                    # R[5] -= 4 
    sw  $r6, $r4, $r1, 0                    # vec3[i] = R[6] 
    sw  $r7, $r4, $r1, 1                    # vec3[i+1] = R[7] 
    sw  $r8, $r4, $r1, 2                    # vec3[i+2] = R[8] 
    sw  $r9, $r4, $r1, 3                    # vec3[i+3] = R[9] 
    add $r2, $r2, $r1, 4                    # i += 4 
    add $r3, $r3, $r1, 4 
    bne $r1, $r5, $r0, block_loop           # if R[5] != 0 

add $r4, $r4, $r1, 4                        # Delay slot 
add $r2, $r0, $r1, 256                      # R[2] = NUMBER OF BLOCKS IN CACHE
add $r3, $r0, $r0, 0                        # R[3] = 0  

update_loop:                                 # Loop to ensure memory update at the end 
    lw  $r0, $r3, $r0, 0                    # Enforce flush of block i 
    bne $r1, $r2, $r3, update_loop          # while R[2] != R[3]
    add $r3, $r3, $r1, 4                    # Delay slot - i forward to the next block 

halt  $r0, $r0, $r0, 0 
halt  $r0, $r0, $r0, 0 
halt  $r0, $r0, $r0, 0 
halt  $r0, $r0, $r0, 0 
```

## 5. Performance Summary  

The following table summarizes the performance metrics for each test and highlights the impact of **parallel execution**.  

### **Counter Test (4 cores, sequential memory updates)**  

The **counter test** demonstrates the **challenges of synchronization**, as all cores attempt to update the same memory location. The results show that instead of improving performance, parallel execution **increased stalls and overall cycles**, leading to inefficient execution.  

| Core | Cycles | Instructions | Read Hits | Write Hits | Read Misses | Write Misses | Decode Stalls | Memory Stalls |
|------|--------|-------------|-----------|------------|-------------|--------------|---------------|---------------|
| 0    | 35235  | 5601        | 889       | 0          | 383         | 128          | 8394          | 21236         |
| 1    | 35262  | 5620        | 893       | 0          | 383         | 128          | 8424          | 21214         |
| 2    | 35310  | 5632        | 895       | 0          | 384         | 128          | 8442          | 21232         |
| 3    | 35350  | 5620        | 891       | 0          | 385         | 128          | 8424          | 21302         |

---

### **Serial Vector Addition (Single Core Execution)**  

The **serial vector addition** uses **loop unrolling** to optimize memory accesses and reduce cache misses. Since only one core is active, no synchronization is required, allowing more efficient memory usage.  

| Cycles  | Instructions | Read Hits | Write Hits | Read Misses | Write Misses | Decode Stalls | Memory Stalls |
|---------|-------------|-----------|------------|-------------|--------------|---------------|---------------|
| 104173  | 21707      | 6144      | 3072       | 6209        | 1024         | 12489         | 69973         |

---

### **Parallel Vector Addition (Each Core Handles a Separate Segment)**  

The **parallel vector addition** utilizes **embarrassingly parallel processing**, where each core independently processes a separate chunk of the vectors. This approach aims to maximize core utilization while minimizing cache conflicts.  

| Core | Cycles | Instructions | Read Hits | Write Hits | Read Misses | Write Misses | Decode Stalls | Memory Stalls |
|------|--------|-------------|-----------|------------|-------------|--------------|---------------|---------------|
| 0    | 74024  | 5579        | 1536      | 768        | 5302        | 256          | 3273          | 65168         |
| 1    | 74045  | 5582        | 1536      | 768        | 6074        | 256          | 3277          | 65182         |
| 2    | 74066  | 5583        | 1536      | 768        | 6074        | 256          | 3280          | 65199         |
| 3    | 74087  | 5583        | 1536      | 768        | 6074        | 256          | 3280          | 65220         |

---

## **Performance Improvement Analysis**  

To quantify the impact of parallelism, we compare the **serial vs. parallel execution times**:

- **Serial execution cycles**: **104,173**
- **Parallel execution cycles (max per core)**: **74,087**
- **Speedup** = 104173 / 74087 ≈ **1.4x** (**40% improvement** in execution time)

While **40% improvement** is significant, it is **far from the expected 4x speedup**, highlighting **bottlenecks such as cache conflicts, bus contention, and memory stalls**.

## **Counter Test Bottleneck**  

Interestingly, the **counter test** (which is supposed to be parallel) performed **worse** than expected due to excessive **synchronization and memory stalls**. The increased **bus traffic and cache invalidations** caused performance **degradation instead of improvement**.

---

## **Key Observations**  

1. **Loop Unrolling Reduced Cache Misses**  
   - Both the **serial and parallel vector additions** benefited from **loop unrolling**, which minimized memory stalls.  
   - However, the **counter test suffered from frequent cache invalidations**, making it inefficient.

2. **Parallel Execution Improved Performance, but Not Linearly**  
   - The **40% improvement** in parallel vector addition shows that parallelism **does help**, but memory contention and cache behavior **limit the scalability**.  
   - Ideally, a **4x improvement** was expected, but shared memory accesses and cache conflicts reduced efficiency.

3. **Synchronization Overhead in the Counter Test**  
   - Instead of accelerating execution, **synchronization overhead caused performance degradation**, making it almost **as slow as the serial case**.  
   - This demonstrates a **common pitfall in multi-core programming**—parallel execution must be carefully designed to avoid **unintended memory stalls**.

---

## **Conclusion**  

- **Parallel execution can significantly improve performance**, but efficiency is dependent on **data partitioning, memory access patterns, and cache behavior**.  
- **Blindly parallelizing a workload (like in the counter test) can actually hurt performance**, demonstrating that **synchronization overhead can become a bottleneck**.  
- The **parallel vector addition** achieved a **40% improvement**, but **further optimizations** (e.g., improved cache utilization and bus efficiency) could push performance closer to the theoretical **4x speedup**.




