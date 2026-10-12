//
// Created by kaleb on 10/10/2026.
//

#include "cpu.h"
#include "../include/types.h"


/**
 * @brief Fetches the next opcode from memory at the current program counter and increments it.
 * @param cpu Pointer to the CPU.
 * @param memory Pointer to the Memory.
 * @return 8-bit opcode value stored in the program counter (PC).
 */
u8 cpu_fetch(CPU *cpu, const Memory *memory) {
  const u8 value = memory_read(memory, cpu->PC);
  cpu->PC++;
  return value;
}

/**
 * @brief Fetches a 16-bit value from memory
 * Reads 2 consecutive bytes from memory using cpu_fetch(), to preserve little-endian byte order,
 * the low byte is fetched first then the high byte.
 * @param cpu Pointer to the CPU.
 * @param memory Pointer to the Memory.
 * @return The combined 16-Bit value
 */
static u16 cpu_fetch16(CPU *cpu, const Memory *memory) {
  //GameBoy uses little-endian
  const u8 low = cpu_fetch(cpu, memory);
  const u8 high = cpu_fetch(cpu, memory);
  return (u16) (high << 8) | low;
}

//opcodes

//Block 0
/**
 *  Executes a NOP operation.
 * @param cpu Pointer to the CPU.
 */
static void nop(const CPU *cpu) {
  (void) cpu;
}

/**
 * @brief Copy the value n16 into register r16.
 * @param cpu Pointer to the CPU.
 * @param memory Pointer to the Memory.
 * @param destination The 16-bit register to load to.
 */
static void ld_r16_imm16(CPU *cpu, const Memory *memory, const RegisterPair destination) {
  const u16 value = cpu_fetch16(cpu, memory);

  switch (destination) {
    case BC:
      cpu->B = (u8) (value >> 8);
      cpu->C = (u8) value & 0xff;
      break;
    case DE:
      cpu->D = (u8) (value >> 8);
      cpu->E = (u8) value & 0xff;
      break;
    case HL:
      cpu->H = (u8) (value >> 8);
      cpu->L = (u8) value & 0xff;
      break;
    default:
      return;
  }
}

/**
 * @brief Copy the value in register A into the byte pointed to by r16.
 * @param cpu Pointer to the CPU.
 * @param memory Pointer to the Memory.
 * @param destination Destination register pair (BC, DE, or HL)
 */
static void ld_r16mem_a(const CPU *cpu, Memory *memory, const RegisterPair destination) {
  u16 address;
  switch (destination) {
    case BC:
      address = ((u16) cpu->B << 8) | cpu->C;
      break;
    case DE:
      address = ((u16) cpu->D << 8) | cpu->E;
      break;
    case HL:
      address = ((u16) cpu->H << 8) | cpu->L;
      break;
    default:
      return;
  }
  memory_write(memory, address, cpu->A);
}

/**
 * @brief Stores the contents of a memory address held by r16mem into A
 * @param cpu Pointer to the CPU
 * @param memory pointer to the Memory
 * @param destination Destination register pair (BC, DE, or HL)
 */
static void ld_a_r16mem(CPU *cpu, const Memory *memory, const RegisterPair destination) {
  u16 address;
  switch (destination) {
    case BC:
      address = ((u16) cpu->B << 8) | cpu->C;
      break;
    case DE:
      address = ((u16) cpu->D << 8) | cpu->E;
      break;
    case HL:
      address = ((u16) cpu->H << 8) | cpu->L;
      break;
    default:
      return;
  }
  cpu->A = memory->bytes[address];
}

/**
 * @brief Copy the byte pointed to by r16 into register A.
 * @param cpu Pointer to the CPU.
 * @param memory Pointer to the Memory.
 */
static void ld_imm16_sp(CPU *cpu, Memory *memory) {
  const u16 address = cpu_fetch16(cpu, memory);
  const u16 value = cpu->SP;

  memory_write(memory, address, (u8) (value & 0xFF));
  memory_write(memory, address + 1, (u8) (value >> 8));
}

/**
 * @brief Increment the value in register r16 by 1.
 * @param cpu Pointer to the CPU.
 * @param destination Destination register pair (BC, DE, or HL)
 */
static void inc_r16(CPU *cpu, const RegisterPair destination) {
  u16 value;
  switch (destination) {
    case BC:
      value = (u16) ((u16) cpu->B << 8 | cpu->C);
      value++;
      cpu->B = (value >> 8);
      cpu->C = (value & 0xFF);
      break;
    case DE:
      value = (u16) ((u16) cpu->D << 8 | cpu->E);
      value++;
      cpu->D = (value >> 8);
      cpu->E = (value & 0xFF);
      break;
    case HL:
      value = (u16) ((u16) cpu->H << 8 | cpu->L);
      value++;
      cpu->H = (value >> 8);
      cpu->L = (value & 0xFF);
      break;
    case SP:
      cpu->SP++;
      break;
    default:
      return;
  }
}

/**
 * @brief Decrement the value in register r16 by 1.
 * @param cpu Pointer to the CPU.
 * @param destination Destination register pair (BC, DE, or HL)
 */
static void dec_r16(CPU *cpu, const RegisterPair destination) {
  u16 value;
  switch (destination) {
    case BC:
      value = (u16) ((u16) cpu->B << 8 | cpu->C);
      value--;
      cpu->B = (value >> 8);
      cpu->C = (value & 0xFF);
      break;
    case DE:
      value = (u16) ((u16) cpu->D << 8 | cpu->E);
      value--;
      cpu->D = (value >> 8);
      cpu->E = (value & 0xFF);
      break;
    case HL:
      value = (u16) ((u16) cpu->H << 8 | cpu->L);
      value--;
      cpu->H = (value >> 8);
      cpu->L = (value & 0xFF);
      break;
    case SP:
      cpu->SP++;
      break;
    default:
      return;
  }
}

/**
 * @brief Copy the value n8 into register r8.
 * @param cpu Pointer to the CPU.
 * @param memory Pointer to the Memory.
 * @param destination The 8-bit register to load to
 */
static void ld_r8_imm8(CPU *cpu, const Memory *memory, const Register destination) {
  const u8 value = cpu_fetch(cpu, memory);
  switch (destination) {
    case B: cpu->B = value;
      break;
    case C: cpu->C = value;
      break;
    case D: cpu->D = value;
      break;
    case E: cpu->E = value;
      break;
    case H: cpu->H = value;
      break;
    case L: cpu->L = value;
      break;
    case A: cpu->A = value;
      break;
    default:
      return;
  }
}

/**
 * @brief Rotate register A left.
 * Z = 0, N = 0, H = 0, C = Result
 * @param cpu Pointer to the CPU.
 */
static void rlca(CPU *cpu) {
  const u8 bit7 = (u8)(cpu->A << 7);
  cpu->A = (u8) (cpu->A << 1 | bit7);
  cpu->F = bit7 ? 0x10 : 0x00;
}


// opcode lookup
/**
 * @brief Executes a CPU instruction.
 * @param cpu Pointer to the CPU
 * @param memory Pointer to the Memory.
 */
void cpu_step(CPU *cpu, const Memory *memory) {
  const u8 opcode = cpu_fetch(cpu, memory);

  switch (opcode) {
    case 0x00: //nop
      nop(cpu);
      break;

    case 0x01:
      ld_r16_imm16(cpu, memory, BC);
      break;

    case 0x06:
      ld_r8_imm8(cpu, memory, B);
      break;

    default:
      return;
  }
}
