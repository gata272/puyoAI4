#include "../ai/search/beam_search.h"
#include "../ai/evaluation/weights.h"
#include "../ai/simulation/simulator.h"

#include <array>
#include <iomanip>
#include <iostream>
#include <random>
#include <vector>

using namespace puyo;

struct Result { double score=0; double chains=0; int survived=0; };

std::vector<PuyoPair> makeQueue(std::mt19937& rng, int n) {
    std::uniform_int_distribution<int> c(1,4);
    std::vector<PuyoPair> q(n);
    for (auto& p : q) p={c(rng),c(rng)};
    return q;
}

Result run(int depth, int width, int games, int turns) {
    std::mt19937 rng(0xA17E + depth*101 + width*17);
    BeamSearch search;
    const auto w = amaBuildWeights();
    Result total;
    for (int g=0; g<games; ++g) {
        Board board;
        auto q = makeQueue(rng, turns+3);
        int score=0, chains=0;
        for (int t=0; t<turns; ++t) {
            std::vector<PuyoPair> pieces(q.begin()+t, q.begin()+std::min<int>(q.size(),t+3));
            Move m=search.chooseMove(board,pieces,w,depth,width);
            if (!m.valid) break;
            auto sim=Simulator::drop(board,pieces[0],m);
            if (sim.gameOver && !sim.allClear) break;
            board=sim.board; score+=sim.score; chains+=sim.chains;
            total.survived++;
        }
        total.score += score;
        total.chains += chains;
    }
    total.score/=games; total.chains/=games;
    return total;
}

int main(int argc,char**argv){
    int games=4, turns=50;
    if(argc>1) games=std::max(1,std::atoi(argv[1]));
    if(argc>2) turns=std::max(10,std::atoi(argv[2]));
    std::cout<<"depth,width,avg_score,avg_chains,avg_survival\n";
    for(int depth: {2,3,4}) for(int width: {4,8,12,16}) {
        auto r=run(depth,width,games,turns);
        std::cout<<depth<<","<<width<<","<<std::fixed<<std::setprecision(1)
                 <<r.score<<","<<r.chains<<","<<(double)r.survived/games<<"\n";
    }
}
