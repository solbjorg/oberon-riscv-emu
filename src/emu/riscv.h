#ifndef __RISCV_H_
#define __RISCV_H_

#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "../risc-io.h"

// Defines whether to use 64-bit or 32-bit
//#define RV64

#ifdef RV64
#define MAX_VALUE LONG_MAX
#else
#define MAX_VALUE INT_MAX
#endif

enum Instructions { ADDI };

typedef uint32_t word_t;
typedef uint64_t dword_t;
typedef uint8_t byte_t;

#ifdef RV64
typedef uint64_t ureg_t;
typedef int64_t reg_t;
typedef uint64_t addr_t;
#else
typedef uint32_t ureg_t;
typedef int32_t reg_t;
typedef uint32_t addr_t;
#endif

typedef struct RV {
  ureg_t pc;
  ureg_t *registers;
  byte_t *RAM;

  const struct RISC_LED *leds;
  const struct RISC_Serial *serial;
  uint32_t spi_selected;
  const struct RISC_SPI *spi[4];
  const struct RISC_Clipboard *clipboard;
} RISC_V;

ureg_t *riscv_read_register(RISC_V *machine, uint8_t reg);
void riscv_write_register(RISC_V *machine, uint8_t reg, ureg_t value);
void riscv_write_pc(RISC_V *machine, ureg_t value);

void riscv_print_state(RISC_V *machine);
void riscv_execute(void);

#endif // __RISCV_H_
