#pragma once
#include <string>

enum class OpCode { ADD, SUB, ADDI, MUL, DIV, REM, LW, SW, BEQ, BNE, BLT, BLE, J, SLT, SLTI, AND, OR, XOR, ANDI, ORI, XORI };
enum class UnitType { ADDER, MULTIPLIER, DIVIDER, LOADSTORE, BRANCH, LOGIC };

struct Instruction {
    OpCode op;
    int dest;
    int src1;
    int src2;
    int imm;
    int pc;
};

struct ProcessorConfig {
    int num_regs = 32;
    int rob_size = 64;
    int mem_size = 1024;

    int logic_lat = 1;
    int add_lat = 2;
    int mul_lat = 4;
    int div_lat = 5;
    int mem_lat = 4;

    int logic_rs_size = 4;
    int adder_rs_size = 4;
    int mult_rs_size = 2;
    int div_rs_size = 2;
    int br_rs_size = 2;
    int lsq_rs_size = 32;
};

struct ROBEntry {
    // valid bit, ready bit, architectural register ID
    // other fields as required
    bool valid = false;     // is this slot occupied?
    bool ready = false;     // has the instruction finished executing?
    bool exception = false; // did this instruction cause an exception?

    OpCode op;   // needed at commit to know what action to take
    int pc = -1; // PC of this instruction (for precise exceptions)

    // Register write info
    int dest_reg = -1; // architectural register to write (-1 = no write)
    int value = 0;     // computed result to write into ARF at commit

    // Branch info (only used if op is BEQ/BNE/BLT/BLE/J)
    bool is_branch = false;
    int predicted_pc = -1; // what fetch predicted as next PC
    int actual_pc = -1;    // what the actual next PC should be

    // Store info (only used if op is SW)
    // We can't write to memory until commit (in-order)
    bool is_store = false;
    int mem_addr = -1; // computed memory address for SW
    int store_val = 0; // value to write to memory at commit
    int age = -1;
};

struct RSEntry {
    // value, tag, ready ... for both operands
    // other fields as required
    bool busy = false; // is this slot occupied?

    OpCode op; // what operation to perform

    // Operand 1
    int vj = 0;  // value of operand 1 (valid when qj == -1)
    int qj = -1; // ROB tag for operand 1 (-1 = ready)

    // Operand 2
    int vk = 0;  // value of operand 2 (valid when qk == -1)
    int qk = -1; // ROB tag for operand 2 (-1 = ready)

    int imm = 0;       // immediate value (for ADDI, SLTI, LW, SW etc.)
    int dest_rob = -1; // which ROB entry this instruction writes result to
    int pc = -1;       // PC of this instruction
    int age = -1;      // instruction age (lower = older = higher priority)
                       // used by execution unit to pick oldest ready entry
};