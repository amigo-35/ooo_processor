#pragma once
#include <iostream>
#include <vector>
#include <string>
#include "Basics.h"
#include <deque>


class LoadStoreQueue {
public:
    int latency;
    int max_size;
    bool has_result = false; 
    bool has_exception = false; 
    int store_data = 0;
    int  result_tag    = -1;     
    int  result_val    = 0;      
    int  result_addr   = -1;     
    bool result_is_store = false; 
    
    struct LSQEntry {
        bool is_store = false;
        int base_val = 0;
        int qbase    = -1; 

        int store_val = 0;
        int qstore    = -1;

        int imm      = 0;
        int dest_rob = -1;
        int pc       = -1;
        int age      = -1;

        bool executing    = false;
        int  cycles_left  = 0;
        int  computed_addr = -1; 
    };

    std::deque<LSQEntry> queue;

    LoadStoreQueue(int lat, int sz) : latency(lat), max_size(sz) {}
    bool isFull() const { return (int)queue.size() >= max_size; }
    bool tryDispatch(const LSQEntry& entry) {
        if (isFull()) return false;
        queue.push_back(entry);
        return true;
    }
    void capture(int tag, int val) {
        for (auto& e : queue) {
            if (e.qbase  == tag) { e.base_val  = val; e.qbase  = -1; }
            if (e.qstore == tag) { e.store_val = val; e.qstore = -1; }
        }
    }
    void executeCycle(std::vector<int>& Memory, const std::vector<ROBEntry>& ROB) {  
        has_result      = false;
        has_exception   = false;
        result_tag      = -1;
        result_val      = 0;
        result_addr     = -1;
        result_is_store = false;

        if (queue.empty()) return;

        // 1. DISPATCH (Start execution sequentially, ONE per cycle)
        for (auto& entry : queue) {
            if (!entry.executing) {
                bool base_ready  = (entry.qbase  == -1);
                bool store_ready = (!entry.is_store) || (entry.qstore == -1);

                // MEMORY DISAMBIGUATION: RAW Hazard Penalty
                bool forward_penalty = false;
                if (!entry.is_store && base_ready) {
                    int addr = entry.base_val + entry.imm;
                    for (auto it = queue.begin(); it != queue.end(); ++it) {
                        if (&(*it) == &entry) break; 
                        
                        if (it->is_store && it->qbase == -1) {
                            if ((it->base_val + it->imm) == addr) {
                                forward_penalty = true;
                                break;
                            }
                        }
                    }
                }

                // Only start if dependencies are met
                if (base_ready && store_ready) {
                    entry.executing     = true;
                    entry.cycles_left   = forward_penalty ? latency + 1 : latency; 
                    entry.computed_addr = entry.base_val + entry.imm;
                }
                
                break; // STRICT: Only ever check/dispatch the oldest waiting instruction
            }
        }

        // 2. EXECUTE (Advance active operations, but STOP at 0)
        for (auto& entry : queue) {
            if (entry.executing && entry.cycles_left > 0) {
                entry.cycles_left--;
            }
        }
        
        // 3. BROADCAST & POP (Strictly In-Order, only check the front)
        auto& front = queue.front();
        if (front.executing && front.cycles_left == 0) {
            int addr = front.computed_addr;
            bool exc = false;
            int val  = 0;

            if (addr < 0 || addr >= (int)Memory.size()) {
                exc = true;
            } else if (!front.is_store) {
                val = Memory[addr]; 
            }

            has_result      = true;
            has_exception   = exc;
            result_tag      = front.dest_rob;
            result_val      = val;
            result_addr     = addr;
            result_is_store = front.is_store;
            store_data      = front.store_val; 
            
            queue.erase(queue.begin());
        }
    }
};

