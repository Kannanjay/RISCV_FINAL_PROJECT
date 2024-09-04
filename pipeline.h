#ifndef __PIPELINE_H__
#define __PIPELINE_H__

#include "config.h"
#include "types.h"
#include "cache.h"
#include <stdbool.h>

//#define DEBUG_CYCLE_CONTENTS
#define DEBUG_HAZARD_DETECTION

///////////////////////////////////////////////////////////////////////////////
/// Functionality
///////////////////////////////////////////////////////////////////////////////

extern simulator_config_t sim_config;
extern uint64_t miss_count;
extern uint64_t hit_count;
extern uint64_t total_cycle_counter;
extern uint64_t mem_access_counter;
extern uint64_t stall_counter;
extern uint64_t branch_counter;
extern uint64_t fwd_exex_counter;
extern uint64_t fwd_exmem_counter;

///////////////////////////////////////////////////////////////////////////////
/// RISC-V Pipeline Register Types
///////////////////////////////////////////////////////////////////////////////

typedef struct
{
  Instruction instr;
  uint32_t    instr_addr; // instruction address
  /**
   * Add other fields here
   */

}ifid_reg_t;

typedef struct
{
  Instruction instr;
  uint32_t    instr_addr;
  /**
   * Add other fields here
   */

  // rs1 and rs2 values
  uint32_t read_data1;
  uint32_t read_data2;

  // immediate value
  uint32_t imm_val;
  
  // On pipeline diagram, Instruction [30, 25, 14-12]
  uint8_t funct3;
  //uint8_t funct7;
  uint8_t funct7_30;
  // Used for recognizing multiply extension of risc-v
  uint8_t funct7_25;

  // On pipeline diagram, Instruction [11-7]
  uint8_t write_reg; 
  uint8_t rd;

  // Write-back stage control lines
  bool reg_write;
  bool mem_to_reg;

  // Memory access stage control lines
  bool mem_read;
  bool mem_write;
  bool branch;

  // Execution/address calculation stage control lines
  uint8_t alu_op0;
  uint8_t alu_op1;
  uint8_t alu_op2;
  bool alu_src;

  // Hazard detection helpers
  uint8_t rs1;
  uint8_t rs2;

}idex_reg_t;

typedef struct
{
  Instruction instr;
  uint32_t    instr_addr;
  /**
   * Add other fields here
   */

  // Result of the add block above the main ALU
  uint32_t branch_addr;

  // ALU result
  uint32_t alu_result;

  // zero control line that comes out of the ALU on the pipeline diagram
  uint8_t zero;

  // rs2 value, naming is confusing but just following what it is called on the pipeline diagram
  uint32_t write_data;

  // On pipeline diagram, Instruction [11-7]
  uint8_t write_reg; 
  uint8_t rd;

  // Write-back stage control lines
  bool reg_write;
  bool mem_to_reg;

  // Memory access stage control lines
  bool mem_read;
  bool mem_write;
  bool branch;

}exmem_reg_t;

typedef struct
{
  Instruction instr;
  uint32_t    instr_addr;
  /**
   * Add other fields here
   */

  // output of the data memory block
  uint32_t read_data;
 
  // ALU result that bypasses data memory block
  uint32_t alu_result;

  // On pipeline diagram, Instruction [11-7]
  uint8_t write_reg;
  uint8_t rd;
  
  // Write-back stage control lines
  bool reg_write;
  bool mem_to_reg;

}memwb_reg_t;


///////////////////////////////////////////////////////////////////////////////
/// Register types with input and output variants for simulator
///////////////////////////////////////////////////////////////////////////////

typedef struct
{
  ifid_reg_t inp;
  ifid_reg_t out;
}ifid_reg_pair_t;

typedef struct
{
  idex_reg_t inp;
  idex_reg_t out;
}idex_reg_pair_t;

typedef struct
{
  exmem_reg_t inp;
  exmem_reg_t out;
}exmem_reg_pair_t;

typedef struct
{
  memwb_reg_t inp;
  memwb_reg_t out;
}memwb_reg_pair_t;

///////////////////////////////////////////////////////////////////////////////
/// Functional pipeline requirements
///////////////////////////////////////////////////////////////////////////////

typedef struct
{
  ifid_reg_pair_t  ifid_preg;
  idex_reg_pair_t  idex_preg;
  exmem_reg_pair_t exmem_preg;
  memwb_reg_pair_t memwb_preg;
}pipeline_regs_t;

typedef struct
{
  bool      pcsrc;
  uint32_t  pc_src0;
  uint32_t  pc_src1;
  /**
   * Add other fields here
   */

  // Technically go accross stages, but not sure if needed
  uint8_t write_reg;

  // Forwarding control signals
  uint8_t forward_a;
  uint8_t forward_b;

  // Hazard detection
  uint32_t write_data;
  uint32_t alu_result;
  bool flush_control;
  bool mem_read;
  bool ifid_write;
  bool pc_write;

  uint8_t rs1;
  uint8_t rs2;
  uint8_t rd;
  

}pipeline_wires_t;


///////////////////////////////////////////////////////////////////////////////
/// Function definitions for different stages
///////////////////////////////////////////////////////////////////////////////

/**
 * output : ifid_reg_t
 **/ 
ifid_reg_t stage_fetch(pipeline_wires_t* pwires_p, regfile_t* regfile_p, Byte* memory_p);

/**
 * output : idex_reg_t
 **/ 
idex_reg_t stage_decode(ifid_reg_t ifid_reg, pipeline_wires_t* pwires_p, regfile_t* regfile_p);

/**
 * output : exmem_reg_t
 **/ 
exmem_reg_t stage_execute(idex_reg_t idex_reg, pipeline_wires_t* pwires_p);

/**
 * output : memwb_reg_t
 **/ 
memwb_reg_t stage_mem(exmem_reg_t exmem_reg, pipeline_wires_t* pwires_p, Byte* memory, Cache* cache_p);

/**
 * output : write_data
 **/ 
void stage_writeback(memwb_reg_t memwb_reg, pipeline_wires_t* pwires_p, regfile_t* regfile_p);

void cycle_pipeline(regfile_t* regfile_p, Byte* memory_p, Cache* cache_p, pipeline_regs_t* pregs_p, pipeline_wires_t* pwires_p, bool* ecall_exit);

void bootstrap(pipeline_wires_t* pwires_p, pipeline_regs_t* pregs_p, regfile_t* regfile_p);

#endif  // __PIPELINE_H__
