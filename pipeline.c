#include <stdbool.h>
#include "cache.h"
#include "riscv.h"
#include "types.h"
#include "utils.h"
#include "pipeline.h"
#include "stage_helpers.h"

uint64_t total_cycle_counter = 0;
uint64_t mem_access_counter = 0;
uint64_t miss_count = 0;
uint64_t hit_count = 0;
uint64_t stall_counter = 0;
uint64_t branch_counter = 0;
uint64_t fwd_exex_counter = 0;
uint64_t fwd_exmem_counter = 0;

simulator_config_t sim_config = {0};

///////////////////////////////////////////////////////////////////////////////

void bootstrap(pipeline_wires_t* pwires_p, pipeline_regs_t* pregs_p, regfile_t* regfile_p)
{
  // PC src must get the same value as the default PC value
  pwires_p->pc_src0 = regfile_p->PC;
}

///////////////////////////
/// STAGE FUNCTIONALITY ///
///////////////////////////

/**
 * STAGE  : stage_fetch
 * output : ifid_reg_t
 **/ 
ifid_reg_t stage_fetch(pipeline_wires_t* pwires_p, regfile_t* regfile_p, Byte* memory_p)
{
  ifid_reg_t ifid_reg = {0};  // Initialize the pipeline register
  /**
   * YOUR CODE HERE 
   */

  uint32_t instruction_bits;  // initialize the bits containing the instruction

  // MUX logic in the IF stage
  if (pwires_p->p csrc) {  // branch taken ->  address to which the PC should jump to
    regfile_p->PC = pwires_p->pc_src1;  // set to target address of the branch
    pwires_p->pc_src0 = pwires_p->pc_src1;
  } else {  // branch not taken
    regfile_p->PC = pwires_p->pc_src0;  // set to next sequential address (pc+4)
  }

  // Check for Hazard
  if (pwires_p->pc_write == 1) { // Hazard detected

    // Decrement PC counter in order to fetch previous instruction
    regfile_p->PC = regfile_p->PC - 4;

    // Set pc_src0 to same decremented value of PC counter
    pwires_p->pc_src0 = regfile_p->PC;

    // Reset control signal
    pwires_p->pc_write = 0;
  }
 
  // Pass on current instruction address to if/id pipeline register
  ifid_reg.instr_addr = regfile_p->PC;
 
  // Increment pc counter for next cycle (Add block above instruction memory)
  pwires_p->pc_src0 += 4;
  
  // load instruction from memory and parse it
  instruction_bits = load(memory_p, regfile_p->PC, LENGTH_WORD);
  ifid_reg.instr = parse_instruction(instruction_bits);
  
  #ifdef DEBUG_CYCLE
  printf("[IF ]: Instruction [%08x]@[%08x]: ", instruction_bits, regfile_p->PC);
  decode_instruction(instruction_bits);
  #endif

  return ifid_reg;
}

/**
 * STAGE  : stage_decode
 * output : idex_reg_t
 **/ 
