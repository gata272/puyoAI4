#pragma once

namespace puyo {

struct PuyoPair {
    int main;
    int sub;
};

struct Move {
    int x = -1;
    int rotation = 0;
    bool valid = false;
};

} // namespace puyo
