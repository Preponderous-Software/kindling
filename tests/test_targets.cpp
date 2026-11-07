// Targets: what the crosshair names, and the survey that counts the kinds of
// damage the player has looked at. describe() is the text under the crosshair at
// every step, so each kind of target is checked in each state it can name.
#include <algorithm>
#include <string>

#include "check.h"
#include "sim/progress.h"
#include "sim/rules.h"

using namespace kindling;

namespace {

Aim aimAt(TargetKind kind, int32_t index = -1, Vec3 point = {}) {
    Aim a;
    a.kind = kind;
    a.index = index;
    a.point = point;
    return a;
}

bool says(const World& w, const Aim& aim, const std::string& name, const std::string& hint) {
    const Description d = describe(w, aim);
    return d.name == name && d.hint == hint;
}

bool silent(const World& w, const Aim& aim) { return says(w, aim, "", ""); }

void fillGrime(World& w, Surface s, uint8_t value) {
    std::vector<uint8_t>& texels = w.grime.texels[static_cast<size_t>(s)];
    std::fill(texels.begin(), texels.end(), value);
}

void cleanEverything(World& w) {
    for (int s = 0; s < kSurfaceCount; ++s) fillGrime(w, static_cast<Surface>(s), 0);
}

Vec3 middle(Surface s) {
    const SurfaceDef& d = surfaceDef(s);
    return d.origin + d.u * (d.uLength / 2) + d.v * (d.vLength / 2);
}

int32_t itemIndex(const World& w, ItemKind kind) {
    for (size_t i = 0; i < w.items.size(); ++i)
        if (w.items[i].kind == kind) return static_cast<int32_t>(i);
    return -1;
}

void carryItem(World& w, ItemKind kind) {
    w.carry = Carry::Item;
    w.carryIndex = itemIndex(w, kind);
}

// A surface offers its cleaning hint only over a texel still dirty, and which
// surface is asked depends on the wall's index.
void checkGrimeHints() {
    World w = newWorld(7);
    cleanEverything(w);
    const Aim boards = aimAt(TargetKind::Floor, -1, middle(Surface::Floor));
    CHECK(says(w, boards, "Floorboards", ""));
    fillGrime(w, Surface::Floor, kCleanThreshold);
    CHECK(says(w, boards, "Floorboards", ""));  // at the threshold counts as clean
    fillGrime(w, Surface::Floor, static_cast<uint8_t>(kCleanThreshold + 1));
    CHECK(says(w, boards, "Floorboards", "Thick with dust and dead leaves. Sweep them (1)."));
    // Off the surface, nothing is dirty.
    CHECK(says(w, aimAt(TargetKind::Floor, -1, {100, 0, 100}), "Floorboards", ""));

    struct WallCase {
        int32_t index;
        Surface surface;
    };
    const WallCase walls[] = {{0, Surface::BackWall},
                              {1, Surface::LeftWall},
                              {2, Surface::RightWall},
                              {3, Surface::FrontWall},
                              {7, Surface::FrontWall}};  // any other index reads the front wall
    for (const WallCase& c : walls) {
        cleanEverything(w);
        const Aim wall = aimAt(TargetKind::Wall, c.index, middle(c.surface));
        CHECK(says(w, wall, "Log wall", ""));
        fillGrime(w, c.surface, 255);
        CHECK(says(w, wall, "Log wall", "Grimy. Scrub it (2)."));
    }

    cleanEverything(w);
    const Aim stones = aimAt(TargetKind::HearthFace, -1, middle(Surface::HearthFace));
    CHECK(says(w, stones, "Chimney stones", ""));
    fillGrime(w, Surface::HearthFace, 255);
    CHECK(says(w, stones, "Chimney stones", "Black with old soot. Scrub them (2)."));
}

// The firebox walks the player through laying a fire, one hint at a time.
void checkFirebox() {
    World w = newWorld(7);
    const Aim box = aimAt(TargetKind::Firebox);
    w.hearth.ash = 1;
    w.repaired = false;
    CHECK(says(w, box, "Cold hearth", "Full of old ash: shovel it out (3) when you come to lay the fire."));
    w.repaired = true;
    CHECK(says(w, box, "Cold hearth", "Full of old ash. Shovel it out (3)."));
    w.hearth.ash = 0;
    CHECK(says(w, box, "Clean hearth", "Lay kindling here (E), from the chopping block."));
    w.hearth.kindlingLaid = true;
    CHECK(says(w, box, "Kindling laid", "Lay a split log on the andirons (E)."));
    w.hearth.logsLaid = 1;
    CHECK(says(w, box, "A fire laid", "Strike a match and hold it to the kindling (8, hold)."));
    w.hearth.lit = true;
    w.hearth.fuelMinutes = 59;
    CHECK(says(w, box, "The fire", "Burning low. Lay another split log (E)."));
    w.hearth.fuelMinutes = 60;
    CHECK(says(w, box, "The fire", ""));
    w.hearth.ash = 1;  // a lit fire is named first, whatever else is in the firebox
    CHECK(says(w, box, "The fire", ""));
}

void checkDebris() {
    World w = newWorld(7);
    const Aim piece = aimAt(TargetKind::Debris, 0);
    w.debris[0].kind = DebrisKind::Board;
    CHECK(says(w, piece, "Broken board", "Haul it out of the cabin."));
    w.debris[0].kind = DebrisKind::Shingle;
    CHECK(says(w, piece, "Fallen shingle", "Haul it out of the cabin."));
    w.debris[0].kind = DebrisKind::Rafter;
    CHECK(says(w, piece, "Fallen rafter", "Too long to carry. Saw it into pieces (4)."));
    w.debris[0].kind = DebrisKind::RafterPiece;
    CHECK(says(w, piece, "Length of rafter", "Haul it out of the cabin."));
    w.debris[0].hauled = true;
    CHECK(silent(w, piece));
    CHECK(silent(w, aimAt(TargetKind::Debris, -1)));
}

// The hint for a repair changes with what is in the player's hands.
void checkRepairs() {
    World w = newWorld(7);
    const Aim plank = aimAt(TargetKind::RottenPlank, 0);
    w.planks[0] = PlankState::Rotten;
    CHECK(says(w, plank, "Rotten floorboard", "Pry it up with the pry bar (5)."));
    carryItem(w, ItemKind::Plank);
    CHECK(says(w, plank, "Rotten floorboard", "Hold the left button to pry it up, planks and all."));
    w.planks[0] = PlankState::Removed;
    CHECK(says(w, plank, "Gap in the floor", "Hold the left button to work the plank in."));
    carryItem(w, ItemKind::Shingles);  // the wrong material
    CHECK(says(w, plank, "Gap in the floor", "Bring a new plank from the wagon and work it in."));
    w.carry = Carry::Debris;  // carryIndex is then an index into debris, not items
    w.carryIndex = itemIndex(w, ItemKind::Plank);
    CHECK(says(w, plank, "Gap in the floor", "Bring a new plank from the wagon and work it in."));
    w.carry = Carry::Item;
    w.carryIndex = 999;
    CHECK(says(w, plank, "Gap in the floor", "Bring a new plank from the wagon and work it in."));
    w.carry = Carry::Nothing;
    w.carryIndex = -1;
    w.planks[0] = PlankState::New;
    CHECK(says(w, plank, "New floorboard", ""));
    CHECK(silent(w, aimAt(TargetKind::RottenPlank, static_cast<int32_t>(w.planks.size()))));
    CHECK(silent(w, aimAt(TargetKind::RottenPlank, -1)));

    const Aim board = aimAt(TargetKind::WindowBoard, 0);
    w.windowBoards[0] = 1;
    CHECK(says(w, board, "Board nailed over the window", "Pry it off to let the light in (5)."));
    w.windowBoards[0] = 0;
    CHECK(says(w, board, "Window", ""));
    CHECK(says(w, aimAt(TargetKind::WindowBoard, static_cast<int32_t>(w.windowBoards.size())), "Window", ""));

    const Aim hole = aimAt(TargetKind::RoofHole, 0);
    w.roofPatched = kRoofAllPatched & ~1u;  // one section still open
    CHECK(says(w, hole, "Hole in the roof", "Bring shingles from the wagon and work them in."));
    carryItem(w, ItemKind::Plank);  // the wrong material
    CHECK(says(w, hole, "Hole in the roof", "Bring shingles from the wagon and work them in."));
    carryItem(w, ItemKind::Shingles);
    CHECK(says(w, hole, "Hole in the roof", "Hold the left button to work the shingles in."));
    w.roofPatched = kRoofAllPatched;
    CHECK(says(w, hole, "Patched roof", ""));

    const Aim snow = aimAt(TargetKind::Snow);
    w.snow = 0.1f;
    CHECK(says(w, snow, "Snow drift", "Shovel it out (3) before it melts into the floor."));
    w.snow = 0;
    CHECK(silent(w, snow));

    const Aim chair = aimAt(TargetKind::Chair);
    w.chair = ChairState::Broken;
    CHECK(says(w, chair, "Broken rocking chair", "Mend it with the hammer (6)."));
    w.chair = ChairState::Mended;
    CHECK(says(w, chair, "Rocking chair", ""));

    const Aim table = aimAt(TargetKind::Table);
    w.table = TableState::Toppled;
    CHECK(says(w, table, "Overturned table", "Set it back on its legs (E)."));
    w.table = TableState::Upright;
    CHECK(says(w, table, "Table", ""));

    const Aim outside = aimAt(TargetKind::OutsideWall, 0);
    w.cleared = false;
    CHECK(says(w, outside, "Log wall, outside", "Weathered, not dirty: the inside needs scrubbing."));
    w.cleared = true;
    CHECK(says(w, outside, "Log wall, outside", ""));
}

void checkYard() {
    World w = newWorld(7);
    CHECK(says(w, aimAt(TargetKind::Ceiling), "Ceiling boards", ""));
    CHECK(silent(w, aimAt(TargetKind::Ground)));
    CHECK(says(w, aimAt(TargetKind::Wagon), "Your wagon", "Tools, planks, shingles and furnishings."));
    CHECK(says(w, aimAt(TargetKind::Stall), "Stall", ""));
    CHECK(says(w, aimAt(TargetKind::SplitWood), "Split logs", "Carry them in to the woodpile (E)."));
    CHECK(says(w, aimAt(TargetKind::Kindling), "Kindling", "Carry it to the hearth (E)."));

    w.wood.roundOnBlock = false;
    CHECK(says(w, aimAt(TargetKind::ChoppingBlock), "Chopping block", "Set a log round on it (E)."));
    w.wood.roundOnBlock = true;
    CHECK(says(w, aimAt(TargetKind::ChoppingBlock), "Round on the block", "Split it with the axe (7)."));

    w.wood.rounds = 1;
    CHECK(says(w, aimAt(TargetKind::LogPile), "Log rounds", "Take one to the chopping block (E)."));
    w.wood.rounds = 0;
    CHECK(says(w, aimAt(TargetKind::LogPile), "Log pile", "All split."));

    w.carry = Carry::Nothing;
    CHECK(says(w, aimAt(TargetKind::HayBale), "Hay bale", "Take an armful for the mare (E)."));
    w.carry = Carry::Hay;
    CHECK(says(w, aimAt(TargetKind::HayBale), "Hay bale", "Put the hay back (E)."));
}

// The woodpile counts logs toward its goal, including one laid in the hearth.
void checkWoodpile() {
    World w = newWorld(7);
    const Aim pile = aimAt(TargetKind::Woodpile);
    w.wood.woodpile = kWoodpileGoal - 1;
    CHECK(says(w, pile, "Woodpile (3 logs)", "Stack split logs here (E)."));
    w.hearth.logsLaid = 1;
    CHECK(says(w, pile, "Woodpile (3 logs)", "Stacked. E takes a log for the fire."));
    w.hearth.logsLaid = 0;
    w.wood.woodpile = kWoodpileGoal;
    CHECK(says(w, pile, "Woodpile (4 logs)", "Stacked. E takes a log for the fire."));
    w.carry = Carry::Hay;  // hands full: the offer is not made
    CHECK(says(w, pile, "Woodpile (4 logs)", ""));
}

// The mare's line: eating first, then hunger, then dust.
void checkMare() {
    World w = newWorld(7);
    const Aim mare = aimAt(TargetKind::Horse);
    w.horse.eatingSeconds = 1;
    w.horse.hunger = 1;
    w.horse.dirt = 1;
    CHECK(says(w, mare, "Your mare", "Eating."));
    w.horse.eatingSeconds = 0;
    CHECK(says(w, mare, "Your mare", "She's hungry. Bring her hay from the bale."));
    w.horse.hunger = 0.5f;
    CHECK(says(w, mare, "Your mare", "Dusty from the trail. Brush her (9)."));
    w.horse.dirt = 0.05f;
    CHECK(says(w, mare, "Your mare", "Content."));
}

void checkSettler() {
    World w = newWorld(7);
    const Aim ada = aimAt(TargetKind::Settler);
    w.settler.state = SettlerState::ToChair;
    CHECK(says(w, ada, "Ada Lark", ""));
    w.settler.state = SettlerState::Seated;
    CHECK(says(w, ada, "Ada Lark", "Settled by the fire."));
}

void checkItems() {
    World w = newWorld(7);
    const Aim plank = aimAt(TargetKind::Item, itemIndex(w, ItemKind::Plank));
    const Aim shingles = aimAt(TargetKind::Item, itemIndex(w, ItemKind::Shingles));
    const int32_t rugIndex = itemIndex(w, ItemKind::Rug);
    const int32_t candleIndex = itemIndex(w, ItemKind::Candle);
    const Aim rug = aimAt(TargetKind::Item, rugIndex);
    const Aim candle = aimAt(TargetKind::Item, candleIndex);

    CHECK(says(w, plank, "New plank", "Take the planks (E)."));
    CHECK(says(w, shingles, "Bundle of shingles", "Take the shingles (E)."));
    w.repaired = false;
    CHECK(says(w, rug, "Braided rug", "For later, once the cabin is mended."));
    w.repaired = true;
    CHECK(says(w, rug, "Braided rug", "Take it (E)."));

    w.items[static_cast<size_t>(rugIndex)].place = ItemPlace::Placed;
    CHECK(says(w, rug, "Braided rug", "Move it (E)."));
    w.items[static_cast<size_t>(rugIndex)].place = ItemPlace::Carried;
    CHECK(silent(w, rug));
    w.items[static_cast<size_t>(rugIndex)].place = ItemPlace::Used;
    CHECK(silent(w, rug));

    CHECK(says(w, candle, "Candle", "Take it (E)."));  // not lit from the wagon
    w.items[static_cast<size_t>(candleIndex)].place = ItemPlace::Placed;
    CHECK(says(w, candle, "Candle", "Light it (8, hold), or move it (E)."));
    w.hearth.candleLit = true;
    CHECK(says(w, candle, "Candle", "Move it (E)."));

    CHECK(silent(w, aimAt(TargetKind::Item, static_cast<int32_t>(w.items.size()))));
    CHECK(silent(w, aimAt(TargetKind::Item, -1)));
}

// A target counts toward the survey only while the damage is still there.
void checkSurveyBits() {
    World w = newWorld(7);
    CHECK(surveyBit(w, aimAt(TargetKind::Floor)) == kSurveyGrime);
    CHECK(surveyBit(w, aimAt(TargetKind::Wall, 2)) == kSurveyGrime);
    CHECK(surveyBit(w, aimAt(TargetKind::HearthFace)) == kSurveyGrime);

    const Aim piece = aimAt(TargetKind::Debris, 0);
    w.debris[0].hauled = false;
    CHECK(surveyBit(w, piece) == kSurveyDebris);
    w.debris[0].hauled = true;
    CHECK(surveyBit(w, piece) == 0);

    const Aim plank = aimAt(TargetKind::RottenPlank, 0);
    w.planks[0] = PlankState::Rotten;
    CHECK(surveyBit(w, plank) == kSurveyRottenPlank);
    w.planks[0] = PlankState::Removed;
    CHECK(surveyBit(w, plank) == 0);
    w.planks[0] = PlankState::New;
    CHECK(surveyBit(w, plank) == 0);
    CHECK(surveyBit(w, aimAt(TargetKind::RottenPlank, static_cast<int32_t>(w.planks.size()))) == 0);

    const Aim hole = aimAt(TargetKind::RoofHole, 0);
    w.roofPatched = 0;
    CHECK(surveyBit(w, hole) == kSurveyRoofHole);
    w.roofPatched = kRoofAllPatched & ~1u;
    CHECK(surveyBit(w, hole) == kSurveyRoofHole);
    w.roofPatched = kRoofAllPatched;
    CHECK(surveyBit(w, hole) == 0);

    const Aim board = aimAt(TargetKind::WindowBoard, 0);
    w.windowBoards[0] = 1;
    CHECK(surveyBit(w, board) == kSurveyBoardedWindow);
    w.windowBoards[0] = 0;
    CHECK(surveyBit(w, board) == 0);
    CHECK(surveyBit(w, aimAt(TargetKind::WindowBoard, static_cast<int32_t>(w.windowBoards.size()))) == 0);

    // Nothing else is damage the survey asks about.
    for (TargetKind k : {TargetKind::None, TargetKind::Snow, TargetKind::Chair, TargetKind::Item, TargetKind::Ground})
        CHECK(surveyBit(w, aimAt(k, 0)) == 0);
}

}  // namespace

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

    checkGrimeHints();
    checkFirebox();
    checkDebris();
    checkRepairs();
    checkYard();
    checkWoodpile();
    checkMare();
    checkSettler();
    checkItems();
    checkSurveyBits();

    TEST_MAIN_END();
}
