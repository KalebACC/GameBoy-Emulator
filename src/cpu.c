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
  if (cpu->halt_bug) {
    cpu->halt_bug = false;
  } else {
    cpu->PC++;
  }
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

/**
 * @brief Pushes a 16-bit value onto the CPU stack.
 * @param cpu Pointer to the CPU.
 * @param memory Pointer to Memory.
 * @param value 16-bit value to push.
 */
static void cpu_push16(CPU *cpu, Memory *memory, const u16 value) {
  cpu->SP--;
  memory_write(memory, cpu->SP, (u8) (value >> 8));

  cpu->SP--;
  memory_write(memory, cpu->SP, (u8) (value & 0xFF));
}

/**
 * @brief Pops a 16-bit value from the CPU stack.
 * @param cpu Pointer to the CPU.
 * @param memory Pointer to Memory.
 * @return The popped 16-bit value.
 */
static u16 cpu_pop16(CPU *cpu, const Memory *memory) {
  const u8 low = memory_read(memory, cpu->SP++);
  const u8 high = memory_read(memory, cpu->SP++);

  return (u16) ((high << 8) | low);
}


/* The opcode's r8 field encodes B, C, D, E, H, L, [HL], A in that order. */
static u8 *cpu_r8_register(CPU *cpu, const u8 operand) {
  u8 *const registers[] = {&cpu->B, &cpu->C, &cpu->D, &cpu->E, &cpu->H, &cpu->L, nullptr, &cpu->A};
  return registers[operand];
}

static u16 cpu_hl(const CPU *cpu) {
  return (u16) ((cpu->H << 8) | cpu->L);
}

static u8 cpu_read_r8(CPU *cpu, const Memory *memory, const u8 operand) {
  return operand == 6 ? memory_read(memory, cpu_hl(cpu)) : *cpu_r8_register(cpu, operand);
}

