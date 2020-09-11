#include "riscv.h"
#include "utils.h"

#include <stdbool.h>

#define DefaultMemSize      0x00100000
#define DefaultDisplayStart 0x000E7F00

#define ROMStart     0xFFFFF800
#define ROMWords     512
#define IOStart      0xFFFFFFC0

const uint32_t NUM_REGS = 32; 
const uint32_t SIZE_MEM = 1048575;

bool jumped = false;

static const uint32_t program[1000] = {
#include "oberonfib.bin"
};

// TODO
RISC_V *riscv_new() {
  RISC_V *machine = malloc(sizeof(RISC_V));
  if (machine == NULL)
    return NULL;
  machine->pc = 0;
  machine->registers = malloc(NUM_REGS * sizeof(ureg_t));
  machine->registers[0] = 0;
  machine->registers[2] = 0xFFFFF; // sp
  machine->RAM = malloc(SIZE_MEM * sizeof(byte_t));
  return machine;
}

uint8_t riscv_get_funct3(uint32_t instruction) {
  return (instruction >> 12) & 0x7;
}

uint8_t riscv_get_funct7(uint32_t instruction) {
  return (instruction >> 25) & 0x7F;
}

uint8_t riscv_get_rs2(uint32_t instruction) {
  return (instruction >> 20) & 0x1F;
}

uint8_t riscv_get_shamt(uint32_t instruction) {
  return riscv_get_rs2(instruction);
}

uint8_t riscv_get_rs1(uint32_t instruction) {
  return (instruction >> 15) & 0x1F;
}

uint8_t riscv_get_rd(uint32_t instruction) { return (instruction >> 7) & 0x1F; }

int32_t riscv_get_imm_typej(uint32_t instruction) {
  uint32_t imm20 = (instruction >> 31) & 1;
  uint32_t imm11 = (instruction >> 20) & 1;
  uint32_t imm10_1 = (instruction >> 21) & 0x3FF;
  uint32_t imm19_12 = (instruction >> 12) & 0xFF;
  return (int32_t)(((imm20 << 20) | (imm19_12 << 12) | (imm11 << 11) |
                   (imm10_1 << 1)) << (32-21)) >> (32-21);
}

int32_t riscv_get_imm_types(uint32_t instruction) {
  int32_t imm11_5 = (instruction >> 25) & 0x3F;
  uint32_t imm4_0 = (instruction >> 7) & 0x1F;
  return (int32_t)((imm11_5 << 5) | (imm4_0));
}

int32_t riscv_get_imm_typeb(uint32_t instruction) {
  uint32_t imm12 = (instruction >> 31) & 1;
  uint32_t imm11 = (instruction >> 7) & 1;
  uint32_t imm10_5 = (instruction >> 25) & 0x3F;
  uint32_t imm4_1 = (instruction >> 8) & 0xF;
  return (int32_t)(((imm12 << 12) | (imm11 << 11) | (imm10_5 << 5) |
                   (imm4_1 << 1)) << (32-13)) >> (32-13);
}

int32_t riscv_get_imm_typeu(uint32_t instruction) {
  return (instruction & 0xFFFFE000);
}

int32_t riscv_get_imm_typei(uint32_t instruction) {
  return ((int32_t)(instruction)) >> 20;
}

uint8_t riscv_get_opcode(uint32_t instruction) { return instruction & 0x7F; }

byte_t riscv_read_mem(RISC_V *machine, addr_t addr) {
  if (addr < SIZE_MEM) {
    return machine->RAM[addr];
  } else {
    printf("Illegal load???");
    return 0;
  }
}

// TODO Make memory access circular
word_t riscv_read_mem_32(RISC_V *machine, addr_t addr) {
  return riscv_read_mem(machine, addr) |
         riscv_read_mem(machine, add(addr, 1)) << 8 | 
         riscv_read_mem(machine, add(addr, 2)) << 16 | 
         riscv_read_mem(machine, add(addr, 3)) << 24;
}

#ifdef RV64
ureg_t riscv_read_mem_64(RISC_V *machine, addr_t addr) {
  return riscv_read_mem_32(machine, addr) << 32 |
         riscv_read_mem_32(machine, addr + 4);
}
#endif