idex_reg_t stage_decode(ifid_reg_t ifid_reg, pipeline_wires_t* pwires_p, regfile_t* regfile_p)
{
  idex_reg_t idex_reg = {0};
  /**
   * YOUR CODE HERE
   */

  // Generate control logic (Control module)
  idex_reg = gen_control(ifid_reg.instr);

  // Determine if there is a load-use data hazard (MUX after control module)
  if (pwires_p->flush_control == 1) { // Data hazard detected

    // Flush control signals that have been generated
    idex_reg.alu_op2 = 0;
    idex_reg.alu_op1 = 0;
    idex_reg.alu_op0 = 0;
    idex_reg.alu_src = 0;
    idex_reg.branch = 0;
    idex_reg.mem_read = 0;
    idex_reg.mem_write = 0;
    idex_reg.reg_write = 0;
    idex_reg.mem_to_reg = 0;

    // Reset control signal
    pwires_p->flush_control = 0;
  } 
  
  // Carry over instruction and PC from one pipeline reg to the next
  idex_reg.instr = ifid_reg.instr;
  idex_reg.instr_addr = ifid_reg.instr_addr;

  // Generate imm value (Imm Gen module)
  idex_reg.imm_val = gen_imm(ifid_reg.instr);

  // Carry over rd address (Bottom of pipeline image, instruction[11-7])
  idex_reg.rd = ifid_reg.instr.rest & ((1U << 5) - 1);

  // Carry over rs1 and rs2 address for hazard detection in the forwarding unit
  idex_reg.rs1 = (ifid_reg.instr.rest >> 8) & ((1U << 5) - 1);
  idex_reg.rs2 = (ifid_reg.instr.rest >> 13) & ((1U << 5) - 1);

  // Determine instruction type and read appropriate registers
  switch(ifid_reg.instr.opcode) {
    case 0x33:  //R-type
      // rd, rs1, rs2
      //idex_reg.rd = ifid_reg.instr.rtype.rd;
      idex_reg.read_data1 = regfile_p->R[ifid_reg.instr.rtype.rs1];
      idex_reg.read_data2 = regfile_p->R[ifid_reg.instr.rtype.rs2];
      idex_reg.funct3 = ifid_reg.instr.rtype.funct3;
      idex_reg.funct7_30 = (ifid_reg.instr.rtype.funct7 << 1) >> 6;
      idex_reg.funct7_25 = ifid_reg.instr.rtype.funct7 & 1U;
      break;
    case 0x13:  //I-type, except load
      // rd, rs1, imm
      //idex_reg.rd = ifid_reg.instr.itype.rd;
      idex_reg.read_data1 = regfile_p->R[ifid_reg.instr.itype.rs1];  
      idex_reg.funct3 = ifid_reg.instr.itype.funct3;
      idex_reg.funct7_30 = (ifid_reg.instr.itype.imm << 1) >> 11;  
      idex_reg.funct7_25 = (ifid_reg.instr.itype.imm >> 4) & 0x1;

      idex_reg.rs2 = 0;
      break;
    case 0x3:   //Load
      // rd, offset, rs1
      //idex_reg.rd = ifid_reg.instr.itype.rd;
      idex_reg.read_data1 = regfile_p->R[ifid_reg.instr.itype.rs1];
      idex_reg.funct3 = ifid_reg.instr.itype.funct3;

      idex_reg.rs2 = 0;
      break;
    case 0x23:  //S-type
      // rs2, offset, rs1
      idex_reg.read_data1 = regfile_p->R[ifid_reg.instr.stype.rs1];
      idex_reg.read_data2 = regfile_p->R[ifid_reg.instr.stype.rs2];
      idex_reg.funct3 = ifid_reg.instr.stype.funct3;
      break;
    case 0x63:  //SB-type
      // rs1, rs2, offset
      idex_reg.read_data1 = regfile_p->R[ifid_reg.instr.sbtype.rs1];
      idex_reg.read_data2 = regfile_p->R[ifid_reg.instr.sbtype.rs2];
      idex_reg.funct3 = ifid_reg.instr.sbtype.funct3;
      break;
    case 0x37:  //U-type
      // rd, offset
      //idex_reg.rd = ifid_reg.instr.utype.rd;
      idex_reg.rs1 = 0;     
      idex_reg.rs2 = 0; 
      break;
    case 0x6F:  //UJ-type
      // rd, imm
      //idex_reg.rd = ifid_reg.instr.ujtype.rd;
      idex_reg.read_data1 = ifid_reg.instr_addr;

      idex_reg.rs1 = 0;     
      idex_reg.rs2 = 0; 
      break; 
    default: 
      break;
  }

  #ifdef DEBUG_CYCLE
  printf("[ID ]: Instruction [%08x]@[%08x]: ", ifid_reg.instr.bits, ifid_reg.instr_addr);
  decode_instruction(ifid_reg.instr.bits);
  #endif

  return idex_reg;
}

/**
 * STAGE  : stage_execute
 * output : exmem_reg_t
 **/ 