static void cpu_write_r8(CPU *cpu, Memory *memory, const u8 operand, const u8 value) {
  if (operand == 6) {
    memory_write(memory, cpu_hl(cpu), value);
  } else {
    *cpu_r8_register(cpu, operand) = value;
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
 * @brief Store SP at the immediate 16-bit address, low byte first.
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

static void inc_r8(CPU *cpu, Memory *memory, const u8 operand) {
  const u8 value = cpu_read_r8(cpu, memory, operand);
  const u8 result = (u8) (value + 1);
  cpu_write_r8(cpu, memory, operand, result);
  cpu->F = (cpu->F & 0x10) | (result == 0 ? 0x80 : 0) |
           ((value & 0x0F) == 0x0F ? 0x20 : 0);
}

static void dec_r8(CPU *cpu, Memory *memory, const u8 operand) {
  const u8 value = cpu_read_r8(cpu, memory, operand);
  const u8 result = (u8) (value - 1);
  cpu_write_r8(cpu, memory, operand, result);
  cpu->F = (cpu->F & 0x10) | 0x40 | (result == 0 ? 0x80 : 0) |
           ((value & 0x0F) == 0 ? 0x20 : 0);
}

static void add_hl_r16(CPU *cpu, const u16 value) {
  const u16 hl = cpu_hl(cpu);
  const u32 result = (u32) hl + value;
  cpu->F = (cpu->F & 0x80) | (result > 0xFFFF ? 0x10 : 0) |
           (((hl & 0x0FFF) + (value & 0x0FFF)) > 0x0FFF ? 0x20 : 0);
  cpu->H = (u8) (result >> 8);
  cpu->L = (u8) result;
}

//---------------------------------Block 1---------------------------------

static void ld_r8_r8(CPU *cpu, Memory *memory, const u8 opcode) {
  const u8 value = cpu_read_r8(cpu, memory, opcode & 7);
  cpu_write_r8(cpu, memory, (opcode >> 3) & 7, value);
}

/** @brief HALT is opcode 0x76, replacing LD [HL], [HL]. */
static void halt(CPU *cpu, const Memory *memory) {
  const u8 pending = memory_read(memory, 0xFFFF) & memory_read(memory, 0xFF0F) & 0x1F;
  if (!cpu->ime && pending != 0) {
    cpu->halt_bug = true;
  } else {
    cpu->halted = true;
  }
}

//---------------------------------Block 2---------------------------------

/**
 * @brief Adds the value in r8 to A.
 * @param cpu Pointer to the CPU
 * @param memory Pointer to Memory for the [HL] operand.
 * @param r8 Source register or REG_HL_IND.
 */
static void add_a_r8(CPU *cpu, const Memory *memory, const Register r8) {
  u8 value;

  switch (r8) {
    case REG_HL_IND:
      value = memory_read(memory, ((u16) cpu->H << 8) | cpu->L);
      break;
    case REG_A: value = cpu->A;
      break;
    case REG_B: value = cpu->B;
      break;
    case REG_C: value = cpu->C;
      break;
    case REG_D: value = cpu->D;
      break;
    case REG_E: value = cpu->E;
      break;
    case REG_H: value = cpu->H;
      break;
    case REG_L: value = cpu->L;
      break;
    default: return;
  }

  const u8 old_a = cpu->A;
  const u16 result = (u16) old_a + value;

  cpu->A = (u8) result;

  cpu->F = (u8) (
    (cpu->A == 0 ? 0x80 : 0) |
    (((old_a & 0x0F) + (value & 0x0F)) > 0x0F ? 0x20 : 0) |
    (result > 0xFF ? 0x10 : 0)
  );
}

/**
 * @brief Adds the value of an 8-bit register or [HL] and the carry flag to A.
 * @param cpu Pointer to the CPU.
 * @param memory Pointer to Memory for the [HL] operand.
 * @param r8 Source register enum.
 */
static void adc_a_r8(CPU *cpu, const Memory *memory, const Register r8) {
  u8 value;

  switch (r8) {
    case REG_HL_IND:
      value = memory_read(memory, ((u16) cpu->H << 8) | cpu->L);
      break;
    case REG_A: value = cpu->A;
      break;
    case REG_B: value = cpu->B;
      break;
    case REG_C: value = cpu->C;
      break;
    case REG_D: value = cpu->D;
      break;
    case REG_E: value = cpu->E;
      break;
    case REG_H: value = cpu->H;
      break;
    case REG_L: value = cpu->L;
      break;
    default: return;
  }

  const u8 old_a = cpu->A;
  const u8 carry = (cpu->F & 0x10) ? 1 : 0;

  // Add A + source + carry.
  const u16 result = (u16) old_a + value + carry;
  cpu->A = (u8) result;

  // Update flags
  cpu->F = (u8) (
    (cpu->A == 0 ? 0x80 : 0) |
    (((old_a & 0x0F) + (value & 0x0F) + carry) > 0x0F ? 0x20 : 0) |
    (result > 0xFF ? 0x10 : 0)
  );
}

/**
 * @brief Subtracts the value of an 8-bit register from A.
 * @param cpu Pointer to the CPU.
 * @param memory Pointer to Memory for the [HL] operand.
 * @param r8 Source register enum.
 */
static void sub_a_r8(CPU *cpu, const Memory *memory, const Register r8) {
  u8 value;

  switch (r8) {
    case REG_HL_IND:
      value = memory_read(memory, ((u16) cpu->H << 8) | cpu->L);
      break;
    case REG_A: value = cpu->A;
      break;
    case REG_B: value = cpu->B;
      break;
    case REG_C: value = cpu->C;
      break;
    case REG_D: value = cpu->D;
      break;
    case REG_E: value = cpu->E;
      break;
    case REG_H: value = cpu->H;
      break;
    case REG_L: value = cpu->L;
      break;
    default: return;
  }

  const u8 old_a = cpu->A;
  const u8 result = (u8) (old_a - value);

  cpu->A = result;

  cpu->F = (u8) (
    (result == 0 ? 0x80 : 0) |
    0x40 |
    ((old_a & 0x0F) < (value & 0x0F) ? 0x20 : 0) |
    (old_a < value ? 0x10 : 0)
  );
}

/**
 * @brief Subtracts an 8-bit register and the carry flag from A.
 * @param cpu Pointer to the CPU.
 * @param memory Pointer to Memory for the [HL] operand.
 * @param r8 Source register enum.
 */
static void sbc_a_r8(CPU *cpu, const Memory *memory, const Register r8) {
  u8 value;

  switch (r8) {
    case REG_HL_IND:
      value = memory_read(memory, ((u16) cpu->H << 8) | cpu->L);
      break;
    case REG_A: value = cpu->A;
      break;
    case REG_B: value = cpu->B;
      break;
    case REG_C: value = cpu->C;
      break;
    case REG_D: value = cpu->D;
      break;
    case REG_E: value = cpu->E;
      break;
    case REG_H: value = cpu->H;
      break;
    case REG_L: value = cpu->L;
      break;
    default: return;
  }

  const u8 old_a = cpu->A;
  const u8 carry = (cpu->F & 0x10) ? 1 : 0;
  const u16 result = (u16) old_a - value - carry;

  // Store the lower 8 bits.
  cpu->A = (u8) result;

  // Update Flags.
  cpu->F = (u8) (
    (cpu->A == 0 ? 0x80 : 0) |
    0x40 |
    ((old_a & 0x0F) < ((value & 0x0F) + carry) ? 0x20 : 0) |
    (old_a < ((u16) value + carry) ? 0x10 : 0)
  );
}

/**
 * @brief Set A to the bitwise AND between the value in r8 and A
 * @param cpu Pointer to the CPU
 * @param memory Pointer to Memory for the [HL] operand.
 * @param r8 Source register Enum.
 */
static void and_a_r8(CPU *cpu, const Memory *memory, const Register r8) {
  u8 value;
  switch (r8) {
    case REG_HL_IND:
      value = memory_read(memory, ((u16) cpu->H << 8) | cpu->L);
      break;
    case REG_A: value = cpu->A;
      break;
    case REG_B: value = cpu->B;
      break;
    case REG_C: value = cpu->C;
      break;
    case REG_D: value = cpu->D;
      break;
    case REG_E: value = cpu->E;
      break;
    case REG_H: value = cpu->H;
      break;
    case REG_L: value = cpu->L;
      break;
    default: return;
  }
  cpu->A &= value;
  cpu->F = (u8) (
    (cpu->A == 0 ? 0x80 : 0) |
    0x20
  );
}

/**
 * @brief Set A to the bitwise XOR between the value in r8 and A.
 * @param cpu Pointer to the CPU
 * @param memory Pointer to Memory for the [HL] operand.
 * @param r8 Source register Enum.
 */
static void xor_a_r8(CPU *cpu, const Memory *memory, const Register r8) {
  u8 value;
  switch (r8) {
    case REG_HL_IND:
      value = memory_read(memory, ((u16) cpu->H << 8) | cpu->L);
      break;
    case REG_A: value = cpu->A;
      break;
    case REG_B: value = cpu->B;
      break;
    case REG_C: value = cpu->C;
      break;
    case REG_D: value = cpu->D;
      break;
    case REG_E: value = cpu->E;
      break;
    case REG_H: value = cpu->H;
      break;
    case REG_L: value = cpu->L;
      break;
    default: return;
  }
  cpu->A ^= value;
  cpu->F = (u8) (
    (cpu->A == 0 ? 0x80 : 0)
  );
}

/**
 * @brief Set A to the bitwise OR between the value in r8 and A.
 * @param cpu Pointer to the CPU
 * @param memory Pointer to Memory for the [HL] operand.
 * @param r8 Source register Enum.
 */
static void or_a_r8(CPU *cpu, const Memory *memory, const Register r8) {
  u8 value;
  switch (r8) {
    case REG_HL_IND:
      value = memory_read(memory, ((u16) cpu->H << 8) | cpu->L);
      break;
    case REG_A: value = cpu->A;
      break;
    case REG_B: value = cpu->B;
      break;
    case REG_C: value = cpu->C;
      break;
    case REG_D: value = cpu->D;
      break;
    case REG_E: value = cpu->E;
      break;
    case REG_H: value = cpu->H;
      break;
    case REG_L: value = cpu->L;
      break;
    default: return;
  }
  cpu->A |= value;
  cpu->F = (u8) (
    (cpu->A == 0 ? 0x80 : 0)
  );
}

/**
 * @brief Compares A with an 8-bit register by subtracting
 *        the register's value from A without modifying A.
 * @param cpu Pointer to the CPU.
 * @param memory Pointer to Memory for the [HL] operand.
 * @param r8 Source register enum.
 */
static void cp_a_r8(CPU *cpu, const Memory *memory, const Register r8) {
  u8 value;

  switch (r8) {
    case REG_HL_IND:
      value = memory_read(memory, ((u16) cpu->H << 8) | cpu->L);
      break;
    case REG_A: value = cpu->A;
      break;
    case REG_B: value = cpu->B;
      break;
    case REG_C: value = cpu->C;
      break;
    case REG_D: value = cpu->D;
      break;
    case REG_E: value = cpu->E;
      break;
    case REG_H: value = cpu->H;
      break;
    case REG_L: value = cpu->L;
      break;
    default: return;
  }

  const u8 a = cpu->A;
  const u8 result = (u8) (a - value);

  cpu->F = (u8) (
    (result == 0 ? 0x80 : 0) |
    0x40 |
    ((a & 0x0F) < (value & 0x0F) ? 0x20 : 0) |
    (a < value ? 0x10 : 0)
  );
}

//---------------------------------Block 3---------------------------------

/**
 * @brief Add the value n8 to A..
 * @param cpu Pointer to the CPU
 * @param memory Pointer to Memory.
 */
static void add_a_imm8(CPU *cpu, const Memory *memory) {
  const u8 value = cpu_fetch(cpu, memory);

  const u8 old_a = cpu->A;
  const u16 result = (u16) old_a + value;

  cpu->A = (u8) result;

  cpu->F = (u8) (
    (cpu->A == 0 ? 0x80 : 0) |
    (((old_a & 0x0F) + (value & 0x0F)) > 0x0F ? 0x20 : 0) |
    (result > 0xFF ? 0x10 : 0)
  );
}

/**
 * @brief Add the value n8 plus the carry flag to A.
 * @param cpu Pointer to the CPU.
 * @param memory Pointer to Memory.
 */
static void adc_a_imm8(CPU *cpu, const Memory *memory) {
  const u8 value = cpu_fetch(cpu, memory);
  const u8 old_a = cpu->A;
  const u8 carry = (cpu->F & 0x10) ? 1 : 0;

  // Add A + source + carry.
  const u16 result = (u16) old_a + value + carry;
  cpu->A = (u8) result;

  // Update flags
  cpu->F = (u8) (
    (cpu->A == 0 ? 0x80 : 0) |
    (((old_a & 0x0F) + (value & 0x0F) + carry) > 0x0F ? 0x20 : 0) |
    (result > 0xFF ? 0x10 : 0)
  );
}

/**
 * @brief Subtract the value n8 from A.
 * @param cpu Pointer to the CPU.
 * @param memory Pointer to Memory
 */
static void sub_a_imm8(CPU *cpu, const Memory *memory) {
  const u8 value = cpu_fetch(cpu, memory);
  const u8 old_a = cpu->A;
  const u8 result = (u8) (old_a - value);

  cpu->A = result;

  cpu->F = (u8) (
    (result == 0 ? 0x80 : 0) |
    0x40 |
    ((old_a & 0x0F) < (value & 0x0F) ? 0x20 : 0) |
    (old_a < value ? 0x10 : 0)
  );
}

/**
 * @brief Subtract the value n8 and the carry flag from A.
 * @param cpu Pointer to the CPU.
 * @param memory Pointer to Memory
 */
static void sbc_a_imm8(CPU *cpu, const Memory *memory) {
  const u8 value = cpu_fetch(cpu, memory);
  const u8 old_a = cpu->A;
  const u8 carry = (cpu->F & 0x10) ? 1 : 0;
  const u16 result = (u16) old_a - value - carry;

  // Store the lower 8 bits.
  cpu->A = (u8) result;

  // Update Flags.
  cpu->F = (u8) (
    (cpu->A == 0 ? 0x80 : 0) |
    0x40 |
    ((old_a & 0x0F) < ((value & 0x0F) + carry) ? 0x20 : 0) |
    (old_a < ((u16) value + carry) ? 0x10 : 0)
  );
}

/**
 * @brief Set A to the bitwise AND between the value n8 and A.
 * @param cpu Pointer to the CPU
 * @param memory Pointer to Memory.
 */
static void and_a_imm8(CPU *cpu, const Memory *memory) {
  const u8 value = cpu_fetch(cpu, memory);

  cpu->A &= value;
  cpu->F = (u8) (
    (cpu->A == 0 ? 0x80 : 0) |
    0x20
  );
}

/**
 * @brief Set A to the bitwise XOR between the value n8 and A.
 * @param cpu Pointer to the CPU
 * @param memory Pointer to Memory.
 */
static void xor_a_imm8(CPU *cpu, const Memory *memory) {
  const u8 value = cpu_fetch(cpu, memory);
  cpu->A ^= value;
  cpu->F = (u8) (
    (cpu->A == 0 ? 0x80 : 0)
  );
}

/**
 * @brief Set A to the bitwise OR between the value n8 and A.
 * @param cpu Pointer to the CPU.
 * @param memory Pointer to Memory.
 */
static void or_a_imm8(CPU *cpu, const Memory *memory) {
  const u8 value = cpu_fetch(cpu, memory);
  cpu->A |= value;
  cpu->F = (u8) (
    (cpu->A == 0 ? 0x80 : 0)
  );
}

/**
 * @brief ComPare the value in A with the value n8.
 * This subtracts the value n8 from A and sets flags accordingly, but discards the result.
 * @param cpu Pointer to the CPU.
 * @param memory Pointer to Memory for the [HL] operand.
 */
static void cp_a_imm8(CPU *cpu, const Memory *memory) {
  const u8 value = cpu_fetch(cpu, memory);
  const u8 a = cpu->A;
  const u8 result = (u8) (a - value);

  cpu->F = (u8) (
    (result == 0 ? 0x80 : 0) |
    0x40 |
    ((a & 0x0F) < (value & 0x0F) ? 0x20 : 0) |
    (a < value ? 0x10 : 0)
  );
}

/**
 * Return from subroutine if condition cc is met.
 * @param cpu Pointer to the CPU.
 * @param memory Pointer to Memory.
 * @param cond Condition Enum.
 */
static void ret_cond(CPU *cpu, const Memory *memory, const Condition cond) {
  if (!cpu_check_condition(cpu, cond)) { return; }
  cpu->PC = cpu_pop16(cpu, memory);
}

/**
 * Return from subroutine.
 * @param cpu Pointer to the CPU
 * @param memory Pointer to Memory
 */
static void ret(CPU *cpu, const Memory *memory) {
  cpu->PC = cpu_pop16(cpu, memory);
}

/**
 * Return from subroutine and enable interrupts.
 * This is basically equivalent to executing EI then RET, meaning that IME is set right after this instruction.
 * @param cpu Pointer to the CPU.
 * @param memory Pointer to Memory.
 */
static void reti(CPU *cpu, const Memory *memory) {
  ret(cpu, memory);
  cpu->ime = true;
}

/**
 * @brief Jump to address n16 if condition cc is met.
 * @param cpu Pointer to the CPU.
 * @param memory Pointer to Memory.
 * @param cond Condition Enum.
 */
static void jp_cond_imm16(CPU *cpu, const Memory *memory, const Condition cond) {
  const u16 address = cpu_fetch16(cpu, memory);

  if (cpu_check_condition(cpu, cond)) {
    cpu->PC = address;
  }
}

/**
 * @brief Jump to address n16; effectively, copy n16 into PC.
 * @param cpu Pointer to the CPU.
 * @param memory Pointer to Memory.
 */
static void jp_imm16(CPU *cpu, const Memory *memory) {
  const u16 address = cpu_fetch16(cpu, memory);
  cpu->PC = address;
}

/**
 * @brief Jump to address in HL; effectively, copy the value in register HL into PC.
 * @param cpu Pointer to the CPU.
 */
static void jp_hl(CPU *cpu) {
  cpu->PC = (u16) ((cpu->H << 8) | cpu->L);
}

/**
 * @brief Call address n16 if condition cc is met.
 * @param cpu Pointer to the CPU;
 * @param memory Pointer to Memory;
 * @param cond Condition Enum;
 */
static void call_cond_imm16(CPU *cpu, Memory *memory, const Condition cond) {
  const u16 address = cpu_fetch16(cpu, memory);
  if (!cpu_check_condition(cpu, cond)) { return; }
  cpu_push16(cpu, memory, cpu->PC);
  cpu->PC = address;
}

/**
* @brief Call address n16.
* This pushes the address of the instruction after the CALL on the stack, such that RET can pop it later;
* then, it executes an implicit JP n16.
 * @param cpu Pointer to the CPU
 * @param memory Pointer to Memory
 */
static void call_imm16(CPU *cpu, Memory *memory) {
  const u16 address = cpu_fetch16(cpu, memory);
  cpu_push16(cpu, memory, cpu->PC);
  cpu->PC = address;
}

/**
 * @brief Calls a subroutine at a fixed restart address.
 * @param cpu Pointer to the CPU.
 * @param memory Pointer to Memory.
 * @param target 3-bit restart target (0-7).
 */
static void rst(CPU *cpu, Memory *memory, const u8 target) {
  cpu_push16(cpu, memory, cpu->PC);
  cpu->PC = (u16) (target << 3);
}

/**
 * @brief Pop register r16 from the stack.
 * @param cpu Pointer to the CPU
 * @param memory Pointer to Memory
 * @param destination Register Pair Destination
 */
static void pop_r16stk(CPU *cpu, const Memory *memory, const RegisterPair destination) {
  const u16 value = cpu_pop16(cpu, memory);
  const u8 high = (u8) (value >> 8);
  const u8 low = (u8) value;

  switch (destination) {
    case BC:
      cpu->B = high;
      cpu->C = low;
      break;
    case DE:
      cpu->D = high;
      cpu->E = low;
      break;
    case HL:
      cpu->H = high;
      cpu->L = low;
      break;
    case AF:
      cpu->A = high;
      cpu->F = (u8) (low & 0xF0u);
      break;
    default:
      break;
  }
}

/**
 * Push register r16 into the stack.
 * @param cpu Pointer to the CPU
 * @param memory Pointer to Memory
 * @param destination Desination Register Pair
 */
static void push_r16stk(CPU *cpu, Memory *memory, const RegisterPair destination) {
  u8 high;
  u8 low;

  switch (destination) {
    case BC:
      high = cpu->B;
      low = cpu->C;
      break;
    case DE:
      high = cpu->D;
      low = cpu->E;
      break;
    case HL:
      high = cpu->H;
      low = cpu->L;
      break;
    case AF:
      high = cpu->A;
      low = (u8) (cpu->F & 0xF0u);
      break;
    default:
      return;
  }

  cpu_push16(cpu, memory, (u16) (((u16) high << 8) | low));
}

/**
 * Copy the value in register A into the byte at address $FF00+C.
 * @param cpu Pointer to the CPU
 * @param memory Pointer to Memory
 */
static void ldh_c_a(const CPU *cpu, Memory *memory) {
  const u16 address = (u16) (0xFF00u + cpu->C);
  memory_write(memory, address, cpu->A);
}

/**
 * Copy the value in register A into the byte at address $FF00 + imm8.
 * @param cpu Pointer to cpu
 * @param memory Pointer to memory
 */
static void ldh_imm8_a(CPU *cpu, Memory *memory) {
  const u8 offset = cpu_fetch(cpu, memory);
  const u16 address = (u16) (0xFF00u + offset);
  memory_write(memory, address, cpu->A);
}

/**
 * Copy the value in register A into the byte at address imm16.
 * @param cpu Pointer to cpu
 * @param memory Pointer to memory
 */
static void ld_imm16_a(CPU *cpu, Memory *memory) {
  const u16 address = cpu_fetch16(cpu, memory);
  memory_write(memory, address, cpu->A);
}

/**
 * Copy the byte at address $FF00+C into register A.
 * @param cpu Pointer to cpu
 * @param memory Pointer to memory
 */
static void ldh_a_c(CPU *cpu, const Memory *memory) {
  const u16 address = (u16) (0xFF00u + cpu->C);
  cpu->A = memory_read(memory, address);
}

/**
 * Copy the byte at address imm8 into register A, provided the address is between $FF00 and $FFFF.
 * @param cpu Pointer cpu
 * @param memory Pointer to memory
 */
static void ldh_a_imm8(CPU *cpu, const Memory *memory) {
  const u8 low = cpu_fetch(cpu, memory);
  const u16 address = (u16) (0xFF00u | low);
  cpu->A = memory_read(memory, address);
}

/**
 * Copy the byte at address imm16 into register A, provided the address is between $FF00 and $FFFF.
 * @param cpu Pointer cpu
 * @param memory Pointer to memory
 */
static void ld_a_imm16(CPU *cpu, const Memory *memory) {
  const u16 address = cpu_fetch16(cpu, memory);
  cpu->A = memory_read(memory, address);
}

/**
 * @Breif Add the signed value e8 to SP.
 * @param cpu Pointer to memory
 * @param memory Pointer to cpu
 */
static void add_sp_imm8(CPU *cpu, const Memory *memory) {
  const u8 imm = cpu_fetch(cpu, memory);
  const int offset = (imm < 0x80u) ? (int) imm : (int) imm - 0x100;
  const u16 sp = cpu->SP;

  cpu->F = 0;
  if ((sp & 0x0Fu) + (imm & 0x0Fu) > 0x0Fu) {
    cpu->F |= 0x20u; /* H */
  }
  if ((sp & 0xFFu) + imm > 0xFFu) {
    cpu->F |= 0x10u; /* C */
  }

  cpu->SP = (u16) (sp + offset);
}

/**
 * Add the signed value e8 to SP and copy the result in HL.
 * @param cpu Pointer to the cpu
 * @param memory Pointer to memory
 */
static void ld_hl_sp_imm8(CPU *cpu, const Memory *memory) {
  const u8 imm = cpu_fetch(cpu, memory);
  const int offset = (imm < 0x80u) ? (int) imm : (int) imm - 0x100;
  const u16 sp = cpu->SP;
  const u16 result = (u16) (sp + offset);

  cpu->F = 0;
  if ((sp & 0x0Fu) + (imm & 0x0Fu) > 0x0Fu) {
    cpu->F |= 0x20u; /* H flag */
  }
  if ((sp & 0xFFu) + imm > 0xFFu) {
    cpu->F |= 0x10u; /* C flag */
  }

  cpu->H = (u8) (result >> 8);
  cpu->L = (u8) result;
}

//---------------------------------CB Prefix---------------------------------

static u8 cb_rotate_shift(CPU *cpu, const u8 operation, const u8 value) {
  const u8 carry_in = (cpu->F >> 4) & 1;
  const u8 high_bit = value >> 7;
  const u8 low_bit = value & 1;
  u8 carry_out = 0;
  u8 result = value;

  switch (operation) {
    case 0: // RLC
      result = (u8) ((value << 1) | high_bit);
      carry_out = high_bit;
      break;
    case 1: // RRC
      result = (u8) ((value >> 1) | (low_bit << 7));
      carry_out = low_bit;
      break;
    case 2: // RL
      result = (u8) ((value << 1) | carry_in);
      carry_out = high_bit;
      break;
    case 3: // RR
      result = (u8) ((value >> 1) | (carry_in << 7));
      carry_out = low_bit;
      break;
    case 4: // SLA
      result = (u8) (value << 1);
      carry_out = high_bit;
      break;
    case 5: // SRA
      result = (u8) ((value >> 1) | (value & 0x80));
      carry_out = low_bit;
      break;
    case 6: // SWAP
      result = (u8) ((value << 4) | (value >> 4));
      break;
    case 7: // SRL
      result = value >> 1;
      carry_out = low_bit;
      break;
    default:
      break;
  }

  cpu->F = (result == 0 ? 0x80 : 0) | (carry_out << 4);
  return result;
}

static void cpu_step_cb(CPU *cpu, Memory *memory) {
  const u8 opcode = cpu_fetch(cpu, memory);
  const u8 operand = opcode & 7;
  const u8 operation = (opcode >> 3) & 7;
  u8 value = cpu_read_r8(cpu, memory, operand);
  const u8 mask = (u8) (1u << operation);

  switch (opcode >> 6) {
    case 0:
      value = cb_rotate_shift(cpu, operation, value);
      break;
    case 1: // BIT
      cpu->F = (cpu->F & 0x10) | 0x20 | ((value & mask) == 0 ? 0x80 : 0);
      return;
    case 2: // RES
      value &= (u8) ~mask;
      break;
    case 3: // SET
      value |= mask;
      break;
    default:
      break;
  }

  cpu_write_r8(cpu, memory, operand, value);
}

//---------------------------------Execution---------------------------------

/**
 * @brief Executes a CPU instruction.
 * @param cpu Pointer to the CPU
 * @param memory Pointer to the Memory.
 */
void cpu_step(CPU *cpu, Memory *memory) {
  if (cpu->stopped) {
    return;
  }

  const u8 pending = memory_read(memory, 0xFFFF) & memory_read(memory, 0xFF0F) & 0x1F;
  if (cpu->halted) {
    if (pending == 0) {
      return;
    }
    cpu->halted = false;
  }

  if (cpu->ime && pending != 0) {
    u8 interrupt = 0;
    while ((pending & (1u << interrupt)) == 0) {
      interrupt++;
    }
    cpu->ime = false;
    cpu->ime_enable_pending = false;
    cpu->halt_bug = false;
    memory_write(memory, 0xFF0F, (u8) (memory_read(memory, 0xFF0F) & ~(1u << interrupt)));
    memory_write(memory, --cpu->SP, (u8) (cpu->PC >> 8));
    memory_write(memory, --cpu->SP, (u8) cpu->PC);
    cpu->PC = (u16) (0x40 + 8 * interrupt);
    return;
  }

  const bool enable_ime_after_instruction = cpu->ime_enable_pending;
  const u8 opcode = cpu_fetch(cpu, memory);

  switch (opcode) {
    // INC r8
    case 0x04:
    case 0x0C:
    case 0x14:
    case 0x1C:
    case 0x24:
    case 0x2C:
    case 0x34:
    case 0x3C:
      inc_r8(cpu, memory, (opcode >> 3) & 7);
      break;

    // DEC r8
    case 0x05:
    case 0x0D:
    case 0x15:
    case 0x1D:
    case 0x25:
    case 0x2D:
    case 0x35:
    case 0x3D:
      dec_r8(cpu, memory, (opcode >> 3) & 7);
      break;

    case 0x09: // ADD HL, BC
      add_hl_r16(cpu, (u16) ((cpu->B << 8) | cpu->C));
      break;

    case 0x19: // ADD HL, DE
      add_hl_r16(cpu, (u16) ((cpu->D << 8) | cpu->E));
      break;

    case 0x29: // ADD HL, HL
      add_hl_r16(cpu, cpu_hl(cpu));
      break;

    case 0x39: // ADD HL, SP
      add_hl_r16(cpu, cpu->SP);
      break;

    case 0x36: // LD [HL], imm8
      cpu_write_r8(cpu, memory, 6, cpu_fetch(cpu, memory));
      break;

    case 0x00: // NOP
      nop(cpu);
      break;

    case 0x01: // LD BC, imm16
      ld_r16_imm16(cpu, memory, BC);
      break;

    case 0x02: // LD [BC], A
      ld_r16mem_a(cpu, memory, BC);
      break;

    case 0x03: // INC BC
      inc_r16(cpu, BC);
      break;

    case 0x06: // LD B, imm8
      ld_r8_imm8(cpu, memory, REG_B);
      break;

    case 0x07: // RLCA
      rlca(cpu);
      break;

    case 0x08: // LD [imm16], SP
      ld_imm16_sp(cpu, memory);
      break;

    case 0x0A: // LD A, [BC]
      ld_a_r16mem(cpu, memory, BC);
      break;

    case 0x0B: // DEC BC
      dec_r16(cpu, BC);
      break;

    case 0x0E: // LD C, imm8
      ld_r8_imm8(cpu, memory, REG_C);
      break;

    case 0x0F: // RRCA
      rrca(cpu);
      break;

    case 0x10: // STOP
      stop(cpu, memory);
      break;

    case 0x11: // LD DE, imm16
      ld_r16_imm16(cpu, memory, DE);
      break;

    case 0x12: // LD [DE], A
      ld_r16mem_a(cpu, memory, DE);
      break;

    case 0x13: // INC DE
      inc_r16(cpu, DE);
      break;

    case 0x16: // LD D, imm8
      ld_r8_imm8(cpu, memory, REG_D);
      break;

    case 0x17: // RLA
      rla(cpu);
      break;

    case 0x18: // JR imm8
      jr_imm8(cpu, memory);
      break;

    case 0x1A: // LD A, [DE]
      ld_a_r16mem(cpu, memory, DE);
      break;

    case 0x1B: // DEC DE
      dec_r16(cpu, DE);
      break;

    case 0x1E: // LD E, imm8
      ld_r8_imm8(cpu, memory, REG_E);
      break;

    case 0x1F: // RRA
      rra(cpu);
      break;

    case 0x20: // JR NZ, imm8
      jr_cond_imm8(cpu, memory, COND_NZ);
      break;

    case 0x21: // LD HL, imm16
      ld_r16_imm16(cpu, memory, HL);
      break;

    case 0x22: // LD [HL+], A
      ld_r16mem_a(cpu, memory, HL);
      inc_r16(cpu, HL);
      break;

    case 0x23: // INC HL
      inc_r16(cpu, HL);
      break;

    case 0x26: // LD H, imm8
      ld_r8_imm8(cpu, memory, REG_H);
      break;

    case 0x27: // DAA
      daa(cpu);
      break;

    case 0x28: // JR Z, imm8
      jr_cond_imm8(cpu, memory, COND_Z);
      break;

    case 0x2A: // LD A, [HL+]
      ld_a_r16mem(cpu, memory, HL);
      inc_r16(cpu, HL);
      break;

    case 0x2B: // DEC HL
      dec_r16(cpu, HL);
      break;

    case 0x2E: // LD L, imm8
      ld_r8_imm8(cpu, memory, REG_L);
      break;

    case 0x2F: // CPL
      cpl(cpu);
      break;

    case 0x30: // JR NC, imm8
      jr_cond_imm8(cpu, memory, COND_NC);
      break;

    case 0x31: // LD SP, imm16
      ld_r16_imm16(cpu, memory, SP);
      break;

    case 0x32: // LD [HL-], A
      ld_r16mem_a(cpu, memory, HL);
      dec_r16(cpu, HL);
      break;

    case 0x33: // INC SP
      inc_r16(cpu, SP);
      break;

    case 0x37: // SCF
      scf(cpu);
      break;

    case 0x38: // JR C, imm8
      jr_cond_imm8(cpu, memory, COND_C);
      break;

    case 0x3A: // LD A, [HL-]
      ld_a_r16mem(cpu, memory, HL);
      dec_r16(cpu, HL);
      break;

    case 0x3B: // DEC SP
      dec_r16(cpu, SP);
      break;

    case 0x3E: // LD A, imm8
      ld_r8_imm8(cpu, memory, REG_A);
      break;

    case 0x3F: // CCF
      ccf(cpu);
      break;

    // LD r8, r8
    case 0x40:
    case 0x41:
    case 0x42:
    case 0x43:
    case 0x44:
    case 0x45:
    case 0x46:
    case 0x47:
    case 0x48:
    case 0x49:
    case 0x4A:
    case 0x4B:
    case 0x4C:
    case 0x4D:
    case 0x4E:
    case 0x4F:
    case 0x50:
    case 0x51:
    case 0x52:
    case 0x53:
    case 0x54:
    case 0x55:
    case 0x56:
    case 0x57:
    case 0x58:
    case 0x59:
    case 0x5A:
    case 0x5B:
    case 0x5C:
    case 0x5D:
    case 0x5E:
    case 0x5F:
    case 0x60:
    case 0x61:
    case 0x62:
    case 0x63:
    case 0x64:
    case 0x65:
    case 0x66:
    case 0x67:
    case 0x68:
    case 0x69:
    case 0x6A:
    case 0x6B:
    case 0x6C:
    case 0x6D:
    case 0x6E:
    case 0x6F:
    case 0x70:
    case 0x71:
    case 0x72:
    case 0x73:
    case 0x74:
    case 0x75:
    case 0x77:
    case 0x78:
    case 0x79:
    case 0x7A:
    case 0x7B:
    case 0x7C:
    case 0x7D:
    case 0x7E:
    case 0x7F:
      ld_r8_r8(cpu, memory, opcode);
      break;

    case 0x76: // HALT (block 1 exception)
      halt(cpu, memory);
      break;

    case 0x80: // ADD A, B
      add_a_r8(cpu, memory, REG_B);
      break;

    case 0x81: // ADD A, C
      add_a_r8(cpu, memory, REG_C);
      break;

    case 0x82: // ADD A, D
      add_a_r8(cpu, memory, REG_D);
      break;

    case 0x83: // ADD A, E
      add_a_r8(cpu, memory, REG_E);
      break;

    case 0x84: // ADD A, H
      add_a_r8(cpu, memory, REG_H);
      break;

    case 0x85: // ADD A, L
      add_a_r8(cpu, memory, REG_L);
      break;

    case 0x86: // ADD A, [HL]
      add_a_r8(cpu, memory, REG_HL_IND);
      break;

    case 0x87: // ADD A, A
      add_a_r8(cpu, memory, REG_A);
      break;

    case 0x88: // ADC A, B
      adc_a_r8(cpu, memory, REG_B);
      break;

    case 0x89: // ADC A, C
      adc_a_r8(cpu, memory, REG_C);
      break;

    case 0x8A: // ADC A, D
      adc_a_r8(cpu, memory, REG_D);
      break;

    case 0x8B: // ADC A, E
      adc_a_r8(cpu, memory, REG_E);
      break;

    case 0x8C: // ADC A, H
      adc_a_r8(cpu, memory, REG_H);
      break;

    case 0x8D: // ADC A, L
      adc_a_r8(cpu, memory, REG_L);
      break;

    case 0x8E: // ADC A, [HL]
      adc_a_r8(cpu, memory, REG_HL_IND);
      break;

    case 0x8F: // ADC A, A
      adc_a_r8(cpu, memory, REG_A);
      break;

    case 0x90: // SUB A, B
      sub_a_r8(cpu, memory, REG_B);
      break;

    case 0x91: // SUB A, C
      sub_a_r8(cpu, memory, REG_C);
      break;

    case 0x92: // SUB A, D
      sub_a_r8(cpu, memory, REG_D);
      break;

    case 0x93: // SUB A, E
      sub_a_r8(cpu, memory, REG_E);
      break;

    case 0x94: // SUB A, H
      sub_a_r8(cpu, memory, REG_H);
      break;

    case 0x95: // SUB A, L
      sub_a_r8(cpu, memory, REG_L);
      break;

    case 0x96: // SUB A, [HL]
      sub_a_r8(cpu, memory, REG_HL_IND);
      break;

    case 0x97: // SUB A, A
      sub_a_r8(cpu, memory, REG_A);
      break;

    case 0x98: // SBC A, B
      sbc_a_r8(cpu, memory, REG_B);
      break;

    case 0x99: // SBC A, C
      sbc_a_r8(cpu, memory, REG_C);
      break;

    case 0x9A: // SBC A, D
      sbc_a_r8(cpu, memory, REG_D);
      break;

    case 0x9B: // SBC A, E
      sbc_a_r8(cpu, memory, REG_E);
      break;

    case 0x9C: // SBC A, H
      sbc_a_r8(cpu, memory, REG_H);
      break;

    case 0x9D: // SBC A, L
      sbc_a_r8(cpu, memory, REG_L);
      break;

    case 0x9E: // SBC A, [HL]
      sbc_a_r8(cpu, memory, REG_HL_IND);
      break;

    case 0x9F: // SBC A, A
      sbc_a_r8(cpu, memory, REG_A);
      break;

    case 0xA0: // AND A, B
      and_a_r8(cpu, memory, REG_B);
      break;

    case 0xA1: // AND A, C
      and_a_r8(cpu, memory, REG_C);
      break;

    case 0xA2: // AND A, D
      and_a_r8(cpu, memory, REG_D);
      break;

    case 0xA3: // AND A, E
      and_a_r8(cpu, memory, REG_E);
      break;

    case 0xA4: // AND A, H
      and_a_r8(cpu, memory, REG_H);
      break;

    case 0xA5: // AND A, L
      and_a_r8(cpu, memory, REG_L);
      break;

    case 0xA6: // AND A, [HL]
      and_a_r8(cpu, memory, REG_HL_IND);
      break;

    case 0xA7: // AND A, A
      and_a_r8(cpu, memory, REG_A);
      break;

    case 0xA8: // XOR A, B
      xor_a_r8(cpu, memory, REG_B);
      break;

    case 0xA9: // XOR A, C
      xor_a_r8(cpu, memory, REG_C);
      break;

    case 0xAA: // XOR A, D
      xor_a_r8(cpu, memory, REG_D);
      break;

    case 0xAB: // XOR A, E
      xor_a_r8(cpu, memory, REG_E);
      break;

    case 0xAC: // XOR A, H
      xor_a_r8(cpu, memory, REG_H);
      break;

    case 0xAD: // XOR A, L
      xor_a_r8(cpu, memory, REG_L);
      break;

    case 0xAE: // XOR A, [HL]
      xor_a_r8(cpu, memory, REG_HL_IND);
      break;

    case 0xAF: // XOR A, A
      xor_a_r8(cpu, memory, REG_A);
      break;

    case 0xB0: // OR A, B
      or_a_r8(cpu, memory, REG_B);
      break;

    case 0xB1: // OR A, C
      or_a_r8(cpu, memory, REG_C);
      break;

    case 0xB2: // OR A, D
      or_a_r8(cpu, memory, REG_D);
      break;

    case 0xB3: // OR A, E
      or_a_r8(cpu, memory, REG_E);
      break;

    case 0xB4: // OR A, H
      or_a_r8(cpu, memory, REG_H);
      break;

    case 0xB5: // OR A, L
      or_a_r8(cpu, memory, REG_L);
      break;

    case 0xB6: // OR A, [HL]
      or_a_r8(cpu, memory, REG_HL_IND);
      break;

    case 0xB7: // OR A, A
      or_a_r8(cpu, memory, REG_A);
      break;

    case 0xB8: // CP A, B
      cp_a_r8(cpu, memory, REG_B);
      break;

    case 0xB9: // CP A, C
      cp_a_r8(cpu, memory, REG_C);
      break;

    case 0xBA: // CP A, D
      cp_a_r8(cpu, memory, REG_D);
      break;

    case 0xBB: // CP A, E
      cp_a_r8(cpu, memory, REG_E);
      break;

    case 0xBC: // CP A, H
      cp_a_r8(cpu, memory, REG_H);
      break;

    case 0xBD: // CP A, L
      cp_a_r8(cpu, memory, REG_L);
      break;

    case 0xBE: // CP A, [HL]
      cp_a_r8(cpu, memory, REG_HL_IND);
      break;

    case 0xBF: // CP A, A
      cp_a_r8(cpu, memory, REG_A);
      break;

    case 0xC0: // RET NZ
      ret_cond(cpu, memory, COND_NZ);
      break;

    case 0xC1: // POP BC
      pop_r16stk(cpu, memory, BC);
      break;

    case 0xC2: // JP NZ, imm16
      jp_cond_imm16(cpu, memory, COND_NZ);
      break;

    case 0xC3: // JP imm16
      jp_imm16(cpu, memory);
      break;

    case 0xC4: // CALL NZ, imm16
      call_cond_imm16(cpu, memory, COND_NZ);
      break;

    case 0xC5: // PUSH BC
      push_r16stk(cpu, memory, BC);
      break;

    case 0xC6: // ADD A, imm8
      add_a_imm8(cpu, memory);
      break;

    case 0xC7: // RST 0x00
      rst(cpu, memory, 0);
      break;

    case 0xC8: // RET Z
      ret_cond(cpu, memory, COND_Z);
      break;

    case 0xC9: // RET
      ret(cpu, memory);
      break;

    case 0xCB: // CB prefix
      cpu_step_cb(cpu, memory);
      break;

    case 0xCA: // JP Z, imm16
      jp_cond_imm16(cpu, memory, COND_Z);
      break;

    case 0xCC: // CALL Z, imm16
      call_cond_imm16(cpu, memory, COND_Z);
      break;

    case 0xCD: // CALL imm16
      call_imm16(cpu, memory);
      break;

    case 0xCE: // ADC A, imm8
      adc_a_imm8(cpu, memory);
      break;

    case 0xCF: // RST 0x08
      rst(cpu, memory, 1);
      break;

    case 0xD0: // RET NC
      ret_cond(cpu, memory, COND_NC);
      break;

    case 0xD1: // POP DE
      pop_r16stk(cpu, memory, DE);
      break;

    case 0xD2: // JP NC, imm16
      jp_cond_imm16(cpu, memory, COND_NC);
      break;

    case 0xD4: // CALL NC, imm16
      call_cond_imm16(cpu, memory, COND_NC);
      break;

    case 0xD5: // PUSH DE
      push_r16stk(cpu, memory, DE);
      break;

    case 0xD6: // SUB A, imm8
      sub_a_imm8(cpu, memory);
      break;

    case 0xD7: // RST 0x10
      rst(cpu, memory, 2);
      break;

    case 0xD8: // RET C
      ret_cond(cpu, memory, COND_C);
      break;

    case 0xD9: // RETI
      reti(cpu, memory);
      break;

    case 0xDA: // JP C, imm16
      jp_cond_imm16(cpu, memory, COND_C);
      break;

    case 0xDC: // CALL C, imm16
      call_cond_imm16(cpu, memory, COND_C);
      break;

    case 0xDE: // SBC A, imm8
      sbc_a_imm8(cpu, memory);
      break;

    case 0xDF: // RST 0x18
      rst(cpu, memory, 3);
      break;

    case 0xE0: // LDH [imm8], A
      ldh_imm8_a(cpu, memory);
      break;

    case 0xE1: // POP HL
      pop_r16stk(cpu, memory, HL);
      break;

    case 0xE2: // LDH [C], A
      ldh_c_a(cpu, memory);
      break;

    case 0xE5: // PUSH HL
      push_r16stk(cpu, memory, HL);
      break;

    case 0xE6: // AND A, imm8
      and_a_imm8(cpu, memory);
      break;

    case 0xE7: // RST 0x20
      rst(cpu, memory, 4);
      break;

    case 0xE8: // ADD SP, imm8
      add_sp_imm8(cpu, memory);
      break;

    case 0xE9: // JP HL
      jp_hl(cpu);
      break;

    case 0xEA: // LD [imm16], A
      ld_imm16_a(cpu, memory);
      break;

    case 0xEE: // XOR A, imm8
      xor_a_imm8(cpu, memory);
      break;

    case 0xEF: // RST 0x28
      rst(cpu, memory, 5);
      break;

    case 0xF0: // LDH A, [imm8]
      ldh_a_imm8(cpu, memory);
      break;

    case 0xF1: // POP AF
      pop_r16stk(cpu, memory, AF);
      break;

    case 0xF2: // LDH A, [C]
      ldh_a_c(cpu, memory);
      break;

    case 0xF3: // DI
      cpu->ime = false;
      cpu->ime_enable_pending = false;
      break;

    case 0xF5: // PUSH AF
      push_r16stk(cpu, memory, AF);
      break;

    case 0xF6: // OR A, imm8
      or_a_imm8(cpu, memory);
      break;

    case 0xF7: // RST 0x30
      rst(cpu, memory, 6);
      break;

    case 0xF8: // LD HL, SP + imm8
      ld_hl_sp_imm8(cpu, memory);
      break;

    case 0xF9: // LD SP, HL
      cpu->SP = cpu_hl(cpu);
      break;

    case 0xFA: // LD A, [imm16]
      ld_a_imm16(cpu, memory);
      break;

    case 0xFB: // EI
      cpu->ime_enable_pending = true;
      break;

    case 0xFE: // CP A, imm8
      cp_a_imm8(cpu, memory);
      break;

    case 0xFF: // RST 0x38
      rst(cpu, memory, 7);
      break;
    default:
      return;
  }

  if (enable_ime_after_instruction && cpu->ime_enable_pending) {
    cpu->ime = true;
    cpu->ime_enable_pending = false;
  }
}
