#pragma once
#include <iostream>
#include <fstream>
#include <map>
#include <sstream>
#include <vector>
#include <iomanip>
#include "Basics.h"
#include "BranchPredictor.h"
#include "ExecutionUnit.h"
#include "LoadStoreQueue.h"

class Processor {
public:
    int pc;
    int clock_cycle;
    bool halted = false;

    // pipeline registers

    std::vector<Instruction> inst_memory;

    // architectural state (do not change)
    std::vector<int> ARF; // regFile
    std::vector<int> Memory; // Memory
    bool exception = false; // exception bit

     struct FetchDecodeReg {
        bool valid       = false;
        Instruction inst;
        int predicted_pc = -1;
    };
    FetchDecodeReg fd_reg;


    // register alias table / reorder buffer

   std::vector<ROBEntry> ROB;
    int rob_head  = 0;  // next entry to commit
    int rob_tail  = 0;  // next free slot to allocate
    int rob_count = 0;  // number of occupied slots
    int rob_size;

    // ── Register Alias Table ─────────────────────────────────────────────────
    // RAT[i] = ROB tag of in-flight instruction writing reg i
    //         or -1 if ARF[i] is the current architectural value
    std::vector<int> RAT;

    // Global age counter — lower = older = higher priority in RS scheduling
    int global_age = 0;

    // ── Execution Units ──────────────────────────────────────────────────────
    // Fixed indices: 0=Adder, 1=Multiplier, 2=Divider, 3=Branch, 4=Logic
    static constexpr int ADDER_IDX  = 0;
    static constexpr int MULT_IDX   = 1;
    static constexpr int DIV_IDX    = 2;
    static constexpr int BRANCH_IDX = 3;
    static constexpr int LOGIC_IDX  = 4;

    std::vector<ExecutionUnit> units;
    LoadStoreQueue* lsq;
    BranchPredictor bp;

    Processor(ProcessorConfig& config) {
        pc = 0;
        clock_cycle = 0;
        rob_size    = config.rob_size;
        ARF.resize(config.num_regs, 0);
        Memory.resize(config.mem_size);
        ROB.resize(config.rob_size);
        RAT.resize(config.num_regs, -1);

        // Instantiate in index order (must match ADDER_IDX etc.)
        units.emplace_back(UnitType::ADDER,      config.add_lat,   config.adder_rs_size);
        units.emplace_back(UnitType::MULTIPLIER, config.mul_lat,   config.mult_rs_size);
        units.emplace_back(UnitType::DIVIDER,    config.div_lat,   config.div_rs_size);
        units.emplace_back(UnitType::BRANCH,     1,                config.br_rs_size);
        units.emplace_back(UnitType::LOGIC,      config.logic_lat, config.logic_rs_size);

        lsq = new LoadStoreQueue(config.mem_lat, config.lsq_rs_size);

        // Instantiate Hardware Units
        // Adder
        // Multiplier
        // Divider
        // Branch Computation
        // Bitwise Logic
        // Load-Store Unit
    }
       ExecutionUnit* getUnitForOp(OpCode op) {
        switch (op) {
            case OpCode::ADD: case OpCode::SUB: case OpCode::ADDI:
            case OpCode::SLT: case OpCode::SLTI:
                return &units[ADDER_IDX];
            case OpCode::MUL:
                return &units[MULT_IDX];
            case OpCode::DIV: case OpCode::REM:
                return &units[DIV_IDX];
            case OpCode::BEQ: case OpCode::BNE:
            case OpCode::BLT: case OpCode::BLE:
                return &units[BRANCH_IDX];
            case OpCode::AND: case OpCode::OR:  case OpCode::XOR:
            case OpCode::ANDI: case OpCode::ORI: case OpCode::XORI:
                return &units[LOGIC_IDX];
            default:  // LW, SW, J
                return nullptr;
        }
    }
     bool robFull() const { return rob_count >= rob_size; }

    // Allocates the next free ROB slot, returns its tag (index)
    int robAllocate() {
        int tag = rob_tail;
        ROB[tag] = ROBEntry();  // reset to defaults
        ROB[tag].valid = true;
        rob_tail = (rob_tail + 1) % rob_size;
        rob_count++;
        return tag;
    }

    // Frees the head of the ROB (call after committing)
    void robFreeHead() {
        ROB[rob_head].valid = false;
        rob_head = (rob_head + 1) % rob_size;
        rob_count--;
    }

