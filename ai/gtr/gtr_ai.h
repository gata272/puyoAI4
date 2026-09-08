#pragma once
#include "../search/move.h"

namespace puyo::gtr {

class GtrAI {
public:
    bool reset();
    Move chooseMove(int turn,
                    const PuyoPair& q1,
                    const PuyoPair& q2,
                    const PuyoPair& q3);
    const char* patternName() const;
};

} // namespace puyo::gtr
