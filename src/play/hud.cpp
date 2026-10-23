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
#include "sim/rules.h"
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
// How far toward a goal, so the line reads 100% exactly when it ticks.
std::string toward(float value, float goal) {
    return std::to_string(static_cast<int>(std::min(1.0f, value / goal) * 100.0f)) + "%";
}
std::string count(int n, const char* one, const char* many) { return std::to_string(n) + " " + (n == 1 ? one : many); }
}  // namespace

std::vector<HudRow> checklist(const World& w) {
    const Progress p = progress(w);
    std::vector<HudRow> rows;
    rows.push_back({HudRow::Step, false,
                    std::string("STEP: ") + stepName(p.step) + (p.step == Step::Survey ? ": look at each of these" : "")});
    auto row = [&](bool done, const std::string& label) { rows.push_back({HudRow::Item, done, label}); };
    if (p.step == Step::Survey) {
        row((w.surveyed & kSurveyGrime) != 0, "Grime");
        row((w.surveyed & kSurveyDebris) != 0, "Debris");
        row((w.surveyed & kSurveyRottenPlank) != 0, "Rotten floorboards");
        row((w.surveyed & kSurveyRoofHole) != 0, "Hole in the roof (stand under it, look up)");
        row((w.surveyed & kSurveyBoardedWindow) != 0, "Boarded window");
    } else if (p.step == Step::Clear) {
        row(p.debrisLeft == 0, "Haul out the debris (" + std::to_string(p.debrisLeft) + " left)");
        row(p.snow <= 0, p.snow <= 0 ? "Shovel out the snow" : "Shovel out the snow (" + percent(1.0f - p.snow) + ")");
        row(p.floorClean >= kFloorCleanGoal, "Sweep the floor (" + toward(p.floorClean, kFloorCleanGoal) + ")");
        row(p.wallsClean >= kWallCleanGoal, "Scrub the walls (" + toward(p.wallsClean, kWallCleanGoal) + ")");
        row(p.hearthClean >= kWallCleanGoal, "Scrub the chimney (" + toward(p.hearthClean, kWallCleanGoal) + ")");
    } else if (p.step == Step::Kindle) {
        row(p.woodStacked, "Split firewood and stack it by the hearth (" + std::to_string(p.woodCount) + " of " +
                               std::to_string(kWoodpileGoal) + ")");
        row(p.ashOut, "Shovel out the old ash");
        row(p.kindlingLaid, "Lay kindling in the hearth");
        row(p.logsLaid, "Lay a split log on the andirons");
        row(p.burning, w.hearth.lit && !p.burning
                           ? "Let the fire take hold (" + toward(w.hearth.fire, kBurningFire) + ")"
                           : std::string("Strike a match and hold it till it catches"));
    } else if (p.step == Step::Repair) {
        row(p.planksLeft == 0, "Replace the rotten floorboards (" + std::to_string(p.planksLeft) + " left)");
        row(p.roofLeft == 0, "Patch the roof (" + count(p.roofLeft, "section", "sections") + " open)");
        row(p.boardsLeft == 0, "Take the boards off the window");
        row(p.chairMended, "Mend the rocking chair");
        row(p.tableUpright, "Set the table right");
        row(p.debrisLeft == 0, "Haul out the old wood (" + count(p.debrisLeft, "piece", "pieces") + ")");
    } else if (p.step == Step::Furnish || p.step >= Step::Welcome) {
        if (p.step == Step::Furnish) {
            row(p.furnished >= kFurnishGoal, "Bring in furnishings (" + std::to_string(p.furnished) + " of " +
                                                 std::to_string(kFurnishGoal) + ")");
        } else if (p.step == Step::Done) {
            // Ada is home: the list steps aside so the cabin can be seen.
            rows.push_back({HudRow::Note, false, "Ada is home. T plays back your restoration."});
            return rows;
        } else {
            row(w.settler.state != SettlerState::Away, "Ada arrives");
            row(w.settler.state == SettlerState::Seated, "She settles by the fire");
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
    if (d.name.empty() || aim.distance > L::kNameRange) return out;
    out.name = d.name;
    out.hint = d.hint;
    out.inReach = aim.distance <= L::kReach;
    if (!out.inReach && toolApplies(w, aim)) out.note = "Too far to reach. Step closer.";
    out.meter = workMeter(w, aim);
    return out;
}

std::string meterText(const World& w, const AimText& at) {
    if (at.meter < 0) return {};
    const bool patch = w.tool == Tool::Broom || w.tool == Tool::ScrubBrush;
    if (patch && w.carry == Carry::Nothing && at.meter >= 1) return "Clean here. Move on.";
    const int filled = static_cast<int>(at.meter * 10.0f + 0.5f);
    return "[" + std::string(static_cast<size_t>(filled), '#') + std::string(static_cast<size_t>(10 - filled), '-') +
           "] " + percent(at.meter) + (patch ? " clean" : "");
}

std::string carryLine(const World& w) {
    if (w.carry == Carry::Debris) return "Carrying: E to put down (outside to haul out)";
    if (w.carry == Carry::Firewood)
        return std::string("Carrying ") + woodName(static_cast<Wood>(w.carryIndex)) + ": E to put it where it goes";
    if (w.carry == Carry::Item) {
        const Item& it = w.items[static_cast<size_t>(w.carryIndex)];
        if (isMaterial(it.kind)) {
            int n = 0;
            for (const Item& o : w.items) n += o.place == ItemPlace::Carried ? 1 : 0;
            const std::string what = it.kind == ItemKind::Plank ? count(n, "new plank", "new planks")
                                                                : count(n, "bundle of shingles", "bundles of shingles");
            return "Carrying " + what + ": hold the left button where it goes";
        }
        return std::string("Carrying ") + itemName(it.kind) + ": E to put down, R to turn";
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

// Words centred on cx within `width` pixels: at `size` if they fit, smaller
// (down to 10 px) if not, and wrapped at spaces onto more lines if even that
// is too wide. Returns the height used.
float block(std::vector<StrataText>& out, float cx, float y, float size, const float color[4], const std::string& s,
            float width, std::vector<std::string>& storage) {
    const float n = static_cast<float>(s.size());
    if (size * n > width) size = std::max(10.0f, std::floor(width / n));
    const size_t perLine = std::max<size_t>(8, static_cast<size_t>(width / size));
    std::vector<std::string> lines;
    std::string rest = s;
    while (rest.size() > perLine) {
        size_t cut = rest.rfind(' ', perLine);
        if (cut == std::string::npos || cut == 0) cut = perLine;
        lines.push_back(rest.substr(0, cut));
        rest = rest.substr(cut < rest.size() && rest[cut] == ' ' ? cut + 1 : cut);
    }
    lines.push_back(rest);
    const float step = std::round(size * 1.4f);
    for (const std::string& l : lines) {
        text(out, centred(cx, size, l), y, size, color, l, storage);
        y += step;
    }
    return step * static_cast<float>(lines.size());
}

void drawHud(strata::Engine& engine, const World& w, const Aim& aim, int width, int height, Help help,
             const std::vector<Message>& messages, bool compact) {
    std::vector<StrataText> texts;
    std::vector<std::string> storage;
    storage.reserve(48);
    if (compact) return;  // the page shows the words, the crosshair included
    const float cx = width * 0.5f, cy = height * 0.5f;
    // Sizes are for a 720-pixel-high window and scale with it, so a small
    // window keeps the middle of the view clear.
    const float u = std::clamp(static_cast<float>(height) / 720.0f, 0.6f, 2.0f);
    auto px = [u](float v) { return std::round(v * u); };
    text(texts, cx - px(6), cy - px(8), px(16), kWhite, "+", storage);

    const float wide = static_cast<float>(width) - 40;  // what centred words may span
    const AimText at = aimText(w, aim);
    float ay = cy + px(28);
    if (!at.name.empty()) {
        ay += block(texts, cx, ay, px(18), at.inReach ? kWhite : kDim, at.name, wide, storage) + px(4);
        if (!at.hint.empty()) ay += block(texts, cx, ay, px(14), kDim, at.hint, wide, storage);
        if (!at.note.empty()) ay += block(texts, cx, ay, px(14), kEmber, at.note, wide, storage);
    }
    const std::string meter = meterText(w, at);
    if (!meter.empty()) ay += block(texts, cx, ay + px(4), px(14), at.meter >= 1 ? kEmber : kWhite, meter, wide, storage) + px(4);

    // The checklist for the step the player is on.
    const float left = px(20);
    float y = px(20);
    for (const HudRow& r : checklist(w)) {
        switch (r.kind) {
            case HudRow::Step:
                text(texts, left, y, px(18), kEmber, r.text, storage);
                y += px(30);
                break;
            case HudRow::Item: {
                const std::string line = std::string(r.done ? "[x] " : "[ ] ") + r.text;
                const float size =
                    std::max(8.0f, std::min(px(16), std::floor((wide * 0.6f) / static_cast<float>(line.size()))));
                text(texts, left, y, size, r.done ? kWhite : kDim, line, storage);
                y += size + px(8);
                break;
            }
            case HudRow::Note:
                text(texts, left, y, px(16), kDim, r.text, storage);
                y += px(24);
                break;
            case HudRow::Brief:
                y += px(8);
                text(texts, left, y, px(16), kEmber, r.text, storage);
                y += px(26);
                break;
        }
    }

    const std::string clock = clockText(w);
    text(texts, static_cast<float>(width) - px(16) * static_cast<float>(clock.size()) - left, px(20), px(16), kDim, clock,
         storage);

    // The tool belt, and what is in hand.
    std::string belt;
    for (int t = 1; t < kToolCount; ++t) {
        const bool held = static_cast<int>(w.tool) == t;
        belt += (held ? "[" : " ") + std::to_string(t) + " " + toolName(static_cast<Tool>(t)) + (held ? "]" : " ") + " ";
    }
    const float beltSize = std::min(px(12), static_cast<float>(width - 40) / static_cast<float>(belt.size()));
    text(texts, centred(cx, beltSize, belt), static_cast<float>(height) - px(36), beltSize, kWhite, belt, storage);
    const std::string carrying = carryLine(w);
    if (!carrying.empty()) block(texts, cx, static_cast<float>(height) - px(64), px(14), kEmber, carrying, wide, storage);

    // A strip of keys above the tool belt, clear of the checklist and the
    // crosshair's words; each line shrinks to the window's width.
    float helpTop = static_cast<float>(height) - px(76);
    if (help != Help::Hidden) {
        std::vector<std::string> lines = {"Mouse / arrows look   W A S D walk   Shift hurry",
                                          "1-9 take a tool   Left mouse work with it",
                                          "E pick up / put down   R turn it   L lantern"};
        lines.push_back(help == Help::Browser ? "F5 save   F9 load   Esc free the mouse   F1 hide these"
                                              : "F5 save   F9 load   Esc free the mouse / quit   F1 hide these   F12 screenshot");
        float size = px(14);
        for (const std::string& l : lines) size = std::min(size, std::floor(wide / static_cast<float>(l.size())));
        size = std::max(8.0f, size);
        const float step = std::round(size * 1.5f);
        float hy = static_cast<float>(height) - px(76) - step * static_cast<float>(lines.size());
        helpTop = hy;
        for (const std::string& l : lines) {
            text(texts, centred(cx, size, l), hy, size, kDim, l, storage);
            hy += step;
        }
    }

    // Ada's words sit above the key strip when it is showing.
    const std::string speech = settlerSpeech(w);
    if (!speech.empty())
        block(texts, cx, std::min(static_cast<float>(height) - px(120), helpTop - px(30)), px(18), kWhite,
              "Ada: \"" + speech + "\"", wide, storage);

    float my = std::max(cy + px(90), ay + px(8));
    for (const Message& m : messages) my += block(texts, cx, my, px(14), kEmber, m.text, wide, storage) + px(2);
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
    // The tool swings while it works, and dust (or snow, or ash, or sawdust)
    // rises from what it works on.
    const bool working = w.workKind != TargetKind::None && w.workKind == aim.kind;
    const float meter = workMeter(w, aim);
    scene.animateTool(w, seconds, working && meter >= 0);
    if (working && meter >= 0 && meter < 1 && w.carry == Carry::Nothing) {
        switch (w.tool) {
            case Tool::Broom: scene.puff(aim.point, {0.55f, 0.50f, 0.42f}, seconds); break;
            case Tool::ScrubBrush:
                scene.puff(aim.point, aim.kind == TargetKind::HearthFace ? bridge::Rgb{0.22f, 0.21f, 0.20f}
                                                                        : bridge::Rgb{0.60f, 0.57f, 0.50f},
                           seconds);
                break;
            case Tool::Shovel:
                scene.puff(aim.point, aim.kind == TargetKind::Snow ? bridge::Rgb{0.90f, 0.92f, 0.97f}
                                                                  : bridge::Rgb{0.50f, 0.50f, 0.49f},
                           seconds);
                break;
            case Tool::Saw: scene.puff(aim.point, {0.72f, 0.56f, 0.34f}, seconds); break;
            default: break;
        }
    }
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
    scene.drawDust(seconds);
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
    scene.animateTool(ghost_, now, false);  // put away while the player is seen from outside
    std::vector<StrataPointLight> lights;
    scene.lights(ghost_, now, setSky(engine, ghost_), lights);
    engine.setPointLights(lights.data(), lights.size());
    StrataFrameDesc fd{};
    fd.clear_flags = STRATA_CLEAR_COLOR | STRATA_CLEAR_DEPTH | STRATA_CLEAR_SKY;
    engine.beginFrame(fd);
    std::vector<StrataText> texts;
    std::vector<std::string> storage;
    storage.reserve(4);
    // Scaled with the window, and shrunk to fit a narrow one.
    const float u = std::clamp(static_cast<float>(height) / 720.0f, 0.6f, 2.0f);
    const float margin = std::round(30 * u), room = static_cast<float>(width) - 2 * margin;
    auto fit = [&](float size, const std::string& s) {
        return std::max(8.0f, std::min(std::round(size * u), std::floor(room / static_cast<float>(s.size()))));
    };
    const std::string title = "Ada's cabin, from ruin to home";
    const std::string when = clockText(ghost_) + "   " + stepName(static_cast<Step>(w.history[index_].step));
    const float titleSize = fit(22, title);
    text(texts, margin, margin, titleSize, kEmber, title, storage);
    text(texts, margin, margin + titleSize + std::round(10 * u), fit(16, when), kWhite, when, storage);
    text(texts, margin, static_cast<float>(height) - std::round(40 * u), fit(14, "Esc to skip"), kDim, "Esc to skip",
         storage);
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
