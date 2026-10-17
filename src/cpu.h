// Created by kaleb on 10/10/2026.

#ifndef GAMEBOYEMU_CPU_H
#define GAMEBOYEMU_CPU_H

#include "memory.h"
#include "../include/types.h"

typedef struct {
  // Registers
  u8 A;
  u8 B, C;
  u8 D, E;
  u8 H, L;

  // Flags
  u8 F;

  // Special registers
  u16 PC;
  u16 SP;

  //stopping
  bool stopped;
} CPU;

typedef enum {
  AF,
  BC,
  DE,
  HL,
  PC,
  SP
} RegisterPair;

typedef enum {
  REG_A,
  REG_B,
  REG_C,
  REG_D,
  REG_E,
  REG_H,
  REG_L,
  REG_F
} Register;

typedef enum {
  COND_NZ,
  COND_Z,
  COND_NC,
  COND_C
} Condition;


void cpu_step(CPU *cpu, Memory *memory);

#endif // GAMEBOYEMU_CPU_H
