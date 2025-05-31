// register.c
#include "register.h"

void register_init(Register* reg) {
    reg->Q = 0;
    reg->D = 0;
    reg->valid = false;
    reg->enable = true;
}

void register_set_next(Register* reg, uint32_t value) {
    reg->D = value;
}

void register_clock_update(Register* reg) {
    if (reg->enable) {
        reg->Q = reg->D;
        reg->valid = true;
    }
}

uint32_t register_get_value(Register* reg) {
    return reg->Q;
}

void register_set_enable(Register* reg, bool enable) {
    reg->enable = enable;
}

void register_set_valid(Register* reg, bool valid) {
    reg->valid = valid;
}