#ifdef RV64
void riscv_immediate_arithmetic_word(RISC_V *machine, uint32_t instruction) {
  uint8_t imm = riscv_get_imm_typei(instruction);
  uint8_t funct3 = riscv_get_funct3(instruction);
  
  ureg_t *rs1 = riscv_read_register(machine, riscv_get_rs1(instruction));
  uint8_t rd = riscv_get_rd(instruction);
  uint8_t shamt = riscv_get_shamt(instruction);
  uint8_t funct7 = riscv_get_funct7(instruction);

  // TODO add overflow bit sets etcetc
  switch (funct3) {
  case 0x0: // ADDI
    riscv_write_register(machine, rd, (word_t)add(*rs1, imm));
    break;
  case 0x1: // SLLI
    riscv_write_register(machine, rd, (word_t)(*rs1 << shamt));
    break;
  case 0x5:               // SRLI / SRAI
    if (funct7 == 0x20) { // SRAI
      // technically whether this is arithmetic is implementation-defined
      // TODO make this better
      riscv_write_register(machine, rd, (word_t)((reg_t)(*rs1) >> shamt));
    } else { // SRLI
      riscv_write_register(machine, rd, (word_t)(*rs1 >> shamt));
    }
    break;
  default:
    printf("Unsupported funct3 in imm arithmetic word: %x", funct3);
    break;
  }
}
#endif

void riscv_immediate_arithmetic(RISC_V *machine, uint32_t instruction) {
  int8_t imm = riscv_get_imm_typei(instruction);
  uint8_t funct3 = riscv_get_funct3(instruction);
  uint8_t rs1_name = riscv_get_rs1(instruction);
  ureg_t *rs1 = riscv_read_register(machine, rs1_name);
  uint8_t rd = riscv_get_rd(instruction);
  uint8_t shamt = riscv_get_shamt(instruction);
  uint8_t funct7 = riscv_get_funct7(instruction);

  // TODO add overflow bit sets etcetc
  switch (funct3) {
  case 0x0: // ADDI 
    printf("addi x%d, x%d, %d", rd, rs1_name, imm);
    riscv_write_register(machine, rd, add((reg_t)*rs1, imm));
    break;
  case 0x1: // SLLI
    printf("slli x%d, x%d, %d", rd, rs1_name, imm);
    riscv_write_register(machine, rd, *rs1 << shamt);
    break;
  case 0x2: // SLTI
    printf("slti x%d, x%d, %d", rd, rs1_name, imm);
    riscv_write_register(machine, rd, (reg_t)(*rs1) < (reg_t)(imm));
    break;
  case 0x3: // SLTIU
    printf("sltiu x%d, x%d, %d", rd, rs1_name, imm);
    riscv_write_register(machine, rd, *rs1 < imm);
    break;
  case 0x4: // XORI
    printf("xori x%d, x%d, %d", rd, rs1_name, imm);
    riscv_write_register(machine, rd, *rs1 ^ imm);
    break;
  case 0x5:               // SRLI / SRAI
    if (funct7 == 0x20) { // SRAI
      // technically whether this is arithmetic is implementation-defined
      // TODO make this better
      printf("srai x%d, x%d, %d", rd, rs1_name, imm);
      riscv_write_register(machine, rd, (reg_t)(*rs1) >> shamt);
    } else { // SRLI
      printf("srli x%d, x%d, %d", rd, rs1_name, imm);
      riscv_write_register(machine, rd, *rs1 >> shamt);
    }
    break;
  case 0x6: // ORI
    printf("ori x%d, %d, %d", rd, rs1_name, imm);
    riscv_write_register(machine, rd, *rs1 | imm);
    break;
  case 0x7: // ANDI
    printf("andi x%d, %d, %d", rd, rs1_name, imm);
    riscv_write_register(machine, rd, *rs1 & imm);
    break;
  default:
    printf("Unsupported funct3 in imm arithmetic: %x", funct3);
    break;
  }
}

