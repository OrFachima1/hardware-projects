// alu.c
#include "alu.h"

uint32_t alu_execute(alu_op_t op, uint32_t a, uint32_t b) {
    switch (op) {
    case ALU_ADD:
        return a + b;

    case ALU_SUB:
        return a - b;

    case ALU_AND:
        return a & b;

    case ALU_OR:
        return a | b;

    case ALU_XOR:
        return a ^ b;

    case ALU_MUL:
        return a * b;

    case ALU_SLL:
        return a << (b & 0x1F); // Shift by lower 5 bits only

    case ALU_SRA: {
        // Sign-extended right shift
        int32_t signed_a = (int32_t)a;
        return (uint32_t)(signed_a >> (b & 0x1F));
    }

    case ALU_SRL:
        return a >> (b & 0x1F); // Logical right shift

    default:
        return 0;
    }
}