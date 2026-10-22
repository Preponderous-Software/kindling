// Targets: what the crosshair names, and the survey that counts the kinds of
// damage the player has looked at.
#include "check.h"
#include "sim/rules.h"

using namespace kindling;

int main() {
    World w = newWorld(7);

    // The ruin has the damage the survey asks for.
    CHECK(w.debris.size() == 15);
    CHECK(w.planks[0] == PlankState::Rotten);
    CHECK(w.windowBoards[3] == 1);
    CHECK(w.roofPatched == 0);
    CHECK(w.snow == 1.0f);

    Aim plank;
    plank.kind = TargetKind::RottenPlank;
    plank.index = 1;
    plank.distance = 1.5f;
    CHECK(describe(w, plank).name == "Rotten floorboard");
    CHECK(!describe(w, plank).hint.empty());

    Aim rafter;
    rafter.kind = TargetKind::Debris;
    rafter.index = 14;
    CHECK(describe(w, rafter).name == "Fallen rafter");

    Aim nothing;
    CHECK(describe(w, nothing).name.empty());

    // An out-of-range index describes nothing rather than reading past the end.
    Aim bad;
    bad.kind = TargetKind::Debris;
    bad.index = 99;
    CHECK(describe(w, bad).name.empty());
    CHECK(surveyBit(w, bad) == 0);

    // Looking at each kind of damage ticks it off the survey.
    const TargetKind kinds[] = {TargetKind::Floor, TargetKind::Debris, TargetKind::RottenPlank,
                                TargetKind::RoofHole, TargetKind::WindowBoard};
    for (TargetKind k : kinds) {
        CHECK((w.surveyed & kSurveyAll) != kSurveyAll);
        Input in;
        in.aim.kind = k;
        in.aim.index = 0;
        in.aim.distance = 2.0f;
        tick(w, in);
    }
    CHECK(w.surveyed == kSurveyAll);

    // Something seen from across the yard does not count.
    World far = newWorld(7);
    Input in;
    in.aim.kind = TargetKind::RoofHole;
    in.aim.distance = 30.0f;
    tick(far, in);
    CHECK(far.surveyed == 0);

    TEST_MAIN_END();
}
