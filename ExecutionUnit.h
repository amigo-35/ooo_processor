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
         // clear previous cycle's result
         has_result    = false;
         has_exception = false;
         result_tag    = -1;
         result_val    = 0;
         result_pc     = -1;
         // advance pipeline , check for completion
         for(auto& slot : pipeline){
             if(!slot.active) continue;
             slot.cycles_left--;
             if(slot.cycles_left==0){
                 // Setting for broadcasting
                 has_result    = true;
                 has_exception = slot.exception;
                 result_tag    = slot.rob_tag;
                 result_val    = slot.result;
                 result_pc     = slot.pc;
                 slot.active = false;
             }
         }
         // now pick oldest ready RS instruction and start executing but first find free slot
         PipelineSlot* free_slot = nullptr;
         for(auto& slot: pipeline){
             if(!slot.active){
                 free_slot = &slot;
                 break;
             }
         } 
         if(!free_slot) return; // pipeline is full, can't accept
 
         RSEntry* oldest = nullptr;
         for(auto& entry: rs){
             if(!entry.busy) continue;
             if(entry.qj !=-1 || entry.qk !=-1) continue;
             if (!oldest || entry.age < oldest->age) { // lower age means older , we define in this way 
                 oldest = &entry;
             }
         } 
         if(!oldest) return; // no instruction is ready
         // compute result and load into pipeline
         bool exc = false;
         int res = compute(oldest->op, oldest->vj, oldest->vk, oldest->imm, exc);
 
         free_slot->active     = true;
         free_slot->rob_tag    = oldest->dest_rob;
         free_slot->result     = res;
         free_slot->exception  = exc;
         free_slot->pc         = oldest->pc;
         free_slot->cycles_left = latency;
 
         oldest->busy = false;
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
    int compute(OpCode op, int vj, int vk, int imm, bool& exc){
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
            case OpCode::BEQ:
                return (vj == vk) ? (imm) : 0; // imm holds offset, 0 = not taken signal
            case OpCode::BNE:
                return (vj != vk) ? (imm) : 0;
            case OpCode::BLT:
                return (vj <  vk) ? (imm) : 0;
            case OpCode::BLE:
                return (vj <= vk) ? (imm) : 0;
            case OpCode::J:
                return imm; // always taken
 
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