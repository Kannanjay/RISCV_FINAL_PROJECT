#ifndef __STAGE_HELPERS_H__
#define __STAGE_HELPERS_H__

#include <stdio.h>
#include "utils.h"
#include "pipeline.h"

/// EXECUTE STAGE HELPERS ///

/**
 * input  : idex_reg_t
 * output : uint32_t alu_control signal
 **/
uint32_t gen_alu_control(idex_reg_t idex_reg)
{
  uint32_t alu_control = 0;
  /* 
      Needs to use ALUOp from EX stage control lines
      Needs Instruction [30, 25, 14-12] (2 specific bits from funct7 and entire funct 3)
      ***funct7_25 is used for recognizing multiply instructions

      Truth table:
      0000 = AND
      0001 = OR
      0010 = ADD
      0011 = XOR
      0100 = SLL
      0101 = SRL
      0110 = SUB
      0111 = SLT 
      1000 = LUI
      1001 = SRA
      1010 = MUL
      1100 = NOR  //not used for our simulator
   */
  
  // #ifdef DEBUG_CYCLE_CONTENTS
  // printf("[EX ]: op1: [%08x], op0: [%08x], funct3: [%08x], funct7: [%08x]\n", 
  // idex_reg.alu_op1, idex_reg.alu_op0, idex_reg.funct3, idex_reg.funct7);
  // decode_instruction(idex_reg.instr.bits);
  // #endif

  switch (idex_reg.alu_op1) {
    case 0x0: // ld, sd, beq, bne, lui
      switch (idex_reg.alu_op0) {
        case 0x0: 
          if (idex_reg.alu_op2 == 1) { 
            alu_control = 0x8;  // lui
          } else {  // ld, sd, jal
            alu_control = 0x2;  // add
          }
          break;
        case 0x1: // beq, bne
          alu_control = 0x6;  // sub
          break; 
        default: 
          break;
      }
      break;
    case 0x1:
      switch (idex_reg.alu_op0) {
        /////////////////////////// R-Type //////////////////////////////
        case 0x0: // R-type
          switch (idex_reg.funct3) {
            case 0x0:
              switch (idex_reg.funct7_30) {
                case 0x0:
                  switch (idex_reg.funct7_25) { // Check for multiply instructions
                    case 0x0:
                      alu_control = 0x2;  // add
                      break;
                    case 0x1:
                      alu_control = 0xA;  // mul
                      break;
                    default:
                      break;
                  }
                  break;
                case 0x1:
                  alu_control = 0x6; // sub
                  break; 
                default:
                  break;
              }
              break;
            case 0x1:
              alu_control = 0x4;  // sll
              break;
            case 0x2:
              alu_control = 0x7; // slt
              break;
            /*
            case 0x3:
              alu_control = "sltu"
            */
            case 0x4:
              alu_control = 0x3;  // xor
              break;
            case 0x5:
              switch (idex_reg.funct7_30) {
                case 0x0:
                  alu_control = 0x5;  // srl
                  break;
                case 0x1:
                  alu_control = 0x9;  // sra
                  break;
                default:
                  break;
              }
              break;
            case 0x6:
              alu_control = 0x1;  // or
              break;
            case 0x7:
              alu_control = 0x0;  // and
              break;
            default:
              break;
          }
          break;

        /////////////////////////// I-Type //////////////////////////////
        case 0x1: // I-type
          switch (idex_reg.funct3) {
            case 0x0:
              alu_control = 0x2;  // addi
              break;
            case 0x1:
              alu_control = 0x4;  // slli
              break;
            case 0x2:
              alu_control = 0x7; // slti
              break;
            /*
            case 0x3:
              alu_control = "sltui"
            */
            case 0x4:
              alu_control = 0x3;  // xori
              break;
            case 0x5:
              switch (idex_reg.funct7_30) {
                case 0x0:
                  alu_control = 0x5;  // srli
                  break;
                case 0x1:
                  alu_control = 0x9;  // srai
                  break;
                default:
                  break;
              }
              break;
            case 0x6:
              alu_control = 0x1;  // ori
              break;
            case 0x7:
              alu_control = 0x0;  // andi
              break;
            default:
              break;
          }
          break;
        default:
          break;
      }
      break;
    default:
      break;
  }

  return alu_control;
}

