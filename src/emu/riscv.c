#include "riscv.h"

#include <stdbool.h>
#include <string.h>
#include <stdarg.h>

//------------------------------------------------------------------------
// Copyright (c) 2020 Ted Fried
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.
//
//------------------------------------------------------------------------

#define U_immediate instruction >> 12
#define J_immediate_SE (instruction&0x80000000) ? 0xFFE00000 | (instruction&0x000FF000) | (instruction&0x00100000)>>9 | (instruction&0x80000000)>>11 | (instruction&0x7FE00000)>>20 : (instruction&0x000FF000) | (instruction&0x00100000)>>9 | (instruction&0x80000000)>>11 | (instruction&0x7FE00000)>>20  
#define B_immediate_SE (instruction&0x80000000) ? 0xFFFFE000 | (instruction&0xF00)>>7 | (instruction&0x7E000000)>>20 | (instruction&0x80)<<4 | (instruction&0x80000000)>> 19 : (instruction&0xF00)>>7 | (instruction&0x7E000000)>>20 | (instruction&0x80)<<4 | (instruction&0x80000000)>> 19
#define I_immediate_SE (instruction&0x80000000) ? 0xFFFFF000 | instruction >> 20 : instruction >> 20
#define S_immediate_SE (instruction&0x80000000) ? 0xFFFFF000 | (instruction&0xFE000000)>>20 | (instruction&0xF80)>>7 : (instruction&0xFE000000)>>20 | (instruction&0xF80)>>7

#define funct7 ((unsigned char) ((instruction&0xFE000000) >> 25) )
#define rs2 ((unsigned char) ((instruction&0x01F00000) >> 20) )
#define rs1 ((unsigned char) ((instruction&0x000F8000) >> 15) )
#define funct3 ((unsigned char) ((instruction&0x00007000) >> 12) )
#define rd ((unsigned char) ((instruction&0x00000F80) >> 7 ) )
#define opcode ((instruction&0x0000007F) )

uint8_t shamt;
uint32_t instruction;
uint32_t temp;
bool logging = false;
bool terminate = false;

static const uint32_t program[ROMWords] = {
#include "bootloader.inc"
};

void write_log(const char *format, ...)
{
  va_list args;
  va_start(args, format);

  if(logging)
    vprintf(format, args);

  va_end(args);
}

CPU *riscv_new() {
  CPU *machine = malloc(sizeof(CPU));
  if (machine == NULL)
    exit(2);

  machine->mem_size = DefaultMemSize;
  machine->num_regs = 32;
  machine->registers = malloc(machine->num_regs * sizeof(ureg_t));
  machine->registers[0] = 0;

  machine->display_start = DefaultDisplayStart;
  machine->fb_width = RISC_FRAMEBUFFER_WIDTH / 32;
  machine->fb_height = RISC_FRAMEBUFFER_HEIGHT;
  machine->damage = (struct Damage){
    .x1 = 0,
    .y1 = 0,
    .x2 = machine->fb_width - 1,
    .y2 = machine->fb_height - 1
  };
  riscv_reset(machine);
  machine->RAM = calloc(1, machine->mem_size);
  memcpy(machine->ROM, program, sizeof(machine->ROM));

  machine->stack_trace = malloc(TRACE_SIZE * sizeof(Trace));
  for (int i = 0; i < TRACE_SIZE; i++) {
    machine->stack_trace[i] = (Trace){ .file = "", .pos = 0, .file_pos = 0 };
  }
  machine->stack_index = 0;
  return machine;
}


