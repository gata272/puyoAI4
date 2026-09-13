#include "ai/evaluation/weights.h"
#include <cassert>
#include <cmath>
#include <iostream>
#include <limits>
int main() {
    puyo::Weights w = puyo::amaBuildWeights();
    assert(puyo::weightCount() == 27);
    for (int i=0;i<puyo::weightCount();++i) {
        assert(puyo::weightName(i)[0] != '\0');
        double old = puyo::getWeight(w,i);
        assert(std::isfinite(old));
        assert(puyo::setWeight(w,i,old+1.25));
        assert(std::abs(puyo::getWeight(w,i)-(old+1.25)) < 1e-9);
    }
    assert(!puyo::setWeight(w,-1,1));
    assert(!puyo::setWeight(w,27,1));
    assert(!puyo::setWeight(w,0,std::numeric_limits<double>::quiet_NaN()));
    std::cout << "weight API test passed\n";
}
