// register.h
#ifndef REGISTER_H
#define REGISTER_H

#include <stdint.h>
#include <stdbool.h>

typedef struct {
    uint32_t Q;        // Current value (output)
    uint32_t D;        // Next value (input)
    bool valid;        // Indicates if the data is valid
    bool enable;       // Clock enable signal
} Register;

// Initialize register with default values
void register_init(Register* reg);

// Set the next value (D input)
void register_set_next(Register* reg, uint32_t value);

// Update current value on clock edge (Q output)
void register_clock_update(Register* reg);

// Get current value
uint32_t register_get_value(Register* reg);

// Set enable signal
void register_set_enable(Register* reg, bool enable);

// Set valid signal
void register_set_valid(Register* reg, bool valid);

#endif // REGISTER_H