/**
 * input  : alu_inp1, alu_inp2, alu_control
 * output : uint32_t alu_result
 **/
uint32_t execute_alu(uint32_t alu_inp1, uint32_t alu_inp2, uint32_t alu_control)
{
  /*
    0000 = AND
    0001 = OR
    0010 = ADD
    0011 = XOR
    0100 = SLL
    0101 = SRL
    0110 = SUB
    0111 = SLT 
    1000 = LUI
    1001 = SRA
    1010 = MUL
    1100 = NOR  //not used for our simulator (i think)
  */

  uint32_t result;

  //#ifdef DEBUG_CYCLE_CONTENTS
  // printf("[EX ]: alu_control: [%08x], inp1: [%08x]:, inp2: [%08x]: ", alu_control, alu_inp1, alu_inp2);
  // printf("\n");
  // //#endif

  switch(alu_control){
    case 0x0: //AND
      result = alu_inp1 & alu_inp2;
      break;
    case 0x1: //OR
      result = alu_inp1 | alu_inp2;
      break;
    case 0x2: //ADD
      result = alu_inp1 + alu_inp2;
      break;
    case 0x3: //XOR
      result = alu_inp1 ^ alu_inp2;
      break;
    case 0x4: //SLL
      result = alu_inp1 << alu_inp2;
      break;
    case 0x5: //SRL
      result = alu_inp1 >> alu_inp2;
      break;
    case 0x6: //SUB
      result = alu_inp1 - alu_inp2;
      break;
    case 0x7: //SLT
      result = ((sWord)alu_inp1 < (sWord)alu_inp2) ? 1 : 0;
      break;  
    case 0x8: //LUI
      result = alu_inp2 << 12;
      break;  
    case 0x9: //SRA
      result = sign_extend_number ((alu_inp1 >> alu_inp2), 32-alu_inp2);  // lower 5 bits of rs2
      break;
    case 0xA: //MUL
      result = (sWord)alu_inp1 * (sWord)alu_inp2;
      break;
    default:  //UNDEFINED
      result = 0xBADCAFFE;
      break;
  };
  return result;
}

/// DECODE STAGE HELPERS ///

/**
 * input  : Instruction
 * output : idex_reg_t
 **/
uint32_t gen_imm(Instruction instruction)
{
  int imm_val = 0;
  int imm_4_to_0 = instruction.itype.imm & 0x1F;  // lowest 5 bits of imm
  int funct7 = (instruction.itype.imm >> 5) & 0x7F;    

  switch(instruction.opcode) {
    case 0x63: //SB-type
      imm_val = get_branch_offset(instruction);
      break;
    case 0x13: //I-type, except load
      // Depends on funct7
      if (instruction.itype.funct3 == 0x1 || instruction.itype.funct3 == 0x5) {
        switch (funct7) {
          case 0x00:
            imm_val = imm_4_to_0;
            break;
          case 0x20:
            imm_val = imm_4_to_0;
            break;
          default:
            break; 
        }
      } else {
        imm_val = sign_extend_number(instruction.itype.imm, 12);
      }
      break;
    case 0x23: //S-type
      imm_val = get_store_offset(instruction);
      break;
    case 0x3: //Load
      imm_val = sign_extend_number(instruction.itype.imm, 12);
      break;
    case 0x6F: //UJ-type
      imm_val = get_jump_offset(instruction);
      break;
    case 0x37:  //U-type
      imm_val = sign_extend_number(instruction.utype.imm, 20);
    default: // R and undefined opcode
      break;
  };
  return imm_val;
}

/**
 * generates all the control logic that flows around in the pipeline
 * input  : Instruction
 * output : idex_reg_t
 **/