void riscv_branch(RISC_V *machine, uint32_t instruction) {
  int8_t imm = riscv_get_imm_typeb(instruction);
  uint8_t funct3 = riscv_get_funct3(instruction);
  uint8_t rs1_name = riscv_get_rs1(instruction);
  uint8_t rs2_name = riscv_get_rs2(instruction);
  ureg_t rs1 = *riscv_read_register(machine, riscv_get_rs1(instruction));
  ureg_t rs2 = *riscv_read_register(machine, riscv_get_rs2(instruction));

  switch (funct3) {
  case 0x0: // BEQ
    printf("beq x%d, x%d, %d", rs1_name, rs2_name, imm);
    if (rs1 == rs2)
      riscv_write_pc(machine, machine->pc + imm);
    break;
  case 0x1: // BNE
    printf("bne x%d, x%d, %d", rs1_name, rs2_name, imm);
    if (rs1 != rs2)
      riscv_write_pc(machine, machine->pc + imm);
    break;
  case 0x4: // BLT
    printf("blt x%d, x%d, %d", rs1_name, rs2_name, imm);
    if ((reg_t)rs1 < (reg_t)rs2)
      riscv_write_pc(machine, machine->pc + imm);
    break;
  case 0x5: // BGE
    printf("bge x%d, x%d, %d", rs1_name, rs2_name, imm);
    if ((reg_t)rs1 >= (reg_t)rs2)
      riscv_write_pc(machine, machine->pc + imm);
    break;
  case 0x6: // BLTU
    printf("bltu x%d, x%d, %d", rs1_name, rs2_name, imm);
    if (rs1 < rs2)
      riscv_write_pc(machine, machine->pc + imm);
    break;
  case 0x7: // BGEU
    printf("bgeu x%d, x%d, %d", rs1_name, rs2_name, imm);
    if (rs1 >= rs2)
      riscv_write_pc(machine, machine->pc + imm);
    break;
  default:
    printf("Unsupported funct3 in branch: %x", funct3);
    break;
  }
}

void riscv_arithmetic(RISC_V *machine, uint32_t instruction) {
  uint8_t funct3 = riscv_get_funct3(instruction);
  uint8_t rs1_name = riscv_get_rs1(instruction);
  uint8_t rs2_name = riscv_get_rs2(instruction);
  ureg_t *rs1 = riscv_read_register(machine, riscv_get_rs1(instruction));
  ureg_t *rs2 = riscv_read_register(machine, riscv_get_rs2(instruction));
  uint8_t rd = riscv_get_rd(instruction);
  uint8_t funct7 = riscv_get_funct7(instruction);

  // TODO merge with arithmetic_immediate
  if (funct7 == 1) { // mul/div
    switch (funct3) {
      case 0x0: // mul
        printf("mul x%d, x%d, x%d", rd, rs1_name, rs2_name);
        riscv_write_register(machine, rd, *rs1 * *rs2);
        break;
      case 0x1: // mulh
        printf("mulh x%d, x%d, x%d", rd, rs1_name, rs2_name);
        riscv_write_register(machine, rd, ((int64_t)(*rs1 * *rs2)) << 32);
        break;
      case 0x2: // mulhsu
        printf("mulhsu x%d, x%d, x%d", rd, rs1_name, rs2_name);
        riscv_write_register(machine, rd, ((int64_t)((reg_t)*rs1 * *rs2)) << 32);
        break;
      case 0x3: // mulhu
        printf("mulhu x%d, x%d, x%d", rd, rs1_name, rs2_name);
        riscv_write_register(machine, rd, ((uint64_t)(*rs1 * *rs2)) << 32);
        break;
      case 0x4: // div
        printf("div x%d, x%d, x%d", rd, rs1_name, rs2_name);
        riscv_write_register(machine, rd, (reg_t)*rs1 / (reg_t)*rs2);
        break;
      case 0x5: // divu
        printf("divu x%d, x%d, x%d", rd, rs1_name, rs2_name);
        riscv_write_register(machine, rd, *rs1 / *rs2);
        break;
      case 0x6: // rem
        printf("rem x%d, x%d, x%d", rd, rs1_name, rs2_name);
        riscv_write_register(machine, rd, (reg_t)*rs1 % (reg_t)*rs2);
        break;
      case 0x7: // remu
        printf("remu x%d, x%d, x%d", rd, rs1_name, rs2_name);
        riscv_write_register(machine, rd, *rs1 % *rs2);
        break;
      default:
        printf("Unsupported funct3 in imm mul arithmetic: %x", funct3);
        break;
    }
  } else {
    switch (funct3) {
    case 0x0:             // ADD/SUB
      if (funct7 == 0x20) { // SUB
        printf("sub x%d, x%d, x%d", rd, rs1_name, rs2_name);
        riscv_write_register(machine, rd, sub(*rs1, *rs2));
      } else if (funct7 == 0x0) {// ADD
        printf("add x%d, x%d, x%d", rd, rs1_name, rs2_name);
        riscv_write_register(machine, rd, add(*rs1, *rs2));
      }
      break;
    case 0x1: // SLL
      printf("sll x%d, x%d, x%d", rd, rs1_name, rs2_name);
      riscv_write_register(machine, rd, *rs1 << *rs2);
      break;
    case 0x2: // SLT
      printf("slt x%d, x%d, x%d", rd, rs1_name, rs2_name);
      riscv_write_register(machine, rd, (reg_t)(*rs1) < (reg_t)(*rs2));
      break;
    case 0x3: // SLTU
      printf("sltu x%d, x%d, x%d", rd, rs1_name, rs2_name);
      riscv_write_register(machine, rd, *rs1 < *rs2);
      break;
    case 0x4: // XOR
      printf("xor x%d, x%d, x%d", rd, rs1_name, rs2_name);
      riscv_write_register(machine, rd, *rs1 ^ *rs2);
      break;
    case 0x5:               // SRL / SRA
      if (funct7 == 0x20) { // SRA
        // technically whether this is arithmetic is implementation-defined
        // TODO make this better
        printf("sra x%d, x%d, x%d", rd, rs1_name, rs2_name);
        riscv_write_register(machine, rd, (reg_t)(*rs1) >> *rs2);
      } else { // SRL
        printf("srl x%d, x%d, x%d", rd, rs1_name, rs2_name);
        riscv_write_register(machine, rd, *rs1 >> *rs2);
      }
      break;
    case 0x6: // ORI
      printf("ori x%d, x%d, x%d", rd, rs1_name, rs2_name);
      riscv_write_register(machine, rd, *rs1 | *rs2);
      break;
    case 0x7: // ANDI
      printf("andi x%d, x%d, x%d", rd, rs1_name, rs2_name);
      riscv_write_register(machine, rd, *rs1 & *rs2);
      break;
    default:
      printf("Unsupported funct3 in imm arithmetic: %x", funct3);
      break;
    }
  }
}