exmem_reg_t stage_execute(idex_reg_t idex_reg, pipeline_wires_t* pwires_p)
{
  exmem_reg_t exmem_reg = {0};
  /**
   * YOUR CODE HERE
   */

  // Variables used in execute stage (could probably go without using them,
  // but this increases readability a bit)
  int32_t alu_inp1;
  int32_t alu_inp2;
  uint32_t alu_control;
  //uint32_t alu_result;
  //uint8_t zero;
  
  //////////////// Carry over control wires //////////////////
  // Write-back stage control lines
  exmem_reg.reg_write = idex_reg.reg_write;
  exmem_reg.mem_to_reg = idex_reg.mem_to_reg;
  // Memory access stage control lines
  exmem_reg.mem_read = idex_reg.mem_read;
  exmem_reg.mem_write = idex_reg.mem_write;
  exmem_reg.branch = idex_reg.branch;
  ////////////////////////////////////////////////////////////

  // Carry over instruction and PC from one pipeline reg to the next
  exmem_reg.instr = idex_reg.instr;
  exmem_reg.instr_addr = idex_reg.instr_addr;

  // Carry over write_reg (On pipeline diagram, the bottom most data path, Instruction [11-7])
  exmem_reg.rd = idex_reg.rd;

  // Calculate branch address (add block above the ALU)
  exmem_reg.branch_addr = idex_reg.instr_addr + idex_reg.imm_val; 

  // MUXs before both alu inputs for hazard detection (forwarding)
  if (pwires_p->forward_a == 0x1) {
    alu_inp1 = pwires_p->write_data;  // input comes from MEM/WB stage
  } else if (pwires_p->forward_a == 0x2) {
    alu_inp1 = pwires_p->alu_result;  // input comes from EX/MEM stage
  } else {
    // Set alu_inp1 to rs1 value
    alu_inp1 = idex_reg.read_data1;
  }
  
  // MUX before alu that determines if a immediate value or rs2 value is used for alu_inp2
  if (pwires_p->forward_b == 0x1) {
    alu_inp2 = pwires_p->write_data;  // input comes from MEM/WB stage
  } else if (pwires_p->forward_b == 0x2) {
    alu_inp2 = pwires_p->alu_result;  // input comes from EX/MEM stage
  } else {
    alu_inp2 = idex_reg.read_data2;
  }
    
  // Pass on value for write_data in mem stage
  exmem_reg.write_data = alu_inp2;

  // ALU source MUX
  // If immediate value is used (alu_src == true), then that overwrites the alu_inp2
  if (idex_reg.alu_src == true) { 
    alu_inp2 = idex_reg.imm_val;
  } 

  // Generate ALU control signals (ALU control module)
  alu_control = gen_alu_control(idex_reg);

  // Execute ALU with generated control signal, storing in ex/mem reg
  exmem_reg.alu_result = execute_alu(alu_inp1, alu_inp2, alu_control);
  
  // Carry over result of alu to pwires for use in the Forwarding unit
  pwires_p->alu_result = exmem_reg.alu_result;

  // Set zero signal of ALU (Dealt with in stage_mem() for now)

  #ifdef DEBUG_CYCLE_CONTENTS
  printf("[EX ]: alu_result [%08x], inp1: [%08x], inp2: [%08x], imm_val: [%08x], branch address: [%08x]: ", 
  alu_result, alu_inp1, alu_inp2, idex_reg.imm_val, exmem_reg.branch_addr);
  decode_instruction(idex_reg.instr.bits);
  #endif

  #ifdef DEBUG_CYCLE
  printf("[EX ]: Instruction [%08x]@[%08x]: ", idex_reg.instr.bits, idex_reg.instr_addr);
  decode_instruction(idex_reg.instr.bits);
  #endif

  return exmem_reg;
}

/**
 * STAGE  : stage_mem
 * output : memwb_reg_t
 **/ 