    void loadProgram(const std::string& filename) {
        std::ifstream file(filename);
        if (!file.is_open())
            throw std::runtime_error("Cannot open: " + filename);

        static const std::map<std::string, OpCode> opMap = {
            {"add",OpCode::ADD},  {"sub",OpCode::SUB},  {"addi",OpCode::ADDI},
            {"mul",OpCode::MUL},  {"div",OpCode::DIV},  {"rem",OpCode::REM},
            {"lw",OpCode::LW},    {"sw",OpCode::SW},
            {"beq",OpCode::BEQ},  {"bne",OpCode::BNE},
            {"blt",OpCode::BLT},  {"ble",OpCode::BLE},
            {"j",OpCode::J},
            {"slt",OpCode::SLT},  {"slti",OpCode::SLTI},
            {"and",OpCode::AND},  {"or",OpCode::OR},    {"xor",OpCode::XOR},
            {"andi",OpCode::ANDI},{"ori",OpCode::ORI},  {"xori",OpCode::XORI}
        };

        // Parse "xN" → register number N
        auto parseReg = [](const std::string& s) -> int {
            return std::stoi(s[0] == 'x' ? s.substr(1) : s);
        };

        // Parse "offset(xN)" → fills imm_out and reg_out
        auto parseMem = [&](const std::string& s, int& imm_out, int& reg_out) {
            auto p = s.find('(');
            imm_out = std::stoi(s.substr(0, p));
            reg_out = parseReg(s.substr(p + 1, s.size() - p - 2));
        };
         std::string line;
        int current_pc = 0;

        while (std::getline(file, line)) {
            // Trim
            while (!line.empty() && isspace((unsigned char)line.back()))  line.pop_back();
            while (!line.empty() && isspace((unsigned char)line.front())) line.erase(line.begin());
            if (line.empty()) continue;

            
            if (line.size() >= 4 && line.substr(0, 4) == "#MEM") {
                std::istringstream ss(line.substr(5));
                int val, idx = 0;
                while (ss >> val && idx < (int)Memory.size()) Memory[idx++] = val;
                continue;
            }
            if (line[0] == '#') continue;

        
            std::vector<std::string> tok;
            std::string cur;
            for (char c : line) {
                if (c == ' ' || c == '\t' || c == ',') {
                    if (!cur.empty()) { tok.push_back(cur); cur.clear(); }
                } else { cur += c; }
            }
            if (!cur.empty()) tok.push_back(cur);
            if (tok.empty()) continue;
             std::string opStr = tok[0];
            for (char& c : opStr) c = (char)tolower((unsigned char)c);
            if (!opMap.count(opStr)) continue;

            Instruction inst;
            inst.op   = opMap.at(opStr);
            inst.pc   = current_pc;
            inst.dest = inst.src1 = inst.src2 = -1;
            inst.imm  = 0;

            if (opStr=="add"||opStr=="sub"||opStr=="mul"||opStr=="div"||
                opStr=="rem"||opStr=="slt"||opStr=="and"||opStr=="or"||opStr=="xor") {
                inst.dest = parseReg(tok[1]);
                inst.src1 = parseReg(tok[2]);
                inst.src2 = parseReg(tok[3]);
            }
            else if (opStr=="addi"||opStr=="slti"||
                     opStr=="andi"||opStr=="ori"||opStr=="xori") {
                inst.dest = parseReg(tok[1]);
                inst.src1 = parseReg(tok[2]);
                inst.imm  = std::stoi(tok[3]);
            }
            else if (opStr == "lw") {
                inst.dest = parseReg(tok[1]);
                parseMem(tok[2], inst.imm, inst.src1);
            }
            else if (opStr == "sw") {
                inst.src2 = parseReg(tok[1]);      
                parseMem(tok[2], inst.imm, inst.src1);
            }
              else if (opStr=="beq"||opStr=="bne"||opStr=="blt"||opStr=="ble") {
                inst.src1 = parseReg(tok[1]);
                inst.src2 = parseReg(tok[2]);
                inst.imm  = std::stoi(tok[3]);
            }
            else if (opStr == "j") {
                inst.imm = std::stoi(tok[1]);
            }

            inst_memory.push_back(inst);
            current_pc++;
        }
    }
    