#ifdef RV64
void riscv_arithmetic_word(RISC_V *machine, uint32_t instruction) {
  uint8_t funct3 = riscv_get_funct3(instruction);
  ureg_t *rs1 = riscv_read_register(machine, riscv_get_rs1(instruction));
  ureg_t *rs2 = riscv_read_register(machine, riscv_get_rs2(instruction));
  uint8_t rd = riscv_get_rd(instruction);
  uint8_t funct7 = riscv_get_funct7(instruction);

  // TODO merge with arithmetic
  switch (funct3) {
  case 0x0:             // ADD/SUB
    if (funct7 == 0x20) // SUB
      riscv_write_register(machine, rd, (word_t)sub(*rs1, *rs2));
    else if (funct7 == 0x0) // ADD
      riscv_write_register(machine, rd, (word_t)add(*rs1, *rs2));
    break;
  case 0x1: // SLL
    riscv_write_register(machine, rd, (word_t)(*rs1 << *rs2));
    break;
  case 0x5:               // SRL / SRA
    if (funct7 == 0x20) { // SRA
      // technically whether this is arithmetic is implementation-defined
      // TODO make this better
      riscv_write_register(machine, rd, (word_t)((reg_t)(*rs1) >> *rs2));
    } else { // SRL
      riscv_write_register(machine, rd, (word_t)(*rs1 >> *rs2));
    }
    break;
  default:
    printf("Unsupported funct3 in imm arithmetic: %x", funct3);
    break;
  }
}
#endif

