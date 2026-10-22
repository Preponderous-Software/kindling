// What the player sees: the HUD, the camera, the sky, and the reveal (the
// restoration played back from its keyframes). Shared by the desktop game
// (app/main.cpp) and the browser game (app/web_main.cpp).
#include "play/hud.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

#include "record/replay.h"
#include "sim/brief.h"
#include "sim/layout.h"
#include "sim/progress.h"
#include "sim/settler.h"

namespace kindling::play {

namespace L = kindling::layout;

namespace {
constexpr float kFovY = 70.0f;
}  // namespace
void setLighting(strata::Engine& engine, bool lighter) {
    StrataLighting light = strata::lightingDefaults(STRATA_LIGHTING_SUN);
    light.cascade_count = lighter ? 3 : 4;
    light.shadow_map_size = lighter ? 1024 : 2048;
    light.shadow_distance = 45.0f;
    light.fog = 1;
    engine.setLighting(light);
}

// The sun (or the moon's sky) at the game's time, over a Montana valley in
// November. 2026 is used for the sky's geometry only; the date is what matters.
// Returns daylight, 0 at night to 1 with the sun well up.
float setSky(strata::Engine& engine, const World& w) {
    const double midnightUtc = 1794380400.0;  // 2026-11-11 00:00 at UTC-7
    const StrataSolarPosition sun = strata::solarPosition(midnightUtc + gameMinutes(w) * 60.0, 46.6, -112.0);
    engine.setSunDirection(sun.to_sun);
    return clampf((sun.elevation_degrees + 4.0f) / 16.0f, 0.0f, 1.0f);
}

std::string clockText(const World& w) {
    const int day = dayNumber(w);
    const int minute = static_cast<int>(timeOfDay(w) * 1440.0);
    char buf[64];
    std::snprintf(buf, sizeof buf, "%d November  %d:%02d %s", 11 + day, (minute / 60 + 11) % 12 + 1, minute % 60,
                  minute < 720 ? "am" : "pm");
    return buf;
}

StrataCamera playerCamera(const Player& p, float aspect) {
    const Vec3 d = lookDirection(p.yaw, p.pitch);
    const float eye[3] = {p.x, L::kEyeHeight, p.z};
    const float target[3] = {p.x + d.x, L::kEyeHeight + d.y, p.z + d.z};
    const float up[3] = {0, 1, 0};
    return strata::cameraLookAt(eye, target, up, kFovY, aspect, 0.05f, 400.0f);
}

namespace {
std::string percent(float f) { return std::to_string(static_cast<int>(f * 100.0f + 0.5f)) + "%"; }
}  // namespace

std::vector<HudRow> checklist(const World& w) {
    const Progress p = progress(w);
    std::vector<HudRow> rows;
    rows.push_back({HudRow::Step, false, std::string("STEP: ") + stepName(p.step)});
    auto row = [&](bool done, const std::string& label) { rows.push_back({HudRow::Item, done, label}); };
    if (p.step == Step::Survey) {
        row((w.surveyed & kSurveyGrime) != 0, "Grime");
        row((w.surveyed & kSurveyDebris) != 0, "Debris");
        row((w.surveyed & kSurveyRottenPlank) != 0, "Rotten floorboards");
        row((w.surveyed & kSurveyRoofHole) != 0, "Hole in the roof");
        row((w.surveyed & kSurveyBoardedWindow) != 0, "Boarded window");
    } else if (p.step == Step::Clear) {
        row(p.debrisLeft == 0, "Haul out the debris (" + std::to_string(p.debrisLeft) + " left)");
        row(p.snow <= 0, "Shovel out the snow");
        row(p.floorClean >= kFloorCleanGoal, "Sweep the floor (" + percent(p.floorClean) + ")");
        row(p.wallsClean >= kWallCleanGoal, "Scrub the walls (" + percent(p.wallsClean) + ")");
        row(p.hearthClean >= kWallCleanGoal, "Scrub the chimney (" + percent(p.hearthClean) + ")");
    } else if (p.step == Step::Kindle) {
        row(p.woodStacked, "Split firewood and stack it by the hearth (" + std::to_string(w.wood.woodpile) + " of " +
                               std::to_string(kWoodpileGoal) + ")");
        row(p.ashOut, "Shovel out the old ash");
        row(p.kindlingLaid, "Lay kindling in the hearth");
        row(p.logsLaid, "Lay a split log on the andirons");
        row(p.burning, "Strike a match and hold it till it catches");
    } else if (p.step == Step::Repair) {
        row(p.planksLeft == 0, "Replace the rotten floorboards (" + std::to_string(p.planksLeft) + " left)");
        row(p.roofLeft == 0, "Patch the roof (" + std::to_string(p.roofLeft) + " sections open)");
        row(p.boardsLeft == 0, "Take the boards off the window");
        row(p.chairMended, "Mend the rocking chair");
        row(p.tableUpright, "Set the table right");
        row(p.debrisLeft == 0, "Haul out the old wood (" + std::to_string(p.debrisLeft) + " pieces)");
    } else if (p.step == Step::Furnish || p.step >= Step::Welcome) {
        if (p.step == Step::Furnish) {
            row(p.furnished >= kFurnishGoal, "Bring in furnishings (" + std::to_string(p.furnished) + " of " +
                                                 std::to_string(kFurnishGoal) + ")");
        } else {
            row(w.settler.state != SettlerState::Away, "Ada arrives");
            row(w.settler.state == SettlerState::Seated, "She settles by the fire");
            if (p.step == Step::Done) {
                rows.push_back({HudRow::Note, false, "T plays back your restoration."});
            }
        }
        const Brief b = evaluateBrief(w);
        rows.push_back({HudRow::Brief, false, b.met() ? "ADA'S BRIEF: ALL MET" : "ADA'S BRIEF"});
        for (const BriefPoint& bp : b.points) row(bp.met, bp.wish);
    }
    return rows;
}

AimText aimText(const World& w, const Aim& aim) {
    AimText out;
    const Description d = describe(w, aim);
    if (d.name.empty() || aim.distance > L::kReach * 2.5f) return out;
    out.name = d.name;
    out.hint = d.hint;
    out.inReach = aim.distance <= L::kReach;
    return out;
}

std::string carryLine(const World& w) {
    if (w.carry == Carry::Debris) return "Carrying: E to put down (outside to haul out)";
    if (w.carry == Carry::Firewood)
        return std::string("Carrying ") + woodName(static_cast<Wood>(w.carryIndex)) + ": E to put it where it goes";
    if (w.carry == Carry::Item) {
        const Item& it = w.items[static_cast<size_t>(w.carryIndex)];
        return std::string("Carrying ") + itemName(it.kind) +
               (isMaterial(it.kind) ? ": hold the left button where it goes" : ": E to put down, R to turn");
    }
    return {};
}

namespace {

const float kWhite[4] = {0.92f, 0.88f, 0.80f, 1.0f};
const float kDim[4] = {0.62f, 0.60f, 0.56f, 1.0f};
const float kEmber[4] = {0.95f, 0.45f, 0.14f, 1.0f};

// Strata's 8x8 font draws each glyph in a square cell `size` pixels wide.
float centred(float cx, float size, const std::string& s) { return cx - 0.5f * size * static_cast<float>(s.size()); }

void text(std::vector<StrataText>& out, float x, float y, float size, const float color[4], const std::string& s,
          std::vector<std::string>& storage) {
    storage.push_back(s);
    StrataText t{};
    t.x = x;
    t.y = y;
    t.pixel_size = size;
    t.flags = STRATA_TEXT_SHADOW;
    for (int i = 0; i < 4; ++i) t.color[i] = color[i];
    out.push_back(t);
}

void drawHud(strata::Engine& engine, const World& w, const Aim& aim, int width, int height, Help help,
             const std::vector<Message>& messages, bool compact) {
    std::vector<StrataText> texts;
    std::vector<std::string> storage;
    storage.reserve(48);
    if (compact) return;  // the page shows the words, the crosshair included
    const float cx = width * 0.5f, cy = height * 0.5f;
    text(texts, cx - 6, cy - 8, 16, kWhite, "+", storage);

    const AimText at = aimText(w, aim);
    if (!at.name.empty()) {
        text(texts, centred(cx, 18, at.name), cy + 28, 18, at.inReach ? kWhite : kDim, at.name, storage);
        if (!at.hint.empty())
            text(texts, centred(cx, 14, at.hint), cy + 52, 14, kDim, at.hint, storage);
    }

    // The checklist for the step the player is on.
    float y = 20;
    for (const HudRow& r : checklist(w)) {
        switch (r.kind) {
            case HudRow::Step:
                text(texts, 20, y, 18, kEmber, r.text, storage);
                y += 30;
                break;
            case HudRow::Item:
                text(texts, 20, y, 16, r.done ? kWhite : kDim, std::string(r.done ? "[x] " : "[ ] ") + r.text, storage);
                y += 24;
                break;
            case HudRow::Note:
                text(texts, 20, y, 16, kDim, r.text, storage);
                y += 24;
                break;
            case HudRow::Brief:
                y += 8;
                text(texts, 20, y, 16, kEmber, r.text, storage);
                y += 26;
                break;
        }
    }

    const std::string clock = clockText(w);
    text(texts, static_cast<float>(width) - 16.0f * static_cast<float>(clock.size()) - 20, 20, 16, kDim, clock, storage);

    // The tool belt, and what is in hand.
    std::string belt;
    for (int t = 1; t < kToolCount; ++t) {
        const bool held = static_cast<int>(w.tool) == t;
        belt += (held ? "[" : " ") + std::to_string(t) + " " + toolName(static_cast<Tool>(t)) + (held ? "]" : " ") + " ";
    }
    const float beltSize = std::min(12.0f, static_cast<float>(width - 40) / static_cast<float>(belt.size()));
    text(texts, centred(cx, beltSize, belt), static_cast<float>(height) - 36, beltSize, kWhite, belt, storage);
    const std::string carrying = carryLine(w);
    if (!carrying.empty())
        text(texts, centred(cx, 14, carrying), static_cast<float>(height) - 64, 14, kEmber, carrying, storage);

    const std::string speech = settlerSpeech(w);
    if (!speech.empty()) {
        const std::string line = "Ada: \"" + speech + "\"";
        text(texts, cx - 4.5f * static_cast<float>(line.size()), static_cast<float>(height) - 110, 18, kWhite, line, storage);
    }

    float my = cy + 90;
    for (const Message& m : messages) {
        text(texts, centred(cx, 14, m.text), my, 14, kEmber, m.text, storage);
        my += 22;
    }

    if (help != Help::Hidden) {
        std::vector<const char*> lines = {"Mouse      look",       "W A S D    walk",          "Shift      hurry",
                                          "1-9        take a tool", "Left mouse work with it", "E          pick up / put down",
                                          "R          turn what you carry", "L          lantern",
                                          "F5 / F9    save / load the last save"};
        if (help == Help::Browser) {
            lines.push_back("Esc        free the mouse");
            lines.push_back("F1  hide help");
        } else {
            lines.push_back("Esc        free the mouse / quit");
            lines.push_back("F1  hide help   F12  screenshot");
        }
        float hy = static_cast<float>(height) - 24.0f * static_cast<float>(lines.size()) - 80;
        for (const char* l : lines) {
            text(texts, 20, hy, 16, kDim, l, storage);
            hy += 24;
        }
    }
    for (size_t i = 0; i < texts.size(); ++i) texts[i].utf8 = storage[i].c_str();
    engine.submitText(texts.data(), texts.size());
}

// While carrying a furnishing: an outline where it would go, if it can go there.
void drawPlacement(strata::Engine& engine, const World& w, const Aim& aim) {
    if (w.carry != Carry::Item) return;
    const Item& it = w.items[static_cast<size_t>(w.carryIndex)];
    if (isMaterial(it.kind)) return;
    Pose pose;
    if (!placementFor(w, it.kind, aim, it.yaw, pose)) return;
    float hx, hz;
    bridge::itemFootprint(it.kind, hx, hz);
    const float c = std::cos(pose.yaw), s = std::sin(pose.yaw);
    const bool upright = anchorOf(it.kind) == Anchor::Wall || anchorOf(it.kind) == Anchor::Chair;
    Vec3 corners[4];
    const float sx[4] = {-1, 1, 1, -1}, sz[4] = {-1, -1, 1, 1};
    for (int k = 0; k < 4; ++k) {
        const float lx = sx[k] * hx, lz = sz[k] * hz;
        corners[k] = upright ? Vec3{pose.pos.x + c * lx, pose.pos.y + lz, pose.pos.z - s * lx}
                             : Vec3{pose.pos.x + c * lx + s * lz, pose.pos.y + 0.02f, pose.pos.z - s * lx + c * lz};
    }
    StrataLineVertex lines[8];
    for (int k = 0; k < 4; ++k) {
        const Vec3 a = corners[k], b = corners[(k + 1) % 4];
        lines[2 * k] = {{a.x, a.y, a.z}, {0.95f, 0.75f, 0.35f, 0.9f}};
        lines[2 * k + 1] = {{b.x, b.y, b.z}, {0.95f, 0.75f, 0.35f, 0.9f}};
    }
    engine.submitLines(lines, 8);
}

}  // namespace

void render(strata::Engine& engine, bridge::Scene& scene, const World& w, const Aim& aim, Help help,
            const std::vector<Message>& messages, double seconds, bool compact) {
    scene.sync(w);
    scene.animate(w, seconds);
    std::vector<StrataPointLight> lights;
    scene.lights(w, seconds, setSky(engine, w), lights);
    engine.setPointLights(lights.data(), lights.size());
    int32_t width = 0, height = 0;
    engine.drawableSize(width, height);
    const float aspect = height > 0 ? static_cast<float>(width) / static_cast<float>(height) : 1.0f;
    engine.setCamera(playerCamera(w.player, aspect));
    StrataFrameDesc fd{};
    fd.clear_flags = STRATA_CLEAR_COLOR | STRATA_CLEAR_DEPTH | STRATA_CLEAR_SKY;
    engine.beginFrame(fd);
    drawHud(engine, w, aim, width, height, help, messages, compact);
    drawPlacement(engine, w, aim);
    engine.endFrame();
}

Aim aimFor(const bridge::Scene& scene, const World& w) {
    const Vec3 eye{w.player.x, L::kEyeHeight, w.player.z};
    return scene.pick(w, eye, lookDirection(w.player.yaw, w.player.pitch), 12.0f);
}

// ---- the reveal: the restoration played back from its keyframes

constexpr double kRevealSeconds = 32.0;

void Reveal::start(double now) {
    active_ = true;
    started_ = now;
    index_ = static_cast<size_t>(-1);
}

bool Reveal::frame(strata::Engine& engine, bridge::Scene& scene, const World& w, double now) {
    if (!active_ || w.history.empty()) return false;
    const double t = (now - started_) / kRevealSeconds;
    if (t < 0) return false;  // not started yet
    if (t >= 1.0) {
        finish(scene, w);
        return false;
    }
    const size_t n = w.history.size();
    const size_t i = std::min(n - 1, static_cast<size_t>(t * static_cast<double>(n)));
    if (i != index_) {
        index_ = i;
        ghost_ = fromKeyframe(w, w.history[i]);
        scene.showPlayer(true);
        scene.sync(ghost_);
        scene.reloadGrime(ghost_);
    }
    int32_t width = 0, height = 0;
    engine.drawableSize(width, height);
    const float aspect = height > 0 ? static_cast<float>(width) / static_cast<float>(height) : 1.0f;
    // A slow drift across the room from the door's corner toward the hearth.
    const float k = static_cast<float>(t);
    const float eye[3] = {3.1f - 0.6f * k, 2.1f - 0.25f * k, 3.7f - 0.5f * k};
    const float target[3] = {-1.0f + 0.3f * k, 0.8f, -1.9f};
    const float up[3] = {0, 1, 0};
    engine.setCamera(strata::cameraLookAt(eye, target, up, 62.0f, aspect, 0.05f, 400.0f));
    scene.animate(ghost_, now);
    std::vector<StrataPointLight> lights;
    scene.lights(ghost_, now, setSky(engine, ghost_), lights);
    engine.setPointLights(lights.data(), lights.size());
    StrataFrameDesc fd{};
    fd.clear_flags = STRATA_CLEAR_COLOR | STRATA_CLEAR_DEPTH | STRATA_CLEAR_SKY;
    engine.beginFrame(fd);
    std::vector<StrataText> texts;
    std::vector<std::string> storage;
    storage.reserve(4);
    text(texts, 30, 30, 22, kEmber, "Ada's cabin, from ruin to home", storage);
    text(texts, 30, 62, 16, kWhite,
         clockText(ghost_) + "   " + stepName(static_cast<Step>(w.history[index_].step)), storage);
    text(texts, 30, static_cast<float>(height) - 40, 14, kDim, "Esc to skip", storage);
    for (size_t j = 0; j < texts.size(); ++j) texts[j].utf8 = storage[j].c_str();
    engine.submitText(texts.data(), texts.size());
    engine.endFrame();
    return true;
}

void Reveal::finish(bridge::Scene& scene, const World& w) {
    active_ = false;
    scene.showPlayer(false);
    scene.sync(w);
    scene.reloadGrime(w);
}

void writeReplayFile(const World& w, const std::string& path) {
    if (path.empty()) return;
    std::string error;
    if (writeReplay(w, path, &error)) std::printf("kindling: wrote the restoration replay to %s\n", path.c_str());
    else std::fprintf(stderr, "kindling: could not write %s: %s\n", path.c_str(), error.c_str());
}

}  // namespace kindling::play