    void flush() {
        fd_reg.valid = false;

        for (auto& unit : units) {
            for (auto& e : unit.rs)       e.busy   = false;
            for (auto& s : unit.pipeline) s.active = false;
            unit.has_result = false;
        }

        lsq->queue.clear();
        lsq->has_result = false;

        for (auto& e : ROB) e.valid = false;
        rob_head = rob_tail = rob_count = 0;

        std::fill(RAT.begin(), RAT.end(), -1);
    };

    void broadcastOnCDB() {
        auto broadcast = [&](int tag, int val, bool exc,
                             bool is_store, int store_addr, int store_val_commit) {
            if (tag < 0 || !ROB[tag].valid) return;

       
            ROB[tag].ready     = true;
            ROB[tag].exception = exc;

            if (is_store) {
                ROB[tag].mem_addr  = store_addr;
                ROB[tag].store_val = store_val_commit;
            } else {
                ROB[tag].value = val;
            }

        
            if (!is_store && !exc) {
                for (auto& unit : units) unit.capture(tag, val);
                lsq->capture(tag, val);
            }
        };

        for (auto& unit : units) {
            if (unit.has_result)
                broadcast(unit.result_tag, unit.result_val, unit.has_exception,
                          false, -1, 0);
        }
         if (lsq->has_result)
            broadcast(lsq->result_tag, lsq->result_val, lsq->has_exception,
                      lsq->result_is_store, lsq->result_addr, lsq->store_data);
    };

    void stageFetch() {};

    void stageDecode() {};

    void stageExecuteAndBroadcast() {};

    void stageCommit() {
     // Nothing to commit if ROB is empty
    if (rob_count == 0) return;
 
    ROBEntry& entry = ROB[rob_head];
 
    // Head must be valid and ready (finished executing)
    if (!entry.valid || !entry.ready) return;
    // CASE 1 — EXCEPTION
    if (entry.exception) {
        exception = true;
        pc        = entry.pc;   // PC of faulting instruction
        halted    = true;
        flush();
        return;                
    }
    // CASE 2 — BRANCH
    if (entry.is_branch) {
        // entry.value holds the offset if taken, 0 if not taken
        // (as computed by ExecutionUnit::compute())
        bool taken       = (entry.value != 0);
        int  actual_pc   = taken ? (entry.pc + entry.value) : (entry.pc + 1);
        bool was_correct = (actual_pc == entry.predicted_pc);
 
        // Update 2-bit saturating counter for this branch's PC
        bp.update(entry.pc, actual_pc, taken, was_correct);
 
        if (!was_correct) {
            // Misprediction — flush and redirect
            pc = actual_pc;
            flush();
            return;
        }
        // Correct prediction — fall through to robFreeHead()
    }
    // CASE 3 — STORE (SW)
    else if (entry.is_store) {
        // safe to write here
        Memory[entry.mem_addr] = entry.store_val;
    }
    // CASE 4 — NORMAL INSTRUCTION
    // Write result to architectural register.
    else {
        if (entry.dest_reg > 0) {  // skip x0
            ARF[entry.dest_reg] = entry.value;
        }
    }
    // RAT CLEANUP
    // Only clear RAT[dest_reg] if this ROB entry
    // is still the one RAT points to.
    if (entry.dest_reg > 0 && RAT[entry.dest_reg] == rob_head) {
        RAT[entry.dest_reg] = -1;
    }
    // Free the ROB head slot
    robFreeHead();
    };

    bool step() {
        if (halted) return false;
        clock_cycle++;

    
        stageCommit();
        stageExecuteAndBroadcast();
        stageDecode();
        stageFetch();

        if (exception) return false;

        bool done = (pc >= (int)inst_memory.size())
                 && (!fd_reg.valid)
                 && (rob_count == 0);
        return !done;
    }

    void dumpArchitecturalState() {
        std::cout << "\n=== ARCHITECTURAL STATE (CYCLE " << clock_cycle << ") ===\n";
        for (int i = 0; i < ARF.size(); i++) {
            std::cout << "x" << i << ": " << std::setw(4) << ARF[i] << " | ";
            if ((i+1) % 8 == 0) std::cout << std::endl;
        }
        if (exception) {
            std::cout << "EXCEPTION raised by instruction " << pc + 1 << std::endl;
        }
        std::cout << "Branch Predictor Stats: " << bp.correct_predictions << "/" << bp.total_branches << " correct.\n";
    }
};