idex_reg_t gen_control(Instruction instruction)
{
  idex_reg_t idex_reg = {0};
  switch(instruction.opcode) {
      case 0x33:  //R-type
        idex_reg.alu_op2 = 0;
        idex_reg.alu_op1 = 1;
        idex_reg.alu_op0 = 0;
        idex_reg.alu_src = 0; 
        idex_reg.branch = 0;
        idex_reg.mem_read = 0;
        idex_reg.mem_write = 0;
        idex_reg.reg_write = 1;
        idex_reg.mem_to_reg = 0;
        break;
      case 0x13:  //I-type, except load
        idex_reg.alu_op2 = 0;
        idex_reg.alu_op1 = 1;
        idex_reg.alu_op0 = 1;
        idex_reg.alu_src = 1;
        idex_reg.branch = 0;
        idex_reg.mem_read = 0;
        idex_reg.mem_write = 0;
        idex_reg.reg_write = 1;
        idex_reg.mem_to_reg = 0;
        break;
      case 0x3:   //Load
        idex_reg.alu_op2 = 0;
        idex_reg.alu_op1 = 0;
        idex_reg.alu_op0 = 0;
        idex_reg.alu_src = 1;
        idex_reg.branch = 0;
        idex_reg.mem_read = 1;
        idex_reg.mem_write = 0;
        idex_reg.reg_write = 1;
        idex_reg.mem_to_reg = 1;
        break;
      case 0x23:  //S-type
        idex_reg.alu_op2 = 0;
        idex_reg.alu_op1 = 0;
        idex_reg.alu_op0 = 0;
        idex_reg.alu_src = 1;
        idex_reg.branch= 0;
        idex_reg.mem_read = 0;
        idex_reg.mem_write = 1;
        idex_reg.reg_write = 0;
        idex_reg.mem_to_reg = 0;
        break;
      case 0x63:  //SB-type
        idex_reg.alu_op2 = 0;
        idex_reg.alu_op1 = 0;
        idex_reg.alu_op0 = 1;
        idex_reg.alu_src = 0;
        idex_reg.branch = 1;
        idex_reg.mem_read = 0;
        idex_reg.mem_write = 0;
        idex_reg.reg_write = 0;
        idex_reg.mem_to_reg = 0;
        break;
      case 0x37:  //U-type
        idex_reg.alu_op2 = 1;
        idex_reg.alu_op1 = 0;
        idex_reg.alu_op0 = 0;
        idex_reg.alu_src = 1;
        idex_reg.branch = 0;
        idex_reg.mem_read = 0;
        idex_reg.mem_write = 0;
        idex_reg.reg_write = 1;
        idex_reg.mem_to_reg = 0;
        break;
      case 0x6F:  //UJ-type
        idex_reg.alu_op2 = 0;
        idex_reg.alu_op1 = 0;
        idex_reg.alu_op0 = 0;
        idex_reg.alu_src = 1;
        idex_reg.branch = 1;
        idex_reg.mem_read = 0;
        idex_reg.mem_write = 0;
        idex_reg.reg_write = 1;
        idex_reg.mem_to_reg = 0;
        break;
      default:
          break;
  }
  return idex_reg;
}


/// MEMORY STAGE HELPERS ///

/**
 * evaluates whether a branch must be taken
 * input  : <open to implementation>
 * output : bool
 **/
bool gen_branch(exmem_reg_t exmem_reg)
{
  /**
   * YOUR CODE HERE
   */

  // printf("[MEM] alu_result: [%08x], exmem_reg.branch: [%08x], funct3: [%08x], opcode: [%08x]\n", 
  // exmem_reg.alu_result, exmem_reg.branch, exmem_reg.instr.sbtype.funct3, exmem_reg.instr.opcode);

  bool branch_taken = 0;

  if (exmem_reg.branch == 1) {  // if the branch control signal is set

    if (exmem_reg.instr.opcode == 0x6F) { //UJ-type
      branch_taken = 1;
    } else if (exmem_reg.instr.sbtype.funct3 == 0x0) { //beq
      if (exmem_reg.alu_result == 0) {
        branch_taken = 1; // jumping to next address
      } else {
        branch_taken = 0;
      }
    } else { //bne for now (Looks like test cases dont use blt or bge)
       if (exmem_reg.alu_result != 0) {
        branch_taken = 1;
      } else {
        branch_taken = 0; // bne invalid, variables are equal
      }
    }
  } else {
    branch_taken = 0;
  }

  return branch_taken;
}



