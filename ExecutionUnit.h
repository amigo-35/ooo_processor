#pragma once
#include <iostream>
#include <vector>
#include <string>
#include "Basics.h"
#include <climits>

class ExecutionUnit {
public:
    // per-unit reservation station
    UnitType name;
    int latency;
    
    // each unit has its own RS
    std::vector<RSEntry> rs;
    int rs_size;

    struct PipelineSlot{
        bool active = false;
        int rob_tag = -1;
        int result = 0;
        bool exception = false ;
        int pc = -1;
        int cycles_left = 0; // if count =0 then we will broadcast the result
    };
    std::vector<PipelineSlot> pipeline;

    bool has_result = false; // result flag
    bool has_exception = false; // exception flag
    int result_tag = -1;
    int result_val = 0;
    int result_pc = -1;
    
    ExecutionUnit(UnitType type, int lat, int rs_sz)
        : name(type), latency(lat), rs_size(rs_sz) {
        rs.resize(rs_size);
        pipeline.resize(latency);
    }
    void capture(int tag, int val) {
        for(auto& entry: rs){
            if(!entry.busy) continue;
            if(entry.qj==tag){
                entry.vj = val;
                entry.qj = -1; // -1 bcz now its ready
            }
            if(entry.qk==tag){
                entry.vk = val;
                entry.qk = -1; // -1 bcz now its ready
            }
        }
    };
    void executeCycle() {
        // 1. Clear previous cycle's broadcast state
        has_result    = false;
        has_exception = false;
        result_tag    = -1;
        result_val    = 0;
        result_pc     = -1;

        // Helper: Check if an instruction is already inside the pipeline
        auto isExecuting = [&](int rob_tag) {
            for(auto& slot : pipeline) {
                if(slot.active && slot.rob_tag == rob_tag) return true;
            }
            return false;
        };

        // 2. DISPATCH: Find a free slot and pull from RS
        PipelineSlot* free_slot = nullptr;
        for(auto& slot : pipeline) {
            if(!slot.active) {
                free_slot = &slot;
                break;
            }
        } 

        if(free_slot) {
            RSEntry* oldest = nullptr;
            for(auto& entry : rs) {
                if(!entry.busy) continue;
                
                // Must be ready AND not already executing in the pipeline
                if(entry.qj == -1 && entry.qk == -1 && !isExecuting(entry.dest_rob)) { 
                    if (!oldest || entry.age < oldest->age) {
                        oldest = &entry;
                    }
                }
            } 
            
            if(oldest) {
                bool exc = false;
                int res = compute(oldest->op, oldest->vj, oldest->vk, oldest->imm, oldest->pc, exc);

                free_slot->active      = true;
                free_slot->rob_tag     = oldest->dest_rob;
                free_slot->result      = res;
                free_slot->exception   = exc;
                free_slot->pc          = oldest->pc;
                free_slot->cycles_left = latency; 

                // DO NOT DEALLOCATE RS HERE! 
                // It stays busy so Decode correctly stalls if the RS is full.
            }
        }

        // 3. EXECUTE: Advance pipeline and broadcast
        for(auto& slot : pipeline) {
            if(!slot.active) continue;
            
            slot.cycles_left--;
            
            if(slot.cycles_left == 0) {
                has_result    = true;
                has_exception = slot.exception;
                result_tag    = slot.rob_tag;
                result_val    = slot.result;
                result_pc     = slot.pc;
                
                // FREE THE RS ENTRY NOW (At the time of broadcast)
                for(auto& entry : rs) {
                    if(entry.busy && entry.dest_rob == slot.rob_tag) {
                        entry.busy = false;
                        break;
                    }
                }
                
                slot.active = false;
            }
        }
    };
    // called at decode stage to insert a new entry
    bool tryDispatch(const RSEntry& entry) {
        for (auto& slot : rs) {
            if (!slot.busy) {
                slot = entry;
                slot.busy = true;
                return true;
            }
        }
        return false; // RS is full, stall decode
    }
    // RS FULL CHECK
    bool isFull() const {
        for (const auto& slot : rs) {
            if (!slot.busy) return false;
        }
        return true;
    }
private:
int compute(OpCode op, int vj, int vk, int imm, int inst_pc, bool& exc){
        exc = false;
        long long result = 0;

        switch(op){
            case OpCode::ADD:  result = (long long)vj + vk; break;
            case OpCode::SUB:  result = (long long)vj - vk; break;
            case OpCode::ADDI: result = (long long)vj + imm; break;
            case OpCode::SLT:  return (vj < vk) ? 1 : 0;
            case OpCode::SLTI: return (vj < imm) ? 1 : 0;
 
            case OpCode::MUL:  result = (long long)vj * vk; break;
 
            case OpCode::DIV:
                if (vk == 0) { exc = true; return 0; }
                result = (long long)vj / vk;
                break;
            case OpCode::REM:
                if (vk == 0) { exc = true; return 0; }
                result = (long long)vj % vk;
                break;
 
            case OpCode::AND:  return vj & vk;
            case OpCode::OR:   return vj | vk;
            case OpCode::XOR:  return vj ^ vk;
            case OpCode::ANDI: return vj & imm;
            case OpCode::ORI:  return vj | imm;
            case OpCode::XORI: return vj ^ imm;
 
            // For branches: vj = src1, vk = src2, imm = offset
            // result = actual next PC (current_pc + offset if taken, else current_pc + 1)
           case OpCode::BEQ: return (vj == vk) ? (inst_pc + imm) : (inst_pc + 1);
           case OpCode::BNE: return (vj != vk) ? (inst_pc + imm) : (inst_pc + 1);
           case OpCode::BLT: return (vj <  vk) ? (inst_pc + imm) : (inst_pc + 1);
           case OpCode::BLE: return (vj <= vk) ? (inst_pc + imm) : (inst_pc + 1);
           case OpCode::J:   return inst_pc + imm;
            default:
                return 0;
        }
        if (result > INT_MAX || result < INT_MIN) {
            exc = true;
            return 0;
        }
        return (int)result;
    }
};