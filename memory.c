//
// Created by kaleb on 10/10/2026.
//

#include "memory.h"


/**
 * @brief Read 8-bit value from memory at specified address.
 * @param memory memory Pointer to the memory instance to read from.
 * @param address 16-bit address ranging from 0x0000 to 0xFFFF.
 * @return 8-bit value stored in memory at specified address.
 */
u8 memory_read(const Memory *memory, const u16 address) {
  return memory->bytes[address];
}


/**
 * @brief Write a 8-bit value to a specified memory address.
 * @param memory memory Pointer to the memory instance to read from.
 * @param address 16-bit address ranging from 0x0000 to 0xFFFF.
 * @param data 8-bit value to be written to the specified memory address
 */
void memory_write(Memory *memory, const u16 address, const u8 data) {
  memory->bytes[address] = data;
}