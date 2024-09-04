#include "utils.h"
#include <stdio.h>
#include <stdlib.h>

/* Unpacks the 32-bit machine code instruction given into the correct
 * type within the instruction struct */
Instruction parse_instruction(uint32_t instruction_bits) {
  /* YOUR CODE HERE */
  Instruction instruction;
  // add x9, x20, x21   hex: 01 5A 04 B3, binary = 0000 0001 0101 1010 0000 0100 1011 0011
  // Opcode: 0110011 (0x33) Get the Opcode by &ing 0x1111111, bottom 7 bits
  instruction.opcode = instruction_bits & ((1U << 7) - 1);

  // Shift right to move to pointer to interpret next fields in instruction.
  instruction_bits >>= 7;

  switch (instruction.opcode) {
  // R-Type
  case 0x33:
    // instruction: 0000 0001 0101 1010 0000 0100 1, destination : 01001
    instruction.rtype.rd = instruction_bits & ((1U << 5) - 1);
    instruction_bits >>= 5;

    // instruction: 0000 0001 0101 1010 0000, func3 : 000
    instruction.rtype.funct3 = instruction_bits & ((1U << 3) - 1);
    instruction_bits >>= 3;

    // instruction: 0000 0001 0101 1010 0, src1: 10100
    instruction.rtype.rs1 = instruction_bits & ((1U << 5) - 1);
    instruction_bits >>= 5;

    // instruction: 0000 0001 0101, src2: 10101
    instruction.rtype.rs2 = instruction_bits & ((1U << 5) - 1);
    instruction_bits >>= 5;

    // funct7: 0000 000
    instruction.rtype.funct7 = instruction_bits & ((1U << 7) - 1);
    break;
  // cases for other types of instructions
  /* YOUR CODE HERE */

  // Load
  case 0x03:

    instruction.itype.rd = instruction_bits & ((1U << 5) - 1);
    instruction_bits >>= 5;

    instruction.itype.funct3 = instruction_bits & ((1U << 3) - 1);
    instruction_bits >>= 3;

    instruction.itype.rs1 = instruction_bits & ((1U << 5) - 1);
    instruction_bits >>= 5;

    instruction.itype.imm = instruction_bits & ((1U << 12) - 1);
    break;

  // I-Type, except load
  case 0x13:

    instruction.itype.rd = instruction_bits & ((1U << 5) - 1);
    instruction_bits >>= 5;

    instruction.itype.funct3 = instruction_bits & ((1U << 3) - 1);
    instruction_bits >>= 3;

    instruction.itype.rs1 = instruction_bits & ((1U << 5) - 1);
    instruction_bits >>= 5;

    instruction.itype.imm = instruction_bits & ((1U << 12) - 1);
    break;

  // Ecall
  case 0x73:
    //Ecall only has an opcode and the rest of the instruction is 0's
    instruction.rest = 0;
    break;

  // S-Type
  case 0x23:

    instruction.stype.imm5 = instruction_bits & ((1U << 5) - 1);
    instruction_bits >>= 5;

    instruction.stype.funct3 = instruction_bits & ((1U << 3) - 1);
    instruction_bits >>= 3;

    instruction.stype.rs1 = instruction_bits & ((1U << 5) - 1);
    instruction_bits >>= 5;

    instruction.stype.rs2 = instruction_bits & ((1U << 5) - 1);
    instruction_bits >>= 5;

    instruction.stype.imm7 = instruction_bits & ((1U << 7) - 1);
    break;

  // SB-Type
  case 0x63:

    instruction.sbtype.imm5 = instruction_bits & ((1U << 5) - 1);
    instruction_bits >>= 5;

    instruction.sbtype.funct3 = instruction_bits & ((1U << 3) - 1);
    instruction_bits >>= 3;

    instruction.sbtype.rs1 = instruction_bits & ((1U << 5) - 1);
    instruction_bits >>= 5;

    instruction.sbtype.rs2 = instruction_bits & ((1U << 5) - 1);
    instruction_bits >>= 5;

    instruction.sbtype.imm7 = instruction_bits & ((1U << 7) - 1);
    break;

  //U-Type
  case 0x37:

    instruction.utype.rd = instruction_bits & ((1U << 5) - 1);
    instruction_bits >>= 5;

    instruction.utype.imm = instruction_bits & ((1U << 20) - 1);
    break;

  //UJ-Type
  case 0x6f:

    instruction.ujtype.rd = instruction_bits & ((1U << 5) - 1);
    instruction_bits >>= 5;

    instruction.ujtype.imm = instruction_bits & ((1U << 20) - 1);
    break;

  #ifndef TESTING
  default:
    exit(EXIT_FAILURE);
  #endif
  }
  return instruction;
}

/************************Helper functions************************/
/* Here, you will need to implement a few common helper functions, 
 * which you will call in other functions when parsing, printing, 
 * or executing the instructions. */

/* Sign extends the given field to a 32-bit integer where field is
 * interpreted an n-bit integer. */
int sign_extend_number(unsigned int field, unsigned int n) {
  /* YOUR CODE HERE */

  // TA provided solution to function:
  int signextended = field << (32-n);
  signextended >>= (32-n);
  return signextended;

}

/* Return the number of bytes (from the current PC) to the branch label using
 * the given branch instruction */
int get_branch_offset(Instruction instruction) {
  /* YOUR CODE HERE */

  /* Explanation: 
   *
   * For sbtype, funct7 is split into imm[12|10:5] and funct5 into imm[4:1|11].
   * We need to rearrange these and combine them into one imm value.
   *
   * For each intermediate imm value, right shift to get rid of unwanted bits, 
   * then mask the bits you want, then left shift the bits into the proper position. */

  unsigned int imm12 = (((instruction.sbtype.imm7 >> 6) & 0x01) << 11);
  unsigned int imm11 = ((instruction.sbtype.imm5 & 0x01) << 10);
  unsigned int imm10_5 = ((instruction.sbtype.imm7 & 0x3F) << 4);
  unsigned int imm4_1 = ((instruction.sbtype.imm5 >> 1) & 0xF);
  unsigned int offset = imm12 | imm11 | imm10_5 | imm4_1;

  // In the lecture notes the branch offset is multiplied by 2
  // Left shifting by 1 is equivalent to multiplying by 2
  return sign_extend_number(offset << 1, 13);
}

/* Returns the number of bytes (from the current PC) to the jump label using the
 * given jump instruction */
int get_jump_offset(Instruction instruction) {
  /* YOUR CODE HERE */

  /* Explanation: 
   *
   * For ujtype, imm is split into imm[20|10:1|11|19:12].
   * We need to rearrange these and combine them into a new imm value.
   *
   * For each intermediate imm value, right shift to get rid of unwanted bits, 
   * then mask the bits you want, then left shift the bits into the proper position. */

  unsigned int imm20 = ((instruction.ujtype.imm >> 19) << 19);
  unsigned int imm19_12 = ((instruction.ujtype.imm & 0xFF) << 11);
  unsigned int imm11 = (((instruction.ujtype.imm >> 8) & 0x01) << 10);
  unsigned int imm10_1 = ((instruction.ujtype.imm >> 9) & 0x3FF);
  unsigned int offset = imm20 | imm19_12 | imm11 | imm10_1;

  // In the lecture notes the jump offset is multiplied by 2
  // Left shifting by 1 is equivalent to multiplying by 2
  return sign_extend_number(offset << 1, 21);
}

/* Returns the number of bytes (from the current PC) to the base address using the
 * given store instruction */
int get_store_offset(Instruction instruction) {
  /* YOUR CODE HERE */

  /* Explanation: 
   *
   * For stype, imm7 is split into imm[11:5] and imm5 into imm[4:0].
   * We need to combine them into a new imm value.
   *
   * Just left shift imm7 and combine it with imm5 */

  int imm = (instruction.stype.imm7 << 5) | instruction.stype.imm5;
  return sign_extend_number(imm, 12);
}
/************************Helper functions************************/

void handle_invalid_instruction(Instruction instruction) {
  printf("Invalid Instruction: 0x%08x\n", instruction.bits);
}

void handle_invalid_read(Address address) {
  printf("Bad Read. Address: 0x%08x\n", address);
  exit(-1);
}

void handle_invalid_write(Address address) {
  printf("Bad Write. Address: 0x%08x\n", address);
  exit(-1);
}