memwb_reg_t stage_mem(exmem_reg_t exmem_reg, pipeline_wires_t* pwires_p, Byte* memory_p, Cache* cache_p)
{
  memwb_reg_t memwb_reg = {0};
  /**
   * YOUR CODE HERE
   */

  //////////////// Carry over control wires //////////////////
  // Write-back stage control lines
  memwb_reg.reg_write = exmem_reg.reg_write;
  memwb_reg.mem_to_reg = exmem_reg.mem_to_reg;
  ////////////////////////////////////////////////////////////

  // Carry over instruction and PC from one pipeline reg to the next
  memwb_reg.instr = exmem_reg.instr;
  memwb_reg.instr_addr = exmem_reg.instr_addr;

  // Carry over write_reg (On pipeline diagram, the bottom most data path, Instruction [11-7])
  memwb_reg.rd = exmem_reg.rd;

  // Carry over alu result that bypasses the memory stage to the next pipeline register
  memwb_reg.alu_result = exmem_reg.alu_result;

  // Set pc_src1 to branch address (This will be used in fetch stage when checking for branch condition)
  pwires_p->pc_src1 = exmem_reg.branch_addr;

  // Determine if branch should be taken based off current instr
  pwires_p->pcsrc = gen_branch(exmem_reg);

  // Data memory access
  
  // loads data from memory using the address in exmem_reg.alu_result
  // essentially the top signal of the RHS of the data memory block (in the image)
  if (exmem_reg.mem_read) { // load
    switch (exmem_reg.instr.itype.funct3) {
      case 0x0: // lb
        //memwb_reg.read_data = (int8_t)memory_p[exmem_reg.alu_result];
        memwb_reg.read_data = sign_extend_number(load(memory_p, exmem_reg.alu_result, LENGTH_BYTE), 8);
        break;
      case 0x1: // lh
        //memwb_reg.read_data = (int16_t)*(int16_t*)&memory_p[exmem_reg.alu_result];
        memwb_reg.read_data = sign_extend_number(load(memory_p, exmem_reg.alu_result, LENGTH_HALF_WORD), 16);
        break;
      case 0x2: // lw
        //memwb_reg.read_data = *(int32_t*)&memory_p[exmem_reg.alu_result];
        memwb_reg.read_data = sign_extend_number(load(memory_p, exmem_reg.alu_result, LENGTH_WORD), 32);
        break;
      default:
        break;
    }
  }
  
  // stores data to memory using the address in exmem_reg.alu_result
  // (top signal of LHS of memory block in image)
  if (exmem_reg.mem_write) { // store
    switch (exmem_reg.instr.stype.funct3) {
      case 0x0: // sb
        //memory_p[exmem_reg.alu_result] = (uint8_t)exmem_reg.write_data;
        store(memory_p, exmem_reg.alu_result, LENGTH_BYTE, exmem_reg.write_data);
        break;
      case 0x1: // sh
        //*(uint16_t*)&memory_p[exmem_reg.alu_result] = (uint16_t)exmem_reg.write_data;
        store(memory_p, exmem_reg.alu_result, LENGTH_HALF_WORD, exmem_reg.write_data);
        break;
      case 0x2: // sw
        //*(uint32_t*)&memory_p[exmem_reg.alu_result] = exmem_reg.write_data;
        store(memory_p, exmem_reg.alu_result, LENGTH_WORD, exmem_reg.write_data);
        break;
      default:
        break;
    }
  } 

  // For load-store data hazard, carry over WB stage MUX output
  if (exmem_reg.mem_to_reg) { // load instruction
    // data read from memory (memwb_reg.read_data) is forwarded
    pwires_p->write_data = memwb_reg.read_data; 
  } else {  // ALU result (exmem_reg.alu_result) is forwarded
    // bottom signal on RHS of data mem block (image)
    pwires_p->write_data = exmem_reg.alu_result;
  }

  #ifdef DEBUG_CYCLE
  printf("[MEM]: Instruction [%08x]@[%08x]: ", exmem_reg.instr.bits, exmem_reg.instr_addr);
  decode_instruction(exmem_reg.instr.bits);
  #endif

  // Milestone 3 Cache access

  long int latency = 0; // latency in cycles

  if (sim_config.cache_en) {  // Check if cache is enabled in the simulation configuration
    if(exmem_reg.mem_read == 1 || exmem_reg.mem_write == 1) {  // Check if there is a memory write or read operation
      
      // Process the cache operation and get the latency
      // simulates a cache access and returns the latency incurred
      latency = processCacheOperation(exmem_reg.alu_result, cache_p); 

      // Check if the latency indicates a cache miss
      if (latency == CACHE_MISS_LATENCY) {
        miss_count++;
      } 

      // Check if the latency indicates a cache hit
      if (latency == CACHE_HIT_LATENCY) {
        hit_count++;
      }
     
      // Add the cache latency to the total cycle counter
      total_cycle_counter += latency;

      // Adjust the total cycle counter to account for the current operation
      total_cycle_counter -= 1;

      #ifdef PRINT_CACHE_TRACES 
      printf("[MEM]: Cache latency at addr: 0x%.8x: %ld cycles\n", exmem_reg.alu_result, latency);
      #endif
    }
    
  } else {  // If cache not enabled, use the default memory latency

    if(exmem_reg.mem_read == 1 || exmem_reg.mem_write == 1) { // Check if there is a memory write or read operation

      // Add the default memory latency to the total cycle counter
      total_cycle_counter += MEM_LATENCY;

      // Adjust the total cycle counter to account for the current operation
      // Enclosed in a macro because it messes with milestone 1 if it isnt
      #ifdef PRINT_CACHE_STATS
      total_cycle_counter = total_cycle_counter - 1;
      #endif

      // Incrememnt the memory access counter
      // tracks the number of memory accesses performed
      mem_access_counter++;
    }

  }

  return memwb_reg;
}

/**
 * STAGE  : stage_writeback
 * output : nothing - The state of the register file may be changed
 **/ 
