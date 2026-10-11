//
// Created by kaleb on 10/10/2026.
//

#ifndef GAMEBOYEMU_CPU_H
#define GAMEBOYEMU_CPU_H
#include "memory.h"
#include "include/types.h"

typedef struct {
  //Registers
  u8 A;
  u8 B, C;
  u8 D, E;
  u8 H, L;

  //flags
  u8 F;

  //special
  u16 PC;
  u16 SP;
} CPU;

typedef enum {
  AF,
  BC,
  DE,
  HL,
} RegisterPair;

typedef enum {
  A, B, C, D, E, H, L, F, PC, SP
} Register;

//Primary functions
void cpu_step(CPU *cpu, const Memory *memory);

u8 cpu_fetch(CPU *cpu, const Memory *memory);

//Helper functions
static u16 cpu_fetch16(CPU *cpu, const Memory *memory);

//opcodes

//Block 0
static void nop(const CPU *cpu); //0x00
static void ld_r16_imm16(CPU *cpu, const Memory *memory, RegisterPair destination);

static void ld_r16mem_a(const CPU *cpu, Memory *memory, RegisterPair destination);

static void ld_a_r16mem(CPU *cpu, const Memory *memory, RegisterPair destination);

static void ld_imm16_sp(CPU *cpu, Memory *memory);

static void ld_r8_imm8(CPU *cpu, const Memory *memory, Register destination);


#endif //GAMEBOYEMU_CPU_H
