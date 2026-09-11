#include "trigger_route.h"

#include "../simulation/simulator.h"
#include "chain_blueprint.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <functional>
#include <queue>
#include <vector>

namespace puyo {
namespace {

struct Pos { int x; int y; };
struct Group { Cell color; std::vector<Pos> cells; };

bool isColor(Cell c) { return c != Cell::Empty && c != Cell::Garbage; }
bool inside(int x, int y) { return x >= 0 && x < BOARD_WIDTH && y >= 0 && y < VISIBLE_HEIGHT; }

std::vector<Group> groupsOf(const Board& board, int wantedSize = 0) {
    std::vector<Group> groups;
    bool seen[BOARD_WIDTH][VISIBLE_HEIGHT]{};
    constexpr int dx[4] = {1,-1,0,0};
    constexpr int dy[4] = {0,0,1,-1};
    for (int y=0; y<VISIBLE_HEIGHT; ++y) for (int x=0; x<BOARD_WIDTH; ++x) {
        if (seen[x][y] || !isColor(board.get(x,y))) continue;
        Group g{board.get(x,y), {}};
        std::queue<Pos> q;
        q.push({x,y}); seen[x][y]=true;
        while (!q.empty()) {
            Pos p=q.front(); q.pop(); g.cells.push_back(p);
            for (int d=0; d<4; ++d) {
                int nx=p.x+dx[d], ny=p.y+dy[d];
                if (!inside(nx,ny) || seen[nx][ny] || board.get(nx,ny)!=g.color) continue;
                seen[nx][ny]=true; q.push({nx,ny});
            }
        }
        if (wantedSize==0 || static_cast<int>(g.cells.size())==wantedSize) groups.push_back(std::move(g));
    }
    return groups;
}

// Remove exactly one latent 3-group, then let gravity + ordinary resolution
// run.  This answers the key question: "if this group is the trigger, how
// much chain follows?"  It is deliberately based on the real simulator.
int activateGroup(const Board& board, const Group& trigger, Board* after = nullptr) {
    Board test = board;
    for (const Pos& p : trigger.cells) test.set(p.x,p.y,Cell::Empty);
    int score=0, erased=0;
    const int chains = Simulator::resolveBoard(test, score, erased);
    if (after) *after = test;
    return chains;
}

// A depends on B when removing B's exact-3 group causes A to become a
// 4+-group after gravity.  This covers the user's vertical BA/AAA motif and
// the horizontal BAAA/A motif without hard-coding either geometry.
void localGravity(Board& board) {
    for (int x=0; x<BOARD_WIDTH; ++x) {
        int writeY=0;
        for (int y=0; y<BOARD_HEIGHT; ++y) {
            const Cell c=board.get(x,y);
            if (c!=Cell::Empty) board.set(x,writeY++,c);
        }
        while (writeY<BOARD_HEIGHT) board.set(x,writeY++,Cell::Empty);
    }
}

bool dependsOn(const Board& board, const Group& b, Cell a) {
    Board test=board;
    for (const Pos& p : b.cells) test.set(p.x,p.y,Cell::Empty);
    localGravity(test);
    for (const Group& g : groupsOf(test, 0)) {
        if (g.color == a && g.cells.size() >= 4) return true;
    }
    return false;
}

int relayGraphDepth(const Board& board) {
    const auto triggers = groupsOf(board,3);
    if (triggers.empty()) return 0;

    // Each color has at most one useful exact-3 trigger in a compact board;
    // use the strongest trigger per color so unrelated duplicates do not
    // inflate the route length.
    std::array<bool,5> present{};
    for (const auto& g : triggers) {
        present[static_cast<int>(g.color)] = true;
    }

    int longest = 1;
    // Colors are only four, so an exhaustive simple-path search is tiny.
    std::array<std::array<bool,5>,5> edge{};
    for (const auto& b : triggers) {
        const int bi=static_cast<int>(b.color);
        for (int ai=1; ai<=4; ++ai) {
            if (ai==bi || !present[ai]) continue;
            if (dependsOn(board,b,static_cast<Cell>(ai))) edge[bi][ai]=true;
        }
    }

    std::array<bool,5> used{};
    std::function<int(int)> dfs = [&](int c) {
        used[c]=true;
        int bestLen=1;
        for (int n=1; n<=4; ++n) if (edge[c][n] && !used[n]) bestLen=std::max(bestLen,1+dfs(n));
        used[c]=false;
        return bestLen;
    };
    for (int c=1;c<=4;++c) if (present[c]) longest=std::max(longest,dfs(c));
    return longest;
}

// Structural score used during the normal beam search.  The highest priority
// is a longer dependency route; activation potential breaks ties.  Small
// exact-3 groups without a dependency are still useful as future anchors.
[[maybe_unused]] double relayStructuralScore(const Board& board) {
    const auto triggers = groupsOf(board,3);
    if (triggers.empty()) return 0.0;

    const int depth = relayGraphDepth(board);
    double score = depth * 18000.0;
    for (const auto& g : triggers) {
        // An exact-3 group is a marked trigger candidate.  A trigger that
        // itself releases a cascade is especially valuable, but a zero-chain
        // activation is still a valid bottom anchor and must not be discarded.
        score += 1200.0 + std::min(activateGroup(board,g), 8) * 1800.0;
    }
    return std::min(score, 120000.0);
}

// Reward trigger colors that can actually be advanced by the visible queue.
// A desired predecessor color missing from the queue is not treated as a
// failure: the anchor itself still receives the structural score above, so
// the AI can wait without destroying it.


} // namespace

int triggerRouteLength(const Board& board) { return analyzeChainBlueprint(board, {}).longestPath; }
double triggerRelayScore(const Board& board) { return chainBlueprintScore(board, {}); }
double triggerAnchorValue(const Board& board) {
    const auto triggers = groupsOf(board,3);
    double best = 0.0;
    for (const auto& g : triggers) {
        // Keep the raw cascade estimate separate from route length: the
        // anchor is the concrete exact-3 group the AI is trying not to lose.
        best = std::max(best, 1000.0 + 2500.0 * std::min(activateGroup(board,g), 8));
    }
    return best;
}

double triggerQueueScore(const Board& board, const std::vector<PuyoPair>& pieces) {
    const auto triggers = groupsOf(board,3);
    if (triggers.empty() || pieces.empty()) return 0.0;

    std::array<bool,5> queued{};
    const std::size_t n = std::min<std::size_t>(3, pieces.size());
    for (std::size_t i=0; i<n; ++i) {
        if (pieces[i].main >= 1 && pieces[i].main <= 4) queued[pieces[i].main] = true;
        if (pieces[i].sub >= 1 && pieces[i].sub <= 4) queued[pieces[i].sub] = true;
    }

    double score = 0.0;
    for (const auto& g : triggers) {
        const int target = static_cast<int>(g.color);
        bool predecessorAvailable = false;
        for (const auto& b : triggers) {
            const int bi = static_cast<int>(b.color);
            if (bi == target) continue;
            if (dependsOn(board, b, static_cast<Cell>(target)) && queued[bi]) {
                predecessorAvailable = true;
                break;
            }
        }
        if (predecessorAvailable) score += 7000.0;
    }
    return std::min(score, 28000.0);
}
double triggerRouteScore(const Board& board) { return triggerRelayScore(board); }


double preparedGroupScore(const Board& board) {
    const auto gs = groupsOf(board, 0);
    double score = 0.0;
    int triples = 0;
    int pairs = 0;

    for (const auto& g : gs) {
        if (g.cells.size() == 3) {
            ++triples;
            // Exact 3 is the primary "marked trigger/target" state.
            score += 1800.0;
        } else if (g.cells.size() == 2) {
            ++pairs;
            score += 350.0;
        }
    }

    // A triple that can be turned into another colour's 4+ group after its
    // activation is precisely the user's 3+1 / 2+2 hand-off motif.
    for (const auto& trigger : groupsOf(board, 3)) {
        for (int c = 1; c <= 4; ++c) {
            if (c == static_cast<int>(trigger.color)) continue;
            if (dependsOn(board, trigger, static_cast<Cell>(c))) {
                score += 6000.0;
            }
        }
    }

    score += std::min(triples, 6) * 500.0;
    score += std::min(pairs, 8) * 100.0;
    return std::min(score, 60000.0);
}

double postTriggerTailScore(const Board& board) {
    double best = 0.0;

    // Treat each exact-3 group as a hypothetical trigger.  activateGroup()
    // removes only that group, then runs the real simulator resolution.  This
    // exposes the chain tail that is invisible in the pre-trigger board.
    for (const auto& trigger : groupsOf(board, 3)) {
        Board after;
        const int chains = activateGroup(board, trigger, &after);
        if (chains <= 0) continue;

        // Count groups which are now fireable.  These are the groups that were
        // latent before the trigger and became part of the post-trigger tail.
        int newlyFireable = 0;
        for (const auto& g : groupsOf(after, 0)) {
            if (g.cells.size() >= 4) ++newlyFireable;
        }

        best = std::max(best,
            static_cast<double>(chains) * 9000.0 +
            static_cast<double>(newlyFireable) * 3500.0);
    }
    return std::min(best, 80000.0);
}

double prematureTriggerRisk(const Board& board) {
    bool hasFiringGroup = false;
    for (const auto& g : groupsOf(board, 0)) {
        if (g.cells.size() >= 4) {
            hasFiringGroup = true;
            break;
        }
    }
    if (!hasFiringGroup) return 0.0;

    const double prepared = preparedGroupScore(board);
    const double route = triggerRouteScore(board);
    // Only penalize premature firing when there is meaningful latent structure
    // that the firing would cut short.
    const double latent = prepared + route;
    return latent > 12000.0 ? std::min(30000.0, latent * 0.20) : 0.0;
}

} // namespace puyo