void stage_writeback(memwb_reg_t memwb_reg, pipeline_wires_t* pwires_p, regfile_t* regfile_p)
{
  /**
   * YOUR CODE HERE
   */
  uint32_t write_data;

  // MUX in write back stage that determines if alu_result or read_data is used
  if (memwb_reg.mem_to_reg) { // load
    write_data = memwb_reg.read_data; // write data to register in id stage from memory
  } else {  // ALU result
    write_data = memwb_reg.alu_result;  // write data to register in id stage directly from ALU
  }
  
  if (memwb_reg.reg_write) {  // writes back to the register
    if (memwb_reg.rd != 0x0) { // In case of attempt to write to x0
      regfile_p->R[memwb_reg.rd] = write_data;  // write data to register
    }
  }

  #ifdef DEBUG_CYCLE
  printf("[WB ]: Instruction [%08x]@[%08x]: ", memwb_reg.instr.bits, memwb_reg.instr_addr);
  decode_instruction(memwb_reg.instr.bits);
  #endif
}

///////////////////////////////////////////////////////////////////////////////

/** 
 * excite the pipeline with one clock cycle
 **/
void cycle_pipeline(regfile_t* regfile_p, Byte* memory_p, Cache* cache_p, pipeline_regs_t* pregs_p, pipeline_wires_t* pwires_p, bool* ecall_exit)
{
  #ifdef DEBUG_CYCLE
  printf("v==============");
  printf("Cycle Counter = %5ld", total_cycle_counter);
  printf("==============v\n\n");
  #endif

  // process each stage

  /* Output               |    Stage      |       Inputs  */
  pregs_p->ifid_preg.inp  = stage_fetch     (pwires_p, regfile_p, memory_p);
  
  // hazard detection unit
  detect_hazard(pregs_p, pwires_p, regfile_p);

  // check for control signal generated by hazard detection unit that flushes if/id pipeline register
  if (pwires_p->ifid_write == 1) {
    pregs_p->ifid_preg.inp = pregs_p->ifid_preg.out;
    pwires_p->ifid_write = 0;
  }

  pregs_p->idex_preg.inp  = stage_decode    (pregs_p->ifid_preg.out, pwires_p, regfile_p);

  // forwarding unit
  gen_forward(pregs_p, pwires_p);

  pregs_p->exmem_preg.inp = stage_execute   (pregs_p->idex_preg.out, pwires_p);

  pregs_p->memwb_preg.inp = stage_mem       (pregs_p->exmem_preg.out, pwires_p, memory_p, cache_p);

                            stage_writeback (pregs_p->memwb_preg.out, pwires_p, regfile_p);

  // update all the output registers for the next cycle from the input registers in the current cycle
  pregs_p->ifid_preg.out  = pregs_p->ifid_preg.inp;
  pregs_p->idex_preg.out  = pregs_p->idex_preg.inp;
  pregs_p->exmem_preg.out = pregs_p->exmem_preg.inp;
  pregs_p->memwb_preg.out = pregs_p->memwb_preg.inp;

  // Flush registers if branch is taken
  // Enclosed by this macro because milestone1 wont work
  // This is because in milestone 2 everytime a branch is taken it is considered a hazard
  #ifdef PRINT_STATS
  if(pwires_p->pcsrc == 1) {
    flush_pipeline(pregs_p);
  }
  #endif

  /////////////////// NO CHANGES BELOW THIS ARE REQUIRED //////////////////////

  // increment the cycle
  total_cycle_counter++;

  #ifdef DEBUG_REG_TRACE
  print_register_trace(regfile_p);
  #endif

  /**
   * check ecall condition
   * To do this, the value stored in R[10] (a0 or x10) should be 10.
   * Hence, the ecall condition is checked by the existence of following
   * two instructions in sequence:
   * 1. <instr>  x10, <val1>, <val2> 
   * 2. ecall
   * 
   * The first instruction must write the value 10 to x10.
   * The second instruction is the ecall (opcode: 0x73)
   * 
   * The condition checks whether the R[10] value is 10 when the
   * `memwb_reg.instr.opcode` == 0x73 (to propagate the ecall)
   * 
   * If more functionality on ecall needs to be added, it can be done
   * by adding more conditions on the value of R[10]
   */
  if( (pregs_p->memwb_preg.out.instr.bits == 0x00000073) &&
      (regfile_p->R[10] == 10) )
  {
    *(ecall_exit) = true;
  }
}

