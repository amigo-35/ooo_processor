#pragma once
#include "Basics.h"
#include <iostream>
#include <vector>
#include <unordered_map>

class BranchPredictor {

private:
  private:
    std::unordered_map<int, int> state_table;

    int getState(int pc) {
        auto it = state_table.find(pc);
        return (it != state_table.end()) ? it->second : 0;
    }
  public:
    int total_branches = 0;
    int correct_predictions = 0;

   public:
    int predict(int pc, int imm, OpCode op) {
        if (op == OpCode::J) return pc + imm;

        int state = getState(pc);
        return (state < 2) ? (pc + imm) : (pc + 1);
    }

   
    void update(int pc, int actual_target, bool taken, bool was_correct) {
        int& state = state_table[pc];
        if (taken) {
            if (state == 1) state = 0;
            else if (state == 2) state = 1;
            else if (state == 3) state = 2;
            // state 0: no change
        } else {
            if (state == 0) state = 1;
            else if (state == 1) state = 2;
            else if (state == 2) state = 3;
            // state 3: no change
        }
        total_branches++;
        if (was_correct) correct_predictions++;
    }
};
