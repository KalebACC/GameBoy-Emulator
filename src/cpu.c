//
// Created by kaleb Accardi on 10/10/2026.
//

#include "cpu.h"
#include "../include/types.h"

/**
 * @brief Fetches the next opcode from memory at the current program counter and increments it.
 * @param cpu Pointer to the CPU.
 * @param memory Pointer to the Memory.
 * @return 8-bit opcode value stored in the program counter (PC).
 */
static u8 cpu_fetch(CPU *cpu, const Memory *memory) {
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
  return (((u16) high << 8) | low);
}

/**
 * Checks whether the specified CPU flag condition is satisfied.
 * @param cpu
 * @param condition
 * @return
 */
static bool cpu_check_condition(const CPU *cpu, const Condition condition) {
  const bool zero = (cpu->F & 0x80) != 0;
  const bool carry = (cpu->F & 0x10) != 0;

  switch (condition) {
    case COND_NZ: return !zero;
    case COND_Z: return zero;
    case COND_NC: return !carry;
    case COND_C: return carry;
    default: return false;
  }
}


//---------------------------------Block 0---------------------------------
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
    case SP:
      cpu->SP = value;
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
  cpu->A = memory_read(memory, address);
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
      value = (u16) (((u16) cpu->B << 8) | cpu->C);
      value++;
      cpu->B = (u8) (value >> 8);
      cpu->C = (u8) (value & 0xFF);
      break;
    case DE:
      value = (u16) (((u16) cpu->D << 8) | cpu->E);
      value++;
      cpu->D = (u8) (value >> 8);
      cpu->E = (u8) (value & 0xFF);
      break;
    case HL:
      value = (u16) (((u16) cpu->H << 8) | cpu->L);
      value++;
      cpu->H = (u8) (value >> 8);
      cpu->L = (u8) (value & 0xFF);
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
      cpu->SP--;
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
    case REG_B: cpu->B = value;
      break;
    case REG_C: cpu->C = value;
      break;
    case REG_D: cpu->D = value;
      break;
    case REG_E: cpu->E = value;
      break;
    case REG_H: cpu->H = value;
      break;
    case REG_L: cpu->L = value;
      break;
    case REG_A: cpu->A = value;
      break;
    default:
      return;
  }
}

/**
 * @brief Rotate register A left.
 * Z = 0, N = 0, H = 0, C = Result.
 * @param cpu Pointer to the CPU.
 */
static void rlca(CPU *cpu) {
  const u8 bit7 = (u8) ((cpu->A >> 7) & 0x01);
  cpu->A = (u8) ((cpu->A << 1) | bit7);
  cpu->F = bit7 ? 0x10 : 0x00;
}

/**
 * @brief Rotate register A right.
 * Z = 0, N = 0, H = 0, C = Result.
 * @param cpu Pointer to the CPU
 */
static void rrca(CPU *cpu) {
  const u8 bit0 = (u8) (cpu->A & 0x01);
  cpu->A = (u8) ((cpu->A >> 1) | (bit0 << 7));
  cpu->F = bit0 ? 0x10 : 0x00;
}

/**
 * @brief Rotate register A left, through the carry flag.
 * Z = 0, N = 0, H = 0, C = Result.
 * @param cpu Pointer to the CPU.
 */
static void rla(CPU *cpu) {
  const u8 old_carry = (u8) ((cpu->F >> 4) & 0x01);
  const u8 bit7 = (u8) ((cpu->A >> 7) & 0x01);
  cpu->A = (u8) ((cpu->A << 1) | old_carry);
  cpu->F = bit7 ? 0x10 : 0x00;
}

/**
 * @brief Rotate register A right, through the carry flag.
 * Z = 0, N = 0, H = 0, C = Result.
 * @param cpu Pointer to the CPU
 */
static void rra(CPU *cpu) {
  const u8 old_carry = (u8) ((cpu->F >> 4) & 0x01);
  const u8 bit0 = (u8) (cpu->A & 0x01);
  cpu->A = (u8) ((cpu->A >> 1) | (old_carry << 7));
  cpu->F = bit0 ? 0x10 : 0x00;
}