void riscv_mem_load(RISC_V *machine, uint32_t instruction) {
  uint32_t imm = riscv_get_imm_typei(instruction);
  uint8_t funct3 = riscv_get_funct3(instruction);
  uint8_t rs1_name = riscv_get_rs1(instruction);
  ureg_t *rs1 = riscv_read_register(machine, riscv_get_rs1(instruction));
  uint8_t rd = riscv_get_rd(instruction);

  switch (funct3) {
  case 0x0: // lb
    printf("lb x%d, %d(x%d)", rd, imm, rs1_name);
    riscv_write_register(machine, rd, (reg_t)machine->RAM[*rs1 + imm]);
    break;
  case 0x1: // lh
    printf("lh x%d, %d(x%d)", rd, imm, rs1_name);
    riscv_write_register(machine, rd,
                         (reg_t)(machine->RAM[*rs1 + imm] |
                                 machine->RAM[*rs1 + imm + 1] << 8));
    break;
  case 0x2: // lw
    printf("lw x%d, %d(x%d)", rd, imm, rs1_name);
    //printf("load time, addr: %x\n", add(*rs1, imm));
    riscv_write_register(machine, rd,
                         (reg_t)riscv_read_mem_32(machine, add(*rs1, imm)));
    break;
#ifdef RV64
  case 0x3: // ld
    printf("ld x%d, %d(x%d)", rd, imm, rs1_name);
    riscv_write_register(machine, rd,
                         (reg_t)riscv_read_mem_64(machine, add(*rs1, imm)));
    break;
#endif
  case 0x4: // lbu
    printf("lbu x%d, %d(x%d)", rd, imm, rs1_name);
    riscv_write_register(machine, rd, (reg_t)machine->RAM[*rs1 + imm]);
    break;
  case 0x5: // lhu
    printf("lhu x%d, %d(x%d)", rd, imm, rs1_name);
    riscv_write_register(machine, rd,
                         machine->RAM[*rs1 + imm] |
                             machine->RAM[*rs1 + imm + 1] << 8);
    break;
#ifdef RV64
  case 0x6: // lwu
    riscv_write_register(machine, rd, riscv_read_mem_32(machine, *rs1 + imm));
    break;
#endif
  default:
    printf("Unsupported funct3 in load: %x", funct3);
    break;
  }
}

void riscv_store_io(RISC_V *machine, uint32_t address, uint32_t value) {
  printf("???");
}

void riscv_store_mem(RISC_V *machine, uint32_t address, uint32_t value) {
  if (address < SIZE_MEM) {
    machine->RAM[address] = value;
  } else {
    riscv_store_io(machine, address, value);
  }
}

void riscv_store(RISC_V *machine, uint32_t instruction) {
  int32_t imm = riscv_get_imm_types(instruction);
  uint8_t funct3 = riscv_get_funct3(instruction);
  uint8_t rs1_name = riscv_get_rs1(instruction);
  uint8_t rs2_name = riscv_get_rs2(instruction);
  reg_t *rs1 = (reg_t *)riscv_read_register(machine, riscv_get_rs1(instruction));
  ureg_t *rs2 = riscv_read_register(machine, riscv_get_rs2(instruction));

  switch (funct3) {
  case 0x0: // sb
    printf("sb x%d, %d(x%d)", rs2_name, imm, rs1_name);
    riscv_store_mem(machine, *rs1 + imm, *rs2 & 0xFF);
    break;
  case 0x1: // sh
    printf("sh x%d, %d(x%d)", rs2_name, imm, rs1_name);
    riscv_store_mem(machine, *rs1 + imm, *rs2 & 0xFF);
    riscv_store_mem(machine, *rs1 + imm + 1, (*rs2 >> 8) & 0xFF);
    break;
  // TODO make this less repetitive
  case 0x2: // sw
    printf("sw x%d, %d(x%d)", rs2_name, imm, rs1_name);
    riscv_store_mem(machine, *rs1 + imm, *rs2 & 0xFF);
    riscv_store_mem(machine, *rs1 + imm + 1, (*rs2 >> 8) & 0xFF);
    riscv_store_mem(machine, *rs1 + imm + 2, (*rs2 >> 16) & 0xFF);
    riscv_store_mem(machine, *rs1 + imm + 3, (*rs2 >> 24) & 0xFF);
    break;
#ifdef RV64
  case 0x3:
    printf("sdw x%d, %d(x%d)", rs2_name, imm, rs1_name);
    machine->RAM[*rs1 + imm] = *rs2 & 0xFF;
    machine->RAM[*rs1 + imm + 1] = (*rs2 >> 8) & 0xFF;
    machine->RAM[*rs1 + imm + 2] = (*rs2 >> 16) & 0xFF;
    machine->RAM[*rs1 + imm + 3] = (*rs2 >> 24) & 0xFF;
    machine->RAM[*rs1 + imm + 4] = (*rs2 >> 32) & 0xFF;
    machine->RAM[*rs1 + imm + 5] = (*rs2 >> 40) & 0xFF;
    machine->RAM[*rs1 + imm + 6] = (*rs2 >> 48) & 0xFF;
    machine->RAM[*rs1 + imm + 7] = (*rs2 >> 56) & 0xFF;
    break;
#endif
  default:
    printf("Unsupported funct3 in store: %x", funct3);
    break;
  }
}

