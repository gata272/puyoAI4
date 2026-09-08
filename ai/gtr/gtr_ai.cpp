#include "gtr_ai.h"
#include "../search/move.h"
#include "../simulation/board.h"
#include <algorithm>
#include <map>
#include <set>
#include <string>
#include <vector>

namespace puyo::gtr {

struct Puyo {
    int main;
    int sub;
};

struct Move {
    int x;
    int rot;
    bool valid = false;
};

enum PatternType {
    NONE = 0,
    AAAB = 1,
    AABB = 2,
    ABAB = 3,
    ABAC = 4,
    AABC = 5,
    ABCC = 6,
    AAAA = 7,
    ABAA = 8,
};

static PatternType lockedType = NONE;
static bool lockedPlanReady = false;
static Move lockedPlan[3] = {
    {0, 0, false},
    {0, 0, false},
    {0, 0, false}
};

static void clearLockedPlan() {
    lockedPlan[0] = {0, 0, false};
    lockedPlan[1] = {0, 0, false};
    lockedPlan[2] = {0, 0, false};
}

static Move makeMove(int col1Based, int rot) {
    return {col1Based - 1, rot, true};
}

static Move resolveVerticalMove(int col1Based, char bottomTargetAlpha, const Puyo& p, const std::map<int, char>& cm) {
    if (cm.at(p.main) == bottomTargetAlpha) return makeMove(col1Based, 0);
    if (cm.at(p.sub) == bottomTargetAlpha) return makeMove(col1Based, 2);
    return makeMove(col1Based, 0);
}

static Move resolveHorizontalMove(int leftCol1Based, int rightCol1Based, char leftTargetAlpha, const Puyo& p, const std::map<int, char>& cm) {
    if (leftTargetAlpha == '\0') return makeMove(leftCol1Based, 3);
    if (cm.at(p.main) == leftTargetAlpha) return makeMove(leftCol1Based, 3); // mainを左、subを右(3)に配置
    if (cm.at(p.sub) == leftTargetAlpha) return makeMove(rightCol1Based, 1);  // subを左、mainを右(1)に配置
    return makeMove(leftCol1Based, 3);
}

static const char* patternTypeName(PatternType t) {
    switch (t) {
        case AAAB: return "AAAB型";
        case AABB: return "AABB型";
        case ABAB: return "ABAB型";
        case ABAC: return "ABAC型";
        case AABC: return "AABC型";
        case ABCC: return "ABCC型";
        case AAAA: return "AAAA型";
        case ABAA: return "ABAA型";
        default:   return "NONE";
    }
}

std::map<int, char> abstractColors(Puyo p1, Puyo p2, Puyo p3) {
    int charA_raw = p1.main;

    if (p1.main != p1.sub) {
        std::vector<int> p1Colors = {p1.main, p1.sub};
        std::vector<int> p2Colors = {p2.main, p2.sub};
        std::vector<int> intersect;

        for (int c : p1Colors) {
            if (std::find(p2Colors.begin(), p2Colors.end(), c) != p2Colors.end()) {
                intersect.push_back(c);
            }
        }

        if (intersect.size() == 1) charA_raw = intersect[0];
        else if (intersect.size() == 2) charA_raw = p1.main;
    }

    std::map<int, char> colorMap;
    colorMap[charA_raw] = 'A';

    std::vector<int> priorityColors = {
        p1.main, p1.sub, p2.main, p2.sub, p3.main, p3.sub
    };

    std::vector<int> uniqueColors;
    std::set<int> seen;
    for (int c : priorityColors) {
        if (seen.find(c) == seen.end()) {
            uniqueColors.push_back(c);
            seen.insert(c);
        }
    }

    char nextAlpha = 'B';
    for (int c : uniqueColors) {
        if (c != charA_raw) {
            colorMap[c] = nextAlpha++;
        }
    }

    return colorMap;
}

std::string getPattern(Puyo p, std::map<int, char>& cm) {
    std::vector<char> chars = { cm[p.main], cm[p.sub] };
    std::sort(chars.begin(), chars.end());
    std::string s = "";
    s += chars[0];
    s += chars[1];
    return s;
}

PatternType detectPatternType(const std::string& patternKey) {
    if (patternKey.rfind("AA-AB", 0) == 0) return AAAB;
    if (patternKey.rfind("AA-BB", 0) == 0) return AABB;
    if (patternKey.rfind("AB-AB", 0) == 0) return ABAB;
    if (patternKey.rfind("AB-AC", 0) == 0) return ABAC;
    if (patternKey.rfind("AA-BC", 0) == 0) return AABC;
    if (patternKey.rfind("AB-CC", 0) == 0) return ABCC;
    if (patternKey.rfind("AA-AA", 0) == 0) return AAAA;
    if (patternKey.rfind("AB-AA", 0) == 0) return ABAA;
    return NONE;
}

static bool buildLockedPlan(
    const std::string& patternKey,
    const Puyo& p1,
    const Puyo& p2,
    const Puyo& p3,
    const std::map<int, char>& cm
) {
    clearLockedPlan();
    lockedPlanReady = false;

    lockedType = detectPatternType(patternKey);


    if (lockedType == NONE) {
        return false;
    }

    bool ok = false;
    auto set = [&](int turn, const Move& m) {
        lockedPlan[turn] = m;
    };

    switch (lockedType) {
        case AAAB:
            if (patternKey == "AA-AB-AA") { set(0, resolveHorizontalMove(1, 2, '\0', p1, cm)); set(1, resolveVerticalMove(3, 'B', p2, cm)); set(2, resolveHorizontalMove(4, 5, '\0', p3, cm)); ok = true; }
            if (patternKey == "AA-AB-AB") { set(0, resolveHorizontalMove(1, 2, '\0', p1, cm)); set(1, resolveVerticalMove(3, 'B', p2, cm)); set(2, resolveVerticalMove(4, 'A', p3, cm)); ok = true; }
            if (patternKey == "AA-AB-AC") { set(0, resolveHorizontalMove(1, 2, '\0', p1, cm)); set(1, resolveVerticalMove(3, 'B', p2, cm)); set(2, resolveVerticalMove(2, 'C', p3, cm)); ok = true; }
            if (patternKey == "AA-AB-BB") { set(0, resolveHorizontalMove(1, 2, '\0', p1, cm)); set(1, resolveVerticalMove(2, 'B', p2, cm)); set(2, resolveVerticalMove(1, 'B', p3, cm)); ok = true; }
            if (patternKey == "AA-AB-BC") { set(0, resolveHorizontalMove(1, 2, '\0', p1, cm)); set(1, resolveVerticalMove(3, 'B', p2, cm)); set(2, resolveVerticalMove(4, 'C', p3, cm)); ok = true; }
            if (patternKey == "AA-AB-CC") { set(0, resolveHorizontalMove(1, 2, '\0', p1, cm)); set(1, resolveVerticalMove(3, 'B', p2, cm)); set(2, resolveHorizontalMove(1, 2, '\0', p3, cm)); ok = true; }
            if (patternKey == "AA-AB-CD") { set(0, resolveHorizontalMove(1, 2, '\0', p1, cm)); set(1, resolveVerticalMove(3, 'B', p2, cm)); set(2, resolveVerticalMove(6, 'D', p3, cm)); ok = true; }
            break;

        case AABB:
            if (patternKey == "AA-BB-AA") { set(0, resolveHorizontalMove(1, 2, '\0', p1, cm)); set(1, resolveHorizontalMove(1, 2, '\0', p2, cm)); set(2, resolveHorizontalMove(4, 5, '\0', p3, cm)); ok = true; }
            if (patternKey == "AA-BB-AB") { set(0, resolveHorizontalMove(1, 2, '\0', p1, cm)); set(1, resolveHorizontalMove(1, 2, '\0', p2, cm)); set(2, resolveHorizontalMove(1, 2, 'B', p3, cm)); ok = true; }
            if (patternKey == "AA-BB-AC") { set(0, resolveHorizontalMove(1, 2, '\0', p1, cm)); set(1, resolveHorizontalMove(1, 2, '\0', p2, cm)); set(2, resolveVerticalMove(3, 'C', p3, cm)); ok = true; }
            if (patternKey == "AA-BB-BB") { set(0, resolveHorizontalMove(1, 2, '\0', p1, cm)); set(1, resolveHorizontalMove(1, 2, '\0', p2, cm)); set(2, resolveHorizontalMove(4, 5, '\0', p3, cm)); ok = true; }
            if (patternKey == "AA-BB-BC") { set(0, resolveHorizontalMove(1, 2, '\0', p1, cm)); set(1, resolveHorizontalMove(1, 2, '\0', p2, cm)); set(2, resolveVerticalMove(1, 'B', p3, cm)); ok = true; }
            if (patternKey == "AA-BB-CC") { set(0, resolveHorizontalMove(1, 2, '\0', p1, cm)); set(1, resolveHorizontalMove(1, 2, '\0', p2, cm)); set(2, resolveHorizontalMove(4, 5, '\0', p3, cm)); ok = true; }
            if (patternKey == "AA-BB-CD") { set(0, resolveHorizontalMove(1, 2, '\0', p1, cm)); set(1, resolveHorizontalMove(1, 2, '\0', p2, cm)); set(2, resolveHorizontalMove(5, 6, 'C', p3, cm)); ok = true; }
            break;

        case ABAB:
            if (patternKey == "AB-AB-AA") { set(0, resolveVerticalMove(1, 'A', p1, cm)); set(1, resolveVerticalMove(2, 'A', p2, cm)); set(2, resolveHorizontalMove(4, 5, '\0', p3, cm)); ok = true; }
            if (patternKey == "AB-AB-AB") { set(0, resolveVerticalMove(1, 'A', p1, cm)); set(1, resolveVerticalMove(2, 'A', p2, cm)); set(2, resolveHorizontalMove(1, 2, 'B', p3, cm)); ok = true; }
            if (patternKey == "AB-AB-AC") { set(0, resolveVerticalMove(1, 'A', p1, cm)); set(1, resolveVerticalMove(2, 'A', p2, cm)); set(2, resolveVerticalMove(3, 'C', p3, cm)); ok = true; }
            if (patternKey == "AB-AB-BB") { set(0, resolveVerticalMove(1, 'B', p1, cm)); set(1, resolveVerticalMove(2, 'B', p2, cm)); set(2, resolveHorizontalMove(4, 5, '\0', p3, cm)); ok = true; }
            if (patternKey == "AB-AB-BC") { set(0, resolveVerticalMove(1, 'A', p1, cm)); set(1, resolveVerticalMove(2, 'A', p2, cm)); set(2, resolveVerticalMove(1, 'B', p3, cm)); ok = true; }
            if (patternKey == "AB-AB-CC") { set(0, resolveVerticalMove(1, 'A', p1, cm)); set(1, resolveVerticalMove(2, 'A', p2, cm)); set(2, resolveHorizontalMove(4, 5, '\0', p3, cm)); ok = true; }
            if (patternKey == "AB-AB-CD") { set(0, resolveVerticalMove(1, 'A', p1, cm)); set(1, resolveVerticalMove(2, 'A', p2, cm)); set(2, resolveHorizontalMove(5, 6, '\0', p3, cm)); ok = true; }
            break;

        case ABAC:
            if (patternKey == "AB-AC-AA") { set(0, resolveHorizontalMove(2, 3, 'A', p1, cm)); set(1, resolveVerticalMove(1, 'A', p2, cm)); set(2, resolveHorizontalMove(3, 4, '\0', p3, cm)); ok = true; }
            if (patternKey == "AB-AC-AB") { set(0, resolveVerticalMove(1, 'A', p1, cm)); set(1, resolveHorizontalMove(2, 3, 'A', p2, cm)); set(2, resolveHorizontalMove(2, 3, 'B', p3, cm)); ok = true; }
            if (patternKey == "AB-AC-AC") { set(0, resolveHorizontalMove(2, 3, 'A', p1, cm)); set(1, resolveVerticalMove(1, 'A', p2, cm)); set(2, resolveHorizontalMove(2, 3, 'C', p3, cm)); ok = true; }
            if (patternKey == "AB-AC-AD") { set(0, resolveVerticalMove(1, 'A', p1, cm)); set(1, resolveHorizontalMove(2, 3, 'A', p2, cm)); set(2, resolveHorizontalMove(3, 4, 'A', p3, cm)); ok = true; }
            if (patternKey == "AB-AC-BB") { set(0, resolveVerticalMove(1, 'A', p1, cm)); set(1, resolveHorizontalMove(2, 3, 'A', p2, cm)); set(2, resolveHorizontalMove(1, 2, '\0', p3, cm)); ok = true; }
            if (patternKey == "AB-AC-BC") { set(0, resolveHorizontalMove(2, 3, 'A', p1, cm)); set(1, resolveVerticalMove(1, 'A', p2, cm)); set(2, resolveVerticalMove(4, 'B', p3, cm)); ok = true; }
            if (patternKey == "AB-AC-BD") { set(0, resolveHorizontalMove(2, 3, 'A', p1, cm)); set(1, resolveVerticalMove(1, 'A', p2, cm)); set(2, resolveVerticalMove(4, 'D', p3, cm)); ok = true; }
            if (patternKey == "AB-AC-CC") { set(0, resolveVerticalMove(4, 'B', p1, cm)); set(1, resolveVerticalMove(3, 'A', p2, cm)); set(2, resolveHorizontalMove(1, 2, '\0', p3, cm)); ok = true; }
            if (patternKey == "AB-AC-CD") { set(0, resolveVerticalMove(1, 'A', p1, cm)); set(1, resolveHorizontalMove(2, 3, 'A', p2, cm)); set(2, resolveVerticalMove(4, 'C', p3, cm)); ok = true; }
            break;

        case AABC:
            if (patternKey == "AA-BC-AA") { set(0, resolveHorizontalMove(1, 2, '\0', p1, cm)); set(1, resolveHorizontalMove(2, 3, 'B', p2, cm)); set(2, resolveHorizontalMove(2, 3, '\0', p3, cm)); ok = true; }
            if (patternKey == "AA-BC-AB") { set(0, resolveHorizontalMove(1, 2, '\0', p1, cm)); set(1, resolveHorizontalMove(3, 4, 'B', p2, cm)); set(2, resolveHorizontalMove(5, 6, 'B', p3, cm)); ok = true; }
            if (patternKey == "AA-BC-AC") { set(0, resolveHorizontalMove(1, 2, '\0', p1, cm)); set(1, resolveHorizontalMove(3, 4, 'C', p2, cm)); set(2, resolveHorizontalMove(5, 6, 'C', p3, cm)); ok = true; }
            if (patternKey == "AA-BC-AD") { set(0, resolveHorizontalMove(1, 2, '\0', p1, cm)); set(1, resolveHorizontalMove(3, 4, 'B', p2, cm)); set(2, resolveHorizontalMove(2, 3, 'A', p3, cm)); ok = true; }
            if (patternKey == "AA-BC-BB") { set(0, resolveHorizontalMove(1, 2, '\0', p1, cm)); set(1, resolveHorizontalMove(3, 4, 'B', p2, cm)); set(2, resolveHorizontalMove(5, 6, '\0', p3, cm)); ok = true; }
            if (patternKey == "AA-BC-BC") { set(0, resolveHorizontalMove(1, 2, '\0', p1, cm)); set(1, resolveHorizontalMove(3, 4, 'B', p2, cm)); set(2, resolveVerticalMove(5, 'B', p3, cm)); ok = true; }
            if (patternKey == "AA-BC-BD") { set(0, resolveHorizontalMove(1, 2, '\0', p1, cm)); set(1, resolveVerticalMove(1, 'B', p2, cm)); set(2, resolveHorizontalMove(2, 3, 'B', p3, cm)); ok = true; }
            if (patternKey == "AA-BC-CC") { set(0, resolveHorizontalMove(1, 2, '\0', p1, cm)); set(1, resolveHorizontalMove(2, 3, 'C', p2, cm)); set(2, resolveVerticalMove(1, 'C', p3, cm)); ok = true; }
            if (patternKey == "AA-BC-CD") { set(0, resolveHorizontalMove(1, 2, '\0', p1, cm)); set(1, resolveHorizontalMove(4, 3, 'C', p2, cm)); set(2, resolveHorizontalMove(5, 6, 'C', p3, cm)); ok = true; }
            break;

        case ABCC:
            if (patternKey == "AB-CC-AA") { set(0, resolveHorizontalMove(3, 4, 'A', p1, cm)); set(1, resolveHorizontalMove(1, 2, '\0', p2, cm)); set(2, resolveHorizontalMove(5, 6, '\0', p3, cm)); ok = true; }
            if (patternKey == "AB-CC-AB") { set(0, resolveVerticalMove(4, 'A', p1, cm)); set(1, resolveHorizontalMove(1, 2, '\0', p2, cm)); set(2, resolveVerticalMove(5, 'A', p3, cm)); ok = true; }
            if (patternKey == "AB-CC-AC") { set(0, resolveHorizontalMove(3, 4, 'B', p1, cm)); set(1, resolveHorizontalMove(1, 2, '\0', p2, cm)); set(2, resolveHorizontalMove(3, 4, 'A', p3, cm)); ok = true; }
            if (patternKey == "AB-CC-AD") { set(0, resolveHorizontalMove(3, 4, 'A', p1, cm)); set(1, resolveHorizontalMove(1, 2, '\0', p2, cm)); set(2, resolveHorizontalMove(5, 6, 'A', p3, cm)); ok = true; }
            if (patternKey == "AB-CC-BB") { set(0, resolveHorizontalMove(3, 4, 'B', p1, cm)); set(1, resolveHorizontalMove(1, 2, '\0', p2, cm)); set(2, resolveHorizontalMove(5, 6, '\0', p3, cm)); ok = true; }
            if (patternKey == "AB-CC-BC") { set(0, resolveHorizontalMove(3, 4, 'A', p1, cm)); set(1, resolveHorizontalMove(1, 2, '\0', p2, cm)); set(2, resolveHorizontalMove(3, 4, 'B', p3, cm)); ok = true; }
            if (patternKey == "AB-CC-BD") { set(0, resolveHorizontalMove(3, 4, 'B', p1, cm)); set(1, resolveHorizontalMove(1, 2, '\0', p2, cm)); set(2, resolveHorizontalMove(5, 6, 'B', p3, cm)); ok = true; }
            if (patternKey == "AB-CC-CC") { set(0, resolveHorizontalMove(3, 4, 'B', p1, cm)); set(1, resolveHorizontalMove(1, 2, '\0', p2, cm)); set(2, resolveHorizontalMove(5, 6, '\0', p3, cm)); ok = true; }
            if (patternKey == "AB-CC-CD") { set(0, resolveHorizontalMove(3, 4, 'B', p1, cm)); set(1, resolveHorizontalMove(1, 2, '\0', p2, cm)); set(2, resolveHorizontalMove(2, 3, 'C', p3, cm)); ok = true; }
            if (patternKey == "AB-CC-DD") { set(0, resolveHorizontalMove(3, 4, 'B', p1, cm)); set(1, resolveHorizontalMove(1, 2, '\0', p2, cm)); set(2, resolveHorizontalMove(1, 2, '\0', p3, cm)); ok = true; }
            break;

        case AAAA:
            if (patternKey == "AA-AA-AA") { set(0, resolveVerticalMove(2, '\0', p1, cm)); set(1, resolveVerticalMove(4, '\0', p2, cm)); set(2, resolveVerticalMove(3, '\0', p3, cm)); ok = true; }
            if (patternKey == "AA-AA-AB") { set(0, resolveVerticalMove(3, '\0', p1, cm)); set(1, resolveVerticalMove(3, '\0', p2, cm)); set(2, resolveHorizontalMove(1, 2, 'A', p3, cm)); ok = true; }
            if (patternKey == "AA-AA-BB") { set(0, resolveVerticalMove(3, '\0', p1, cm)); set(1, resolveVerticalMove(3, '\0', p2, cm)); set(2, resolveHorizontalMove(1, 2, '\0', p3, cm)); ok = true; }
            if (patternKey == "AA-AA-BC") { set(0, resolveVerticalMove(3, '\0', p1, cm)); set(1, resolveVerticalMove(3, '\0', p2, cm)); set(2, resolveHorizontalMove(1, 2, 'B', p3, cm)); ok = true; }
            break;

        case ABAA:
            if (patternKey == "AB-AA-AA") { set(0, resolveVerticalMove(3, 'B', p1, cm)); set(1, resolveHorizontalMove(1, 2, '\0', p2, cm)); set(2, resolveHorizontalMove(4, 5, '\0', p3, cm)); ok = true; }
            if (patternKey == "AB-AA-AB") { set(0, resolveVerticalMove(3, 'B', p1, cm)); set(1, resolveHorizontalMove(1, 2, '\0', p2, cm)); set(2, resolveVerticalMove(4, 'A', p3, cm)); ok = true; }
            if (patternKey == "AB-AA-AC") { set(0, resolveVerticalMove(3, 'B', p1, cm)); set(1, resolveHorizontalMove(1, 2, '\0', p2, cm)); set(2, resolveVerticalMove(2, 'C', p3, cm)); ok = true; }
            if (patternKey == "AB-AA-BB") { set(0, resolveVerticalMove(3, 'B', p1, cm)); set(1, resolveHorizontalMove(1, 2, '\0', p2, cm)); set(2, resolveHorizontalMove(1, 2, '\0', p3, cm)); ok = true; }
            if (patternKey == "AB-AA-BC") { set(0, resolveVerticalMove(3, 'B', p1, cm)); set(1, resolveHorizontalMove(1, 2, '\0', p2, cm)); set(2, resolveVerticalMove(4, 'C', p3, cm)); ok = true; }
            if (patternKey == "AB-AA-CC") { set(0, resolveVerticalMove(3, 'B', p1, cm)); set(1, resolveHorizontalMove(1, 2, '\0', p2, cm)); set(2, resolveHorizontalMove(1, 2, '\0', p3, cm)); ok = true; }
            if (patternKey == "AB-AA-CD") { set(0, resolveVerticalMove(3, 'B', p1, cm)); set(1, resolveHorizontalMove(1, 2, '\0', p2, cm)); set(2, resolveVerticalMove(6, 'D', p3, cm)); ok = true; }
            break;

        default:
            break;
    }

    lockedPlanReady = ok;
    if (!ok) {
        clearLockedPlan();
    }
    return ok;
}



bool GtrAI::reset() {
    lockedType = NONE;
    lockedPlanReady = false;
    clearLockedPlan();
    return true;
}

puyo::Move GtrAI::chooseMove(
    int turn,
    const PuyoPair& q1,
    const PuyoPair& q2,
    const PuyoPair& q3
) {
    if (turn < 0 || turn > 2) return {-1, 0, false};

    Puyo p1{q1.main, q1.sub};
    Puyo p2{q2.main, q2.sub};
    Puyo p3{q3.main, q3.sub};

    auto colorMap = abstractColors(p1, p2, p3);
    std::string patternKey =
        getPattern(p1, colorMap) + "-" +
        getPattern(p2, colorMap) + "-" +
        getPattern(p3, colorMap);

    if (turn == 0) {
        if (!buildLockedPlan(patternKey, p1, p2, p3, colorMap)) {
            return {-1, 0, false};
        }
    }

    if (!lockedPlanReady || !lockedPlan[turn].valid) {
        return {-1, 0, false};
    }

    return {lockedPlan[turn].x, lockedPlan[turn].rot, true};
}

const char* GtrAI::patternName() const {
    return patternTypeName(lockedType);
}

} // namespace puyo::gtr
