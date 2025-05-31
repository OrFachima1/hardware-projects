// alu.h
#ifndef ALU_H
#define ALU_H

#include <stdint.h>

// Arithmetic operations supported by ALU
typedef enum {
    ALU_ADD,  // add
    ALU_SUB,  // sub 
    ALU_AND,  // and
    ALU_OR,   // or
    ALU_XOR,  // xor
    ALU_MUL,  // mul
    ALU_SLL,  // sll
    ALU_SRA,  // sra (arithmetic)
    ALU_SRL   // srl (logical)
} alu_op_t;

uint32_t alu_execute(alu_op_t op, uint32_t a, uint32_t b);

#endif

