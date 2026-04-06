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
    void executeCycle(std::vector<int>& Memory,const std::vector<ROBEntry>& ROB) {   
        has_result     = false;
        has_exception  = false;
        result_tag     = -1;
        result_val     = 0;
        result_addr    = -1;
        result_is_store = false;
        if (queue.empty()) return;
        auto& front = queue.front();
        if (!front.executing) {
            bool base_ready  = (front.qbase  == -1);
            bool store_ready = (!front.is_store) || (front.qstore == -1);

            if (base_ready && store_ready) {
                front.executing    = true;
                front.cycles_left  = latency;
                front.computed_addr = front.base_val + front.imm;
            }
        } else {
            front.cycles_left--;

            if (front.cycles_left == 0) {
                int  addr = front.computed_addr;
                bool exc  = false;
                int  val  = 0;
                if (addr < 0 || addr >= (int)Memory.size()) {
                    exc = true;
                } else if (!front.is_store) {
                   bool forwarded    = false;
                    int  best_age     = -1;
                    int  forward_val  = 0;
                    
                    for (const auto& rob : ROB) {
                        if (!rob.valid)       continue;
                        if (!rob.is_store)    continue;
                        if (!rob.ready)       continue;   // SW hasn't executed yet
                        if (rob.age >= front.age) continue; // SW is newer than LW
                        if (rob.mem_addr != addr) continue;
                        
                        // Pick the youngest store older than this load
                        if (rob.age > best_age) {
                            best_age    = rob.age;
                            forward_val = rob.store_val;
                            forwarded   = true;
                        }
                    }
                    val = forwarded ? forward_val : Memory[addr];
                }
                has_result      = true;
                has_exception   = exc;
                result_tag      = front.dest_rob;
                result_val      = val;
                result_is_store = front.is_store;
                result_addr     = addr;
                if (front.is_store) {
                    store_data = front.store_val; 
                }

                queue.pop_front(); 
            }
        }
    }
};