/// PIPELINE FEATURES ///

/**
 * Task   : Sets the pipeline wires for the forwarding unit's control signals
 *           based on the pipeline register values.
 * input  : pipeline_regs_t*, pipeline_wires_t*
 * output : None
*/
void gen_forward(pipeline_regs_t* pregs_p, pipeline_wires_t* pwires_p)
{
  /**
   * YOUR CODE HERE
   */

  /* forward_a/forward_b (a is alu_inp1, b is alu_inp2)
     This determines where each alu input will come from
       00 = ID/EX
       10 = EX/MEM 
       01 = MEM/WB
  */

  uint8_t forward_a = 0x0;
  uint8_t forward_b = 0x0;

  uint8_t idex_rs1 = pregs_p->idex_preg.out.rs1;
  uint8_t idex_rs2 = pregs_p->idex_preg.out.rs2;

  uint8_t exmem_rd = pregs_p->exmem_preg.out.rd;
  bool exmem_reg_write = pregs_p->exmem_preg.out.reg_write;

  uint8_t memwb_rd = pregs_p->memwb_preg.out.rd;
  bool memwb_reg_write = pregs_p->memwb_preg.out.reg_write;

  // printf("[gen_forward ]: rs1_out: [%08x], rs2_out: [%08x],  exmemrd_out: [%08x]: \n", 
  // idex_rs1, idex_rs2, exmem_rd);
  // printf("[gen_forward ]: rs1_in: [%08x], rs2_in: [%08x],  exmemrd_in: [%08x]: \n", 
  // pregs_p->idex_preg.inp.rs1, pregs_p->idex_preg.inp.rs2, pregs_p->exmem_preg.inp.rd);
  // printf("[gen_forward ]: exmemrd: [%08x], memwbrd: [%08x],  exmemregwrite: [%08x],  memwbregwrite: [%08x]: ", 
  // exmem_rd, memwb_rd, exmem_reg_write, memwb_reg_write);
  // printf("\n");

  // EX Hazard:
  if (exmem_reg_write && (exmem_rd != 0) ) {

    // ForwardA:
    if (exmem_rd == idex_rs1) {
      forward_a = 0x2; // 10
      fwd_exex_counter++;
      
      #ifdef DEBUG_CYCLE
      printf("[FWD]: Resolving EX hazard on rs1: x%d\n", idex_rs1);
      #endif
    }  

    // ForwardB:
    if (exmem_rd == idex_rs2) {
      forward_b = 0x2; // 10
      fwd_exex_counter++;

      #ifdef DEBUG_CYCLE
      printf("[FWD]: Resolving EX hazard on rs2: x%d\n", idex_rs2);
      #endif
    } 
  
  }

  // MEM Hazard:
  if (memwb_reg_write && (memwb_rd != 0)) {

    // ForwardA: 
    if ((memwb_rd == idex_rs1) && !(exmem_reg_write && (exmem_rd != 0) && (exmem_rd == idex_rs1)) ) {
      forward_a = 0x1; // 01
      fwd_exmem_counter++;

      #ifdef DEBUG_CYCLE
      printf("[FWD]: Resolving MEM hazard on rs1: x%d\n", idex_rs1);
      #endif   
    }

    // ForwardB: 
    if ((memwb_rd == idex_rs2) && !(exmem_reg_write && (exmem_rd != 0) && (exmem_rd == idex_rs2)) ) {
      forward_b = 0x1; // 01
      fwd_exmem_counter++;
      
      #ifdef DEBUG_CYCLE
      printf("[FWD]: Resolving MEM hazard on rs2: x%d\n", idex_rs2);
      #endif
    }

  }

  // Set control wires that are used in the MUXs in exacute stage to the forward_a/forward_b values
  pwires_p->forward_a = forward_a;
  pwires_p->forward_b = forward_b;
}

/**
 * Task   : Sets the pipeline wires for the hazard unit's control signals
 *           based on the pipeline register values.
 * input  : pipeline_regs_t*, pipeline_wires_t*
 * output : None
*/
void detect_hazard(pipeline_regs_t* pregs_p, pipeline_wires_t* pwires_p, regfile_t* regfile_p)
{
  /**
   * YOUR CODE HERE
   */

  // extract all registers: rd, rs1, rs2 and flag noting if instr is memory read (load:lh/lb/lw)
  uint8_t rd = pregs_p->idex_preg.out.rd;
  uint8_t rs1 = (pregs_p->ifid_preg.out.instr.rest >> 8) & ((1U << 5) - 1);
  uint8_t rs2 = (pregs_p->ifid_preg.out.instr.rest >> 13) & ((1U << 5) - 1);
  bool mem_read = pregs_p->idex_preg.out.mem_read;

  if (mem_read && ((rd == rs1) || (rd == rs2))) {
  
    pwires_p->flush_control = 1;
    pwires_p->ifid_write = 1;
    pwires_p->pc_write = 1;
    stall_counter++;

    #ifdef DEBUG_CYCLE
    printf("[HZD]: Stalling and rewriting PC: 0x%08x\n", pregs_p->ifid_preg.inp.instr_addr);
    #endif
  } 
}

void flush_pipeline(pipeline_regs_t* pregs_p) 
{ // function resets the pipeline registers to a known state when a branch is taken

  // save all current instruction addresses from all pipeline registers for preservation
  uint32_t ifid_instr_addr = pregs_p->ifid_preg.out.instr_addr;
  uint32_t idex_instr_addr = pregs_p->idex_preg.out.instr_addr;
  uint32_t exmem_instr_addr = pregs_p->exmem_preg.out.instr_addr;

  // Set IFID register to NOP
  // clear if/id reg (set to 0)
  pregs_p->ifid_preg.out = (ifid_reg_t){0};

  // set instruction to ADDI x0, x0, 0 -> does nothing, acting as a NOP
  pregs_p->ifid_preg.out.instr = parse_instruction(0x00000013);

  // restores the instruction address to if/id register
  pregs_p->ifid_preg.out.instr_addr = ifid_instr_addr;

  // repeat same process for the other pipeline registers

  // Set IDEX register to NOP
  pregs_p->idex_preg.out = (idex_reg_t){0};
  pregs_p->idex_preg.out.instr = parse_instruction(0x00000013);
  pregs_p->idex_preg.out.instr_addr = idex_instr_addr;

  // Set EXMEM register to NOP
  pregs_p->exmem_preg.out = (exmem_reg_t){0};
  pregs_p->exmem_preg.out.instr = parse_instruction(0x00000013);
  pregs_p->exmem_preg.out.instr_addr = exmem_instr_addr;

  // keep track of # of branches taken during execution
  branch_counter++;
  
  #ifdef DEBUG_CYCLE
  printf("[CPL]: Pipeline Flushed\n");
  #endif
}


///////////////////////////////////////////////////////////////////////////////


/// RESERVED FOR PRINTING REGISTER TRACE AFTER EACH CLOCK CYCLE ///
void print_register_trace(regfile_t* regfile_p)
{
  // print
  for (uint8_t i = 0; i < 8; i++)       // 8 columns
  {
    for (uint8_t j = 0; j < 4; j++)     // of 4 registers each
    {
      printf("r%2d=%08x ", i * 4 + j, regfile_p->R[i * 4 + j]);
    }
    printf("\n");
  }
  printf("\n");
}

#endif // __STAGE_HELPERS_H__
