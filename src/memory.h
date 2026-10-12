//
// Created by kaleb on 10/10/2026.
//

#ifndef GAMEBOYEMU_MEMORY_H
#define GAMEBOYEMU_MEMORY_H
#include "../include/types.h"


/**
 * @brief Struct to represent the GameBoy's 16-bit address bus
 */
typedef struct {
  u8 bytes[65536];
} Memory;

u8 memory_read(const Memory *memory, u16 address);

void memory_write(Memory *memory, u16 address, u8 data);

#endif //GAMEBOYEMU_MEMORY_H
