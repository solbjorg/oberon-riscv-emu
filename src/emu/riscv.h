#ifndef __RISCV_H_
#define __RISCV_H_

#include <limits.h>
#include <string.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#include "cpu.h"

#ifdef RV64
#define MAX_VALUE LONG_MAX
#else
#define MAX_VALUE INT_MAX
#endif

CPU *riscv_new();

void riscv_execute(CPU *machine, uint32_t cycles);

#endif // __RISCV_H_