void riscv_execute(CPU *machine, uint32_t cycles) {
  machine->progress = 20;
  for (uint32_t i = 0; i < cycles && machine->progress; i++) {
    if (machine->pc < machine->mem_size) {
      instruction = machine->RAM[machine->pc / 4];
    } else if (machine->pc >= ROMStart) {
      instruction = machine->ROM[(machine->pc - ROMStart) / 4];
    } else {
      printf("Panic! PC = %0x", machine->pc);
      terminate = true;
    }
    shamt=rs2;

    write_log("PC:0x%x\n", machine->pc);
    int insttype = 0;
    switch (opcode) {
      case 0b0110011:
        insttype = 1; // R
        break;
      case 0b1100111:
      case 0b0000011:
      case 0b0010011:
        insttype = 2; // I
        break;
      case 0b0100011:
        insttype = 3; // S
        break;
      case 0b1100011:
        insttype = 4; // B
        break;
      case 0b0110111:
      case 0b0010111:
        insttype = 5; // U
        break;
      case 0b1101111:
        insttype = 6; // J
        break;
    }
 
    write_log("INSTRUCTION:\t");
    // https://github.com/MicroCoreLabs/Projects/blob/master/RISCV_C_Version/C_Version/riscv.c
    if (opcode==0b0110111) { machine->registers[rd] = U_immediate << 12; write_log(" LUI "); } else // LUI
    if (opcode==0b0010111) { machine->registers[rd] = (U_immediate << 12) + machine->pc; write_log(" AUIPC "); } else // AUIPC
    if (opcode==0b1101111) { machine->registers[rd] = machine->pc + 0x4; machine->pc = (J_immediate_SE) + machine->pc - 0x4; write_log(" JAL "); } else // JAL
    if (opcode==0b1100111) { machine->registers[rd] = machine->pc + 0x4; machine->pc = (((I_immediate_SE) + machine->registers[rs1]) & 0xFFFFFFFE) - 0x4; write_log(" JALR "); } else // JALR
    if (opcode==0b1100011 && funct3==0b000) { if (machine->registers[rs1]==machine->registers[rs2]) machine->pc = ( (B_immediate_SE) + machine->pc) - 0x4; write_log(" BEQ "); } else // BEQ
    if (opcode==0b1100011 && funct3==0b001) { if (machine->registers[rs1]!=machine->registers[rs2]) machine->pc = ( (B_immediate_SE) + machine->pc) - 0x4; write_log(" BNE "); } else // BNE
    if (opcode==0b1100011 && funct3==0b100) { if ((int32_t)machine->registers[rs1]< (int32_t)machine->registers[rs2]) machine->pc = ((B_immediate_SE) + machine->pc) - 0x4; write_log(" BLT "); } else // BLT
    if (opcode==0b1100011 && funct3==0b101) { if ((int32_t)machine->registers[rs1]>=(int32_t)machine->registers[rs2]) machine->pc = ((B_immediate_SE) + machine->pc) - 0x4; write_log(" BGE "); } else // BGE
    if (opcode==0b1100011 && funct3==0b110) { if (machine->registers[rs1]<machine->registers[rs2]) machine->pc = ( (B_immediate_SE) + machine->pc) - 0x4; write_log(" BLTU ");  } else // BLTU
    if (opcode==0b1100011 && funct3==0b111) { if (machine->registers[rs1]>=machine->registers[rs2]) machine->pc = ( (B_immediate_SE) + machine->pc) - 0x4; write_log(" BGTU "); } else // BGTU
    if (opcode==0b0000011 && funct3==0b000) {
      addr_t addr = (I_immediate_SE)+machine->registers[rs1];
      uint32_t data = (riscv_load(machine, addr) >> ((addr % 4) * 8)) & 0xFF;
      machine->registers[rd] = data & 0x80 ? 0xFFFFFF00 | data : data; write_log(" LB "); // LB
    } else 
    if (opcode==0b0000011 && funct3==0b001) {
      addr_t addr = (I_immediate_SE)+machine->registers[rs1];
      machine->registers[rd] = (riscv_load(machine, addr) & 0x8000) ? 0xFFFF0000| (riscv_load(machine,addr) >> ((addr%4) * 8)) : ((riscv_load(machine,addr) >> ((addr%4) * 8)) & 0xFFFF); write_log(" LH ");
    } else // LH
    if (opcode==0b0000011 && funct3==0b010) { machine->registers[rd] = riscv_load(machine, (I_immediate_SE)+machine->registers[rs1]); write_log(" LW "); } else // LW
    if (opcode==0b0000011 && funct3==0b100) {
      addr_t addr = (I_immediate_SE)+machine->registers[rs1];
      uint32_t data = (riscv_load(machine, addr) >> ((addr % 4) * 8)) & 0xFF;
      machine->registers[rd] = data; write_log(" LBU "); // LBU
    } else 
    if (opcode==0b0000011 && funct3==0b101) { machine->registers[rd] = riscv_load(machine, (I_immediate_SE)+machine->registers[rs1]) >> ((((I_immediate_SE)+machine->registers[rs1])%4) * 8) & 0x0000FFFF; write_log(" LHU "); } else // LHU
    if (opcode==0b0100011 && funct3==0b000) {
      addr_t addr = (S_immediate_SE)+machine->registers[rs1];
      word_t data = (riscv_load(machine,addr));
      word_t shamt = (addr % 4) * 8;
      riscv_store(machine, addr, (data & (0xFFFFFFFF ^ (0xFF << shamt))) | (((machine->registers[rs2]&0xFF) << shamt))); write_log(" SB ");
    } else // SB
    if (opcode==0b0100011 && funct3==0b001) { riscv_store(machine,(S_immediate_SE)+machine->registers[rs1], (riscv_load(machine,(S_immediate_SE)+machine->registers[rs1])&0xFFFF0000) | (machine->registers[rs2]&0xFFFF)); write_log(" SH "); } else // SH
    if (opcode==0b0100011 && funct3==0b010) { riscv_store(machine,(S_immediate_SE)+machine->registers[rs1], machine->registers[rs2]); write_log(" SW "); } else // SW
    if (opcode==0b0010011 && funct3==0b000) { machine->registers[rd] = (I_immediate_SE) + machine->registers[rs1]; write_log(" ADDI "); } else // ADDI
    if (opcode==0b0010011 && funct3==0b010) { if ((int32_t)machine->registers[rs1] < ((int32_t)I_immediate_SE)) machine->registers[rd]=1; else machine->registers[rd]=0; write_log(" SLTI "); } else // SLTI
    if (opcode==0b0010011 && funct3==0b011) { if (machine->registers[rs1] < (I_immediate_SE)) machine->registers[rd]=1; else machine->registers[rd]=0; write_log(" SLTIU "); } else // SLTIU
    if (opcode==0b0010011 && funct3==0b100) { machine->registers[rd] = machine->registers[rs1] ^ (I_immediate_SE); write_log(" XORI "); } else // XORI
    if (opcode==0b0010011 && funct3==0b110) { machine->registers[rd] = machine->registers[rs1] | (I_immediate_SE); write_log(" ORI "); } else // ORI
    if (opcode==0b0010011 && funct3==0b111) { machine->registers[rd] = machine->registers[rs1] & (I_immediate_SE); write_log(" ANDI "); } else // ANDI
    if (opcode==0b0010011 && funct3==0b001 && funct7==0b0000000) { machine->registers[rd] = machine->registers[rs1] << shamt; write_log(" SLLI "); } else // SLLI
    if (opcode==0b0010011 && funct3==0b101 && funct7==0b0100000) {machine->registers[rd]=machine->registers[rs1]; temp=machine->registers[rs1]&0x80000000; while (shamt>0) { machine->registers[rd]=(machine->registers[rd]>>1)|temp; shamt--;} write_log(" SRAI "); } else // SRAI
    if (opcode==0b0010011 && funct3==0b101 && funct7==0b0000000) { machine->registers[rd] = machine->registers[rs1] >> shamt; write_log(" SRLI "); } else // SRLI
    if (opcode==0b0110011 && funct3==0b000 && funct7==0b0000001) { machine->registers[rd] = (int32_t)machine->registers[rs1] * (int32_t)machine->registers[rs2]; write_log(" MUL "); } else // MUL
    if (opcode==0b0110011 && funct3==0b100 && funct7==0b0000001) { machine->registers[rd] = (int32_t)machine->registers[rs1] / (int32_t)machine->registers[rs2]; write_log(" DIV "); } else // DIV

    if (opcode==0b0110011 && funct3==0b110 && funct7==0b0000001) { machine->registers[rd] = machine->registers[rs1] % machine->registers[rs2]; write_log(" REM "); } else // REM
    if (opcode==0b0110011 && funct3==0b000 && funct7==0b0100000) { machine->registers[rd] = machine->registers[rs1] - machine->registers[rs2]; write_log(" SUB "); } else // SUB
    if (opcode==0b0110011 && funct3==0b000 && funct7==0b0000000) { machine->registers[rd] = machine->registers[rs1] + machine->registers[rs2]; write_log(" ADD "); } else // ADD
    if (opcode==0b0110011 && funct3==0b001 && funct7==0b0000000) { machine->registers[rd] = machine->registers[rs1] << (machine->registers[rs2]&0x1F); write_log(" SLL "); } else // SLL
    if (opcode==0b0110011 && funct3==0b010 && funct7==0b0000000) { if ((int32_t)machine->registers[rs1] < (int32_t)machine->registers[rs2]) machine->registers[rd]=1; else machine->registers[rd]=0; write_log(" SLT "); } else // SLT
    if (opcode==0b0110011 && funct3==0b011 && funct7==0b0000000) { if (machine->registers[rs1] < machine->registers[rs2]) machine->registers[rd]=1; else machine->registers[rd]=0; write_log(" SLTU "); } else // SLTU
    if (opcode==0b0110011 && funct3==0b100 && funct7==0b0000000) { machine->registers[rd] = machine->registers[rs1] ^ machine->registers[rs2]; write_log(" XOR "); } else // XOR
    if (opcode==0b0110011 && funct3==0b101 && funct7==0b0100000) {machine->registers[rd]=machine->registers[rs1]; shamt=(machine->registers[rs2]&0x1F); temp=machine->registers[rs1]&0x80000000; while (shamt>0) { machine->registers[rd]=(machine->registers[rd]>>1)|temp; shamt--;} write_log(" SRA "); } else // SRA
    if (opcode==0b0110011 && funct3==0b101 && funct7==0b0000000) { machine->registers[rd] = machine->registers[rs1] >> (machine->registers[rs2]&0x1F); write_log(" SRL "); } else // SRL
    if (opcode==0b0110011 && funct3==0b110 && funct7==0b0000000) { machine->registers[rd] = machine->registers[rs1] | machine->registers[rs2]; write_log(" OR "); } else // OR
    if (opcode==0b0110011 && funct3==0b111 && funct7==0b0000000) { machine->registers[rd] = machine->registers[rs1] & machine->registers[rs2]; write_log(" AND "); } else write_log(" **INVALID** "); // AND

    machine->pc = machine->pc + 0x4;
    machine->registers[0]=0;

    switch (insttype) {
      case 1:
        write_log("x%d x%d x%d", rd, rs1, rs2);
        break;
      case 2:
        write_log("x%d x%d %d", rd, rs1, I_immediate_SE);
        break;
      case 3:
        write_log("x%d %d(x%d)", rs2, S_immediate_SE, rs1);
        write_log(" Store to 0x%x of value 0x%x", (S_immediate_SE) + machine->registers[rs1], machine->registers[rs2]);
        break;
      case 4:
        write_log("x%d x%d %d", rs1, rs2, B_immediate_SE);
        break;
      case 5:
        write_log("x%d %d", rd, U_immediate);
        break;
      case 6:
        write_log("x%d %d", rd, J_immediate_SE);
        if (rd == 0 && (J_immediate_SE) == 0) { terminate = true; }
        break;
      default: printf("invalid insttype\n"); write_log(" [%08x]", instruction); terminate = true;
    }
    write_log(" [%08x]", instruction);
    write_log("\n");

    //write_log("rd:%d rs1:%d rs2:%d U_immediate:0x%x J_immediate:0x%x B_immediate:0x%x I_immediate:0x%x S_immediate:0x%x funct3:0x%x funct7:0x%x\n",rd,rs1,rs2,U_immediate,J_immediate_SE,B_immediate_SE,I_immediate_SE,S_immediate_SE,funct3,funct7);
    write_log("Regs:\n"); for (int i=0; i<32; i++) { if (machine->registers[i] != 0) write_log("x%d: 0x%x\n",i,machine->registers[i]); } write_log("\n"); 
    //printf("Memory: "); for (int i=0; i<7; i++) { printf("Addr%d:%x ",i,machine->RAM[i]); } printf("\n");
    if (terminate) {
      write_log("Stack trace:\n");
      riscv_print_trace(machine);

      write_log("Mem:\n");
      for (int i=0; i<machine->mem_size/4; i++) { if (machine->RAM[i] != 0) write_log("%x: 0x%x\n",i*4,machine->RAM[i]); } write_log("\n"); 

      exit(1);
    }
  }
}