void riscv_execute_instruction(RISC_V *machine, uint32_t instruction) {
  if (instruction == 0) {
    printf("Illegal instruction encountered: %d", instruction);
    return;
  }

  uint8_t opcode = riscv_get_opcode(instruction);
  ureg_t rd = riscv_get_rd(instruction);
  ureg_t rs1 = riscv_get_rs1(instruction);
  switch (opcode) {
  case 0x3: // loads
    riscv_mem_load(machine, instruction);
    break;
  case 0x13: // Arithmetic with imm RHS
    riscv_immediate_arithmetic(machine, instruction);
    break;
  case 0x17: // auipc
    printf("auipc x%d, %d", rd, riscv_get_imm_typeu(instruction));
    riscv_write_register(machine, rd,
                         add(machine->pc, riscv_get_imm_typeu(instruction)));
    break;
#ifdef RV64
  case 0x1B: // immw
    riscv_immediate_arithmetic_word(machine, instruction);
    break;
#endif
  case 0x23: // stores
    riscv_store(machine, instruction);
    break;
  case 0x33: // Arithmetic with register RHS
    riscv_arithmetic(machine, instruction);
    break;
  case 0x37: // lui
    printf("lui x%d, %d", rd, riscv_get_imm_typeu(instruction));
    riscv_write_register(machine, rd, riscv_get_imm_typeu(instruction));
    break;
  case 0x63: // branches
    riscv_branch(machine, instruction);
    break;
#ifdef RV64
  case 0x3B:
    riscv_arithmetic_word(machine, instruction);
    break;
#endif
  case 0x67: // jalr
    printf("jalr x%d, x%d, %d", rd, rs1, riscv_get_imm_typei(instruction));
    riscv_write_register(machine, rd, machine->pc + 4);
    riscv_write_pc(machine, riscv_get_imm_typei(instruction) + (reg_t)*riscv_read_register(machine, rs1));
    break;
  case 0x6F: // jal
    printf("jal x%d, %d", rd, riscv_get_imm_typej(instruction));
    riscv_write_register(machine, rd, machine->pc + 4);
    riscv_write_pc(machine, machine->pc + riscv_get_imm_typej(instruction));
    break;
  default:
    printf("Unsupported opcode %x\n", opcode);
  }
}

void riscv_execute() {
  RISC_V *machine = riscv_new();
  for (uint32_t i = 0; i < 5000; i++) {
    if (i != 0 && !jumped)
      riscv_write_pc(machine, machine->pc + 4);
    jumped = false;
    uint32_t instruction = program[machine->pc / 4];
    printf("INSTRUCTION:\t");
    riscv_execute_instruction(machine, instruction);
    printf(" [%08x]", instruction);
    riscv_print_state(machine);
  }
}

ureg_t *riscv_read_register(RISC_V *machine, uint8_t reg) {
  return &machine->registers[reg];
}

void riscv_write_register(RISC_V *machine, uint8_t reg, ureg_t value) {
  if (reg != 0) {
    machine->registers[reg] = value;
  }
}

void riscv_write_pc(RISC_V *machine, ureg_t value) {
  // check if it actually jumps anywhere
  if (machine->pc != value) {
    jumped = true;
    machine->pc = value;
  }
}

void riscv_print_state(RISC_V *machine) {
  printf("\nState:\nPC: 0x%x\n", machine->pc);
  for (uint8_t i = 0; i < NUM_REGS; i++) {
    if (machine->registers[i] != 0)
      printf("Reg %d: 0x%x\n", i, machine->registers[i]);
  }
  for (uint32_t i = 0; i < SIZE_MEM; i += 1) {
    // int32_t m = ((int32_t *)(machine->RAM))[i];
    uint8_t m = machine->RAM[i];
    if (m != 0)
      printf("Mem %x: 0x%x\n", i, m);
  }
  printf("\n");
}