/**
 * @brief Decimal Adjust Accumulator.
 * Designed to be used after performing an arithmetic instruction (ADD, ADC, SUB, SBC)
 * whose inputs were in Binary-Coded Decimal (BCD),
 * adjusting the result to likewise be in BCD.
 * @param cpu Pointer to the CPU
 */
static void daa(CPU *cpu) {
  u8 adjustment = 0;
  bool carry = (cpu->F & 0x10) != 0;
  const bool subtract = (cpu->F & 0x40) != 0;
  const bool half_carry = (cpu->F & 0x20) != 0;

  if (!subtract) {
    if (half_carry || (cpu->A & 0x0F) > 0x09) {
      adjustment |= 0x06;
    }

    if (carry || cpu->A > 0x99) {
      adjustment |= 0x60;
      carry = true;
    }
    cpu->A = (u8) (cpu->A + adjustment);
  } else {
    // Previous operation was subtraction.
    if (half_carry) {
      adjustment |= 0x06;
    }

    if (carry) {
      adjustment |= 0x60;
    }

    cpu->A = (u8) (cpu->A - adjustment);
  }
  cpu->F = (u8) (
    (cpu->F & 0x40) |
    (cpu->A == 0 ? 0x80 : 0) |
    (carry ? 0x10 : 0)
  );
}

/**
 * Complement accumulator (A = ~A); also called bitwise NOT.
 * @param cpu Pointer to the CPU
 */
static void cpl(CPU *cpu) {
  cpu->A = (u8) ~cpu->A;
  cpu->F = (u8) ((cpu->F & 0x90) | 0x60);
}

/**
 * Set Carry Flag.
 * @param cpu Pointer to the CPU.
 */
static void scf(CPU *cpu) {
  cpu->F = (u8) ((cpu->F & 0x80) | 0x10);
}

/**
 * Complement Carry Flag.
 * @param cpu Pointer to the CPU.
 */
static void ccf(CPU *cpu) {
  cpu->F = (cpu->F & 0x80) |
           ((cpu->F ^ 0x10) & 0x10);
}

/**
 * @brief Jumps to a relative address using a signed 8-bit offset.
 * @param cpu Pointer to the CPU.
 * @param memory Pointer to Memory.
 */
static void jr_imm8(CPU *cpu, const Memory *memory) {
  const i8 offset = (i8) cpu_fetch(cpu, memory);
  cpu->PC = (u16) (cpu->PC + offset);
}

/**
 * @brief Performs a conditional relative jump using a signed 8-bit offset.
 * @param cpu Pointer to the CPU
 * @param memory Pointer to Memory.
 * @param condition Condition that determines whether the jump occurs.
 */
static void jr_cond_imm8(CPU *cpu, const Memory *memory, const Condition condition) {
  const i8 offset = (i8) cpu_fetch(cpu, memory);
  if (cpu_check_condition(cpu, condition)) {
    cpu->PC = (u16) (cpu->PC + offset);
  }
}

/**
 * Stops the CPU until a wake-up event occurs.
 * @param cpu Pointer to the CPU.
 * @param memory Pointer to Memory.
 */
static void stop(CPU *cpu, const Memory *memory) {
  (void) cpu_fetch(cpu, memory); // Use the second byte.
  cpu->stopped = true;
}

//---------------------------------Block 1---------------------------------


/**
 * @brief Executes a CPU instruction.
 * @param cpu Pointer to the CPU
 * @param memory Pointer to the Memory.
 */
void cpu_step(CPU *cpu, Memory *memory) {
  const u8 opcode = cpu_fetch(cpu, memory);

  switch (opcode) {
    case 0x00: //nop
      nop(cpu);
      break;

    case 0x01:
      ld_r16_imm16(cpu, memory, BC);
      break;

    case 0x06:
      ld_r8_imm8(cpu, memory, REG_B);
      break;

    default:
      return;
  }
}
