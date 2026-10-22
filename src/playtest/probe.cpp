#include "playtest/probe.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <system_error>
#include <sstream>

#include "sim/layout.h"
#include "sim/progress.h"

#ifndef KINDLING_VERSION
#define KINDLING_VERSION "0.0.0"
#endif

namespace kindling::playtest {

namespace L = layout;

namespace {

constexpr double kPiD = 3.14159265358979323846;

std::string number(double v, int decimals = 2) {
    if (!std::isfinite(v)) return "0";
    char buf[48];
    std::snprintf(buf, sizeof buf, "%.*f", decimals, v);
    // trim trailing zeros (and a trailing point) so 2.50 reads 2.5 and 3.00 reads 3
    std::string s = buf;
    if (s.find('.') != std::string::npos) {
        while (!s.empty() && s.back() == '0') s.pop_back();
        if (!s.empty() && s.back() == '.') s.pop_back();
    }
    if (s == "-0") s = "0";
    return s;
}

const char* boolean(bool b) { return b ? "true" : "false"; }

// Distance from (x, z) to the nearest point of a box on the ground plane.
float distanceToBox(float x, float z, const Box2& b) {
    const float dx = std::max({b.x0 - x, 0.0f, x - b.x1});
    const float dz = std::max({b.z0 - z, 0.0f, z - b.z1});
    return std::sqrt(dx * dx + dz * dz);
}

Target pointTarget(const Player& p, const char* kind, const char* name, const std::string& id, float x, float z) {
    Target t;
    t.kind = kind;
    t.name = name;
    t.id = id;
    const float dx = x - p.x, dz = z - p.z;
    t.distance = std::sqrt(dx * dx + dz * dz);
    t.bearing = bearingTo(p, x, z);
    return t;
}

Target boxTarget(const Player& p, const char* kind, const char* name, const Box2& b) {
    Target t = pointTarget(p, kind, name, kind, (b.x0 + b.x1) / 2, (b.z0 + b.z1) / 2);
    t.distance = distanceToBox(p.x, p.z, b);
    return t;
}

const char* debrisName(DebrisKind k) {
    switch (k) {
        case DebrisKind::Board: return "Broken board";
        case DebrisKind::Shingle: return "Fallen shingle";
        case DebrisKind::Rafter: return "Fallen rafter";
        case DebrisKind::RafterPiece: return "Piece of rafter";
    }
    return "Debris";
}

const char* carryName(Carry c) {
    switch (c) {
        case Carry::Nothing: return "nothing";
        case Carry::Debris: return "debris";
        case Carry::Item: return "item";
        case Carry::Firewood: return "firewood";
        case Carry::Hay: return "hay";
    }
    return "nothing";
}

const char* settlerName(SettlerState s) {
    switch (s) {
        case SettlerState::Away: return "away";
        case SettlerState::Arriving: return "arriving";
        case SettlerState::LookingAround: return "looking-around";
        case SettlerState::ToChair: return "to-chair";
        case SettlerState::Seated: return "seated";
    }
    return "away";
}

}  // namespace

const std::vector<std::string>& targetKinds() {
    static const std::vector<std::string> kinds{"door", "wagon", "hearth", "window", "chopping_block", "mare", "debris"};
    return kinds;
}

float headingDegrees(float yaw) {
    double d = std::fmod(static_cast<double>(yaw) * 180.0 / kPiD, 360.0);
    if (d < 0) d += 360.0;
    if (d >= 360.0) d -= 360.0;
    return static_cast<float>(d);
}

float bearingTo(const Player& p, float x, float z) {
    // Forward is (sin yaw, -cos yaw) and right is (cos yaw, sin yaw) (sim/rules.cpp).
    const double dx = x - p.x, dz = z - p.z;
    const double fwd = dx * std::sin(p.yaw) - dz * std::cos(p.yaw);
    const double right = dx * std::cos(p.yaw) + dz * std::sin(p.yaw);
    if (fwd == 0 && right == 0) return 0;
    return static_cast<float>(std::atan2(right, fwd) * 180.0 / kPiD);
}

std::vector<Target> nearest(const World& w, int perKind) {
    const Player& p = w.player;
    std::vector<Target> all;
    // The door is a gap in the front wall; aim for its middle, in the wall.
    all.push_back(pointTarget(p, "door", "Cabin door", "door", (L::kDoorX0 + L::kDoorX1) / 2,
                              L::kInnerZ1 + L::kWallThickness / 2));
    all.push_back(boxTarget(p, "wagon", "Wagon", {1.2f, 7.0f, 3.8f, 9.4f}));
    all.push_back(pointTarget(p, "hearth", "Hearth", "hearth", (L::kFireboxX0 + L::kFireboxX1) / 2, L::kBreastFront));
    all.push_back(pointTarget(p, "window", "Window", "window", L::kInnerX0, (L::kWindowZ0 + L::kWindowZ1) / 2));
    all.push_back(boxTarget(p, "chopping_block", "Chopping block", {-2.15f, 6.65f, -1.45f, 7.35f}));
    all.push_back(pointTarget(p, "mare", "Mare", "mare", L::kHorse.x, L::kHorse.z));
    for (size_t i = 0; i < w.debris.size(); ++i) {
        const Debris& d = w.debris[i];
        if (d.hauled) continue;
        if (w.carry == Carry::Debris && w.carryIndex == static_cast<int32_t>(i)) continue;
        all.push_back(pointTarget(p, "debris", debrisName(d.kind), "debris-" + std::to_string(d.id), d.pos.x, d.pos.z));
    }
    std::stable_sort(all.begin(), all.end(), [](const Target& a, const Target& b) { return a.distance < b.distance; });
    std::vector<Target> out;
    for (const std::string& kind : targetKinds()) {
        int n = 0;
        for (const Target& t : all)
            if (t.kind == kind && n < perKind) {
                out.push_back(t);
                ++n;
            }
    }
    return out;
}

const char* gameVersion() { return KINDLING_VERSION; }

std::string stepTitle(int32_t step) { return stepName(static_cast<Step>(step)); }

std::string quote(const std::string& text) {
    std::string out = "\"";
    for (unsigned char c : text) {
        switch (c) {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:
                if (c < 0x20) {
                    char buf[8];
                    std::snprintf(buf, sizeof buf, "\\u%04x", c);
                    out += buf;
                } else {
                    out += static_cast<char>(c);
                }
        }
    }
    return out + "\"";
}

std::string stateJson(const World& w, const HostView& v, int64_t wallTimeMs, double ticksPerSecond) {
    const Progress pr = progress(w);
    std::ostringstream s;
    s << "{\"wallTimeMs\":" << wallTimeMs << ",\"version\":" << quote(gameVersion()) << ",\"screen\":" << quote(v.screen)
      << ",\"fps\":" << v.fps << ",\"world\":true,\"tps\":" << number(ticksPerSecond, 1) << ",\"openPanels\":[";
    bool first = true;
    if (v.help) {
        s << "\"help\"";
        first = false;
    }
    if (!v.mouseCaptured) s << (first ? "" : ",") << "\"mouse-free\"";
    s << "],\"seed\":" << w.seed << ",\"tick\":" << w.tick << ",\"day\":" << dayNumber(w)
      << ",\"timeOfDay\":" << number(timeOfDay(w), 3) << ",\"clock\":" << quote(v.clock);
    s << ",\"player\":{\"x\":" << number(w.player.x) << ",\"z\":" << number(w.player.z)
      << ",\"yawDeg\":" << number(headingDegrees(w.player.yaw), 1)
      << ",\"pitchDeg\":" << number(w.player.pitch * 180.0 / kPiD, 1) << ",\"speed\":" << number(L::kWalkSpeed)
      << ",\"indoors\":" << boolean(L::kInterior.contains(w.player.x, w.player.z)) << ",\"tool\":" << quote(toolName(w.tool))
      << ",\"carrying\":" << quote(carryName(w.carry)) << ",\"lantern\":" << boolean(w.lantern) << "}";
    s << ",\"objective\":{\"number\":" << static_cast<int>(pr.step) + 1 << ",\"title\":" << quote(stepName(pr.step))
      << ",\"completed\":" << static_cast<int>(pr.step) << "}";
    s << ",\"step\":" << quote(stepName(pr.step));
    s << ",\"progress\":{\"floorClean\":" << number(pr.floorClean) << ",\"wallsClean\":" << number(pr.wallsClean)
      << ",\"hearthClean\":" << number(pr.hearthClean) << ",\"debrisLeft\":" << pr.debrisLeft
      << ",\"snow\":" << number(pr.snow) << ",\"surveyed\":" << boolean(pr.surveyed)
      << ",\"planksLeft\":" << pr.planksLeft << ",\"roofLeft\":" << pr.roofLeft << ",\"boardsLeft\":" << pr.boardsLeft
      << ",\"chairMended\":" << boolean(pr.chairMended) << ",\"tableUpright\":" << boolean(pr.tableUpright)
      << ",\"furnished\":" << pr.furnished << ",\"ashOut\":" << boolean(pr.ashOut)
      << ",\"kindlingLaid\":" << boolean(pr.kindlingLaid) << ",\"logsLaid\":" << boolean(pr.logsLaid)
      << ",\"woodStacked\":" << boolean(pr.woodStacked) << ",\"burning\":" << boolean(pr.burning) << "}";
    s << ",\"checklist\":[";
    for (size_t i = 0; i < v.checklist.size(); ++i)
        s << (i ? "," : "") << "{\"done\":" << boolean(v.checklist[i].first) << ",\"text\":" << quote(v.checklist[i].second)
          << "}";
    s << "]";
    s << ",\"aim\":";
    if (v.aim.empty()) s << "null";
    else
        s << "{\"name\":" << quote(v.aim) << ",\"hint\":" << quote(v.aimHint) << ",\"inReach\":" << boolean(v.aimInReach)
          << "}";
    s << ",\"status\":" << (v.carrying.empty() ? std::string("null") : quote(v.carrying));
    s << ",\"settler\":" << quote(settlerName(w.settler.state)) << ",\"hearthLit\":" << boolean(w.hearth.lit)
      << ",\"mare\":{\"hunger\":" << number(w.horse.hunger) << ",\"dirt\":" << number(w.horse.dirt) << "}";
    s << ",\"nearest\":{";
    const std::vector<Target> near = nearest(w);
    bool firstKind = true;
    for (const std::string& kind : targetKinds()) {
        s << (firstKind ? "" : ",") << quote(kind) << ":[";
        firstKind = false;
        bool firstTarget = true;
        for (const Target& t : near) {
            if (t.kind != kind) continue;
            s << (firstTarget ? "" : ",") << "{\"name\":" << quote(t.name) << ",\"id\":" << quote(t.id)
              << ",\"distance\":" << number(t.distance) << ",\"bearing\":" << number(t.bearing, 1) << "}";
            firstTarget = false;
        }
        s << "]";
    }
    s << "}}";
    return s.str();
}

std::string eventJson(const std::string& type, int64_t tick, int64_t wallTimeMs,
                      const std::vector<std::pair<std::string, std::string>>& rawFields) {
    std::ostringstream s;
    s << "{\"type\":" << quote(type) << ",\"tick\":" << tick << ",\"wallTimeMs\":" << wallTimeMs;
    for (const auto& f : rawFields) s << "," << quote(f.first) << ":" << f.second;
    s << "}";
    return s.str();
}

Recorder::Recorder(std::string directory, std::function<int64_t()> clockMs, int stateIntervalMs)
    : dir_(std::move(directory)), clockMs_(std::move(clockMs)), intervalMs_(stateIntervalMs) {
    std::error_code ignored;
    std::filesystem::create_directories(dir_, ignored);
}

void Recorder::failed(const std::string& what) {
    if (reportedFailure_) return;
    reportedFailure_ = true;
    std::fprintf(stderr, "kindling: --playtest-out: could not write %s\n", what.c_str());
}

void Recorder::append(const std::string& type, int64_t tick, const std::vector<std::pair<std::string, std::string>>& fields) {
    std::ofstream f(dir_ + "/events.jsonl", std::ios::app | std::ios::binary);
    if (!f) return failed(dir_ + "/events.jsonl");
    f << eventJson(type, tick, clockMs_(), fields) << "\n";
}

void Recorder::writeState(const std::string& json) {
    const std::string path = dir_ + "/state.json", temporary = path + ".tmp";
    {
        std::ofstream f(temporary, std::ios::trunc | std::ios::binary);
        if (!f) return failed(temporary);
        f << json << "\n";
    }
#ifdef _WIN32
    std::remove(path.c_str());  // rename does not replace on Windows
#endif
    if (std::rename(temporary.c_str(), path.c_str()) != 0) failed(path);
}

void Recorder::worldStarted(const World& w, const char* how) {
    append("world", w.tick, {{"how", quote(how)}, {"seed", std::to_string(w.seed)}});
    step_ = static_cast<int32_t>(progress(w).step);
    stepTick_ = w.tick;
    lastStateMs_ = -1;
}

void Recorder::update(const World& w, const HostView& view, const std::vector<std::string>& messages, int savesWritten) {
    const int64_t now = clockMs_();
    if (!primed_) {
        primed_ = true;
        saves_ = savesWritten;
    }
    if (view.screen != screen_) {
        append("screen", w.tick, {{"from", screen_.empty() ? "null" : quote(screen_)}, {"to", quote(view.screen)}});
        screen_ = view.screen;
    }
    if (savesWritten != saves_) {
        append("save", w.tick, {{"count", std::to_string(savesWritten)}});
        saves_ = savesWritten;
    }
    for (const std::string& m : messages) append("status", w.tick, {{"text", quote(m)}});
    const int32_t step = static_cast<int32_t>(progress(w).step);
    while (step_ < step) {
        append("objective_completed", w.tick,
               {{"number", std::to_string(step_ + 1)},
                {"id", quote(stepTitle(step_))},
                {"title", quote(stepTitle(step_))},
                {"ticksTaken", std::to_string(w.tick - stepTick_)}});
        ++step_;
        stepTick_ = w.tick;
    }
    if (step < step_) step_ = step;  // a load went back to an earlier step
    if (now >= nextStateMs_) {
        nextStateMs_ = now + intervalMs_;
        if (lastStateMs_ >= 0 && now > lastStateMs_ && w.tick >= lastStateTick_)
            tps_ = static_cast<double>(w.tick - lastStateTick_) * 1000.0 / static_cast<double>(now - lastStateMs_);
        lastStateMs_ = now;
        lastStateTick_ = w.tick;
        writeState(stateJson(w, view, now, tps_));
    }
}

}  // namespace kindling::playtest
