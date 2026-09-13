#include "../ai/search/beam_search.h"
#include "../ai/evaluation/features.h"
#include "../ai/evaluation/weights.h"
#include "../ai/simulation/simulator.h"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <random>
#include <string>
#include <vector>

using namespace puyo;

constexpr int kTunedWeights = 27;

struct Vec { double v[kTunedWeights]; };

Vec toVec(const Weights& w) {
    Vec out{};
    for (int i = 0; i < kTunedWeights; ++i) out.v[i] = getWeight(w, i);
    return out;
}

Weights fromVec(const Vec& x) {
    Weights w = amaBuildWeights();
    for (int i = 0; i < kTunedWeights; ++i) {
        if (!setWeight(w, i, x.v[i])) return amaBuildWeights();
    }
    return w;
}

std::vector<PuyoPair> queueFor(std::mt19937& rng, int n) {
    std::uniform_int_distribution<int> c(1,4);
    std::vector<PuyoPair> q(n);
    for(auto& p:q) p={c(rng),c(rng)};
    return q;
}

double evaluateWeights(const Vec& x, int seed, int games, int turns) {
    const Weights w=fromVec(x);
    BeamSearch search;
    std::mt19937 rng(seed);
    double objective=0;
    for(int g=0;g<games;++g){
        Board board;
        auto q=queueFor(rng,turns+6);
        int score=0, chains=0, survived=0, maxChain=0;
        for(int t=0;t<turns;++t){
            std::vector<PuyoPair> pieces(q.begin()+t,q.begin()+std::min<int>(q.size(),t+6));
            Move m=search.chooseMove(board,pieces,w,2,4);
            if(!m.valid) break;
            auto sim=Simulator::drop(board,pieces[0],m);
            if(sim.gameOver && !sim.allClear) break;
            board=sim.board; score+=sim.score; chains+=sim.chains; maxChain=std::max(maxChain, sim.chains); ++survived;
        }
        const auto h = board.heights();
        double heightPenalty = 0.0;
        for (int v : h) heightPenalty += v;
        const Features finalF = extractStaticFeatures(board);
        // The terminal board terms keep SPSA informative even on short
        // samples where no full chain happens to occur.
        objective += 10000.0*maxChain + 1000.0*chains + 250.0*survived
                   - 35.0*heightPenalty
                   + 40.0*finalF.link2 + 70.0*finalF.link3
                   - 100.0*finalF.nuisance;
    }
    return objective/games;
}

void clamp(Vec& x) {
    static const double lo[kTunedWeights]={
        100,-500,-1000,0,-500,-500,-500,-100,-500,-500,-500,-200,-500,-1000,-1000,
        0,0,-1500,-1000,-1500,-1500,0,0,-500,0,-1000,-1000
    };
    static const double hi[kTunedWeights]={
        5000,1000,500,1000,500,500,500,500,1000,1000,1000,200,500,0,0,
        2500,3000,0,0,0,0,300,400,0,300,0,0
    };
    for(int i=0;i<kTunedWeights;++i) x.v[i]=std::max(lo[i],std::min(hi[i],x.v[i]));
}

void writeJson(const Vec& x,const std::string& path){
    std::ofstream o(path);
    o<<"{\n  \"profile\": \"spsa\",\n  \"weights\": {\n";
    for(int i=0;i<kTunedWeights;++i) {
        o<<"    \""<<weightName(i)<<"\": "<<std::llround(x.v[i])<<(i==kTunedWeights-1?"\n":" ,\n");
    }
    o<<"  }\n}\n";
}

int main(int argc,char**argv){
    int iterations=20,games=6,turns=35,seed=20260908;
    std::string out="config/weights_tuned_experimental.json";
    if(argc>1) iterations=std::max(1,std::atoi(argv[1]));
    if(argc>2) games=std::max(1,std::atoi(argv[2]));
    if(argc>3) turns=std::max(10,std::atoi(argv[3]));
    if(argc>4) out=argv[4];

    Vec x=toVec(amaBuildWeights());
    std::mt19937 rng(seed);
    double best=evaluateWeights(x,seed,games,turns);
    Vec bestX=x;
    std::cout<<"initial="<<best<<"\n";

    // SPSA: two evaluations per iteration regardless of parameter count.
    const double a=180.0, c=25.0, A=iterations*0.15, alpha=0.602, gamma=0.101;
    for(int k=0;k<iterations;++k){
        const double ak=a/std::pow(k+1+A,alpha);
        const double ck=c/std::pow(k+1,gamma);
        Vec plus=x, minus=x;
        int delta[kTunedWeights];
        for(int i=0;i<kTunedWeights;++i){ delta[i]=(rng()&1)?1:-1; plus.v[i]+=ck*delta[i]; minus.v[i]-=ck*delta[i]; }
        clamp(plus); clamp(minus);
        const int commonSeed = seed + k*17;
        const double yp=evaluateWeights(plus,commonSeed,games,turns);
        const double ym=evaluateWeights(minus,commonSeed,games,turns);
        for(int i=0;i<kTunedWeights;++i) x.v[i]+=ak*((yp-ym)/(2*ck*delta[i]));
        clamp(x);
        const double y=evaluateWeights(x,commonSeed,games,turns);
        if(y>best){best=y;bestX=x;}
        std::cout<<"iter="<<(k+1)<<" score="<<y<<" best="<<best<<"\n";
    }
    writeJson(bestX,out);
    std::cout<<"best weights written to "<<out<<"\n";
}
