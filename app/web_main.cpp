// Kindling in a browser: the same game on Strata's WebGL 2 backend (Emscripten).
//
// The same rules, scene, HUD and session (src/play) as the desktop game, with
// these differences:
//
//  - Saves are the desktop format (src/save: checksummed, previous save kept),
//    written to /kindling-save, which the page mounts on IndexedDB (IDBFS) and
//    loads before main() runs. Every save is flushed to IndexedDB at once
//    (Module.flushSaves in web/shell.html). The browser's saves stay in that
//    browser: they are never sent anywhere.
//  - The page owns the pointer lock (a click on the canvas takes it, Esc gives
//    it back) and turns mouse movement into kindling_look(); there is no quit
//    and no F12 screenshot. The restoration replay file is not written.
//  - On a touch screen the page draws a stick, a look pad and buttons, which
//    call the kindling_* functions exported below.
//  - On a narrow page the canvas shows no words: the page shows the crosshair,
//    the checklist and the rest as text (kindling_status()).
#include <cinttypes>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <string>

#include <emscripten/emscripten.h>
#include <emscripten/html5.h>

#include "play/hud.h"
#include "play/session.h"
#include "sim/progress.h"
#include "sim/settler.h"
#include "strata/strata.hpp"

using namespace kindling;

namespace {

constexpr const char* kSlot = "/kindling-save/default";

struct Game {
    std::unique_ptr<strata::Engine> engine;
    std::unique_ptr<play::Session> session;
    double last = 0;
    std::string statusJson;
};

Game* g = nullptr;

void frame(void*) {
    StrataEvent e;
    while (g->engine->pollEvent(e)) g->session->handle(e);
    const double now = emscripten_get_now() / 1000.0;
    g->session->frame(now - g->last);
    g->last = now;
    g->engine->present();
}

void jsonString(std::string& out, const std::string& s) {
    out += '"';
    for (char ch : s) {
        if (ch == '"' || ch == '\\') {
            out += '\\';
            out += ch;
        } else if (static_cast<unsigned char>(ch) < 0x20) {
            out += ' ';
        } else {
            out += ch;
        }
    }
    out += '"';
}

}  // namespace

// ---- Called by the page (web/shell.html) ------------------------------------
extern "C" {

EMSCRIPTEN_KEEPALIVE void kindling_look(double dx, double dy) {
    if (g) g->session->look(static_cast<float>(dx), static_cast<float>(dy));
}
EMSCRIPTEN_KEEPALIVE void kindling_stick(double forward, double strafe) {
    if (g) g->session->setStick(static_cast<float>(forward), static_cast<float>(strafe));
}
EMSCRIPTEN_KEEPALIVE void kindling_use(int down) {
    if (g) g->session->setUse(down != 0);
}
// A key press from an on-screen button (an SDL scancode, as the keyboard sends).
EMSCRIPTEN_KEEPALIVE void kindling_press(int scancode) {
    if (g) g->session->press(scancode);
}
EMSCRIPTEN_KEEPALIVE void kindling_captured(int on) {
    if (g) g->session->setCaptured(on != 0);
}
EMSCRIPTEN_KEEPALIVE void kindling_help(int on) {
    if (g) g->session->setHelp(on != 0);
}
EMSCRIPTEN_KEEPALIVE void kindling_skip_reveal() {
    if (g) g->session->skipReveal();
}
// Saves now (the page calls this when it is hidden); 1 on success.
EMSCRIPTEN_KEEPALIVE int kindling_save() { return g && g->session->save("Saved.") ? 1 : 0; }
EMSCRIPTEN_KEEPALIVE void kindling_restart() {
    if (g) g->session->restart();
}
// The canvas changed size: draw at w x h (the page caps the device pixel
// ratio), with the HUD's words on the page instead when `compact`.
EMSCRIPTEN_KEEPALIVE void kindling_resize(double w, double h, int compact) {
    if (!g || w < 1 || h < 1) return;
    g->engine->resize(static_cast<int32_t>(w), static_cast<int32_t>(h));
    g->session->setCompactHud(compact != 0);
}
// The game's state for the page, as JSON.
EMSCRIPTEN_KEEPALIVE const char* kindling_status() {
    if (!g) return "{}";
    const play::Session& s = *g->session;
    const World& w = s.world();
    const Progress p = progress(w);
    std::string o = "{\"step\":";
    jsonString(o, stepName(p.step));
    o += ",\"done\":";
    o += p.step == Step::Done ? "true" : "false";
    o += ",\"tool\":" + std::to_string(static_cast<int>(w.tool));
    o += ",\"tools\":[";
    for (int t = 0; t < kToolCount; ++t) {
        if (t) o += ',';
        jsonString(o, toolName(static_cast<Tool>(t)));
    }
    o += "],\"captured\":";
    o += s.captured() ? "true" : "false";
    o += ",\"help\":";
    o += s.help() ? "true" : "false";
    o += ",\"revealing\":";
    o += s.revealing() ? "true" : "false";
    char buf[96];
    std::snprintf(buf, sizeof buf, ",\"tick\":%" PRId64 ",\"x\":%.3f,\"z\":%.3f,\"yaw\":%.3f", w.tick,
                  static_cast<double>(w.player.x), static_cast<double>(w.player.z),
                  static_cast<double>(w.player.yaw));
    o += buf;
    o += ",\"clock\":";
    jsonString(o, play::clockText(w));
    const play::AimText at = play::aimText(w, s.aim());
    o += ",\"aim\":";
    jsonString(o, at.name);
    o += ",\"hint\":";
    jsonString(o, at.hint);
    o += ",\"inReach\":";
    o += at.inReach ? "true" : "false";
    o += ",\"carry\":";
    jsonString(o, play::carryLine(w));
    o += ",\"speech\":";
    jsonString(o, settlerSpeech(w));
    o += ",\"message\":";
    jsonString(o, s.lastMessage());
    o += ",\"checklist\":[";
    bool first = true;
    for (const play::HudRow& r : play::checklist(w)) {
        if (!first) o += ',';
        first = false;
        const char* kinds[] = {"step", "item", "note", "brief"};
        o += "{\"kind\":\"";
        o += kinds[r.kind];
        o += "\",\"done\":";
        o += r.done ? "true" : "false";
        o += ",\"text\":";
        jsonString(o, r.text);
        o += '}';
    }
    o += "]}";
    g->statusJson = std::move(o);
    return g->statusJson.c_str();
}

}  // extern "C"

int main() {
    static Game game;
    g = &game;

    double cw = 0, ch = 0;
    emscripten_get_element_css_size("#canvas", &cw, &ch);
    if (cw < 1 || ch < 1) {
        cw = 1280;
        ch = 720;
    }
    strata::EngineDesc desc;
    desc.surface = STRATA_SURFACE_WINDOW;
    desc.width = static_cast<int32_t>(cw);
    desc.height = static_cast<int32_t>(ch);
    desc.title = "Kindling";
    try {
        g->engine = strata::Engine::create(desc);
    } catch (const std::exception& ex) {
        std::fprintf(stderr, "kindling: cannot start the renderer: %s\n", ex.what());
        std::string js = "if (Module.onRendererFailed) Module.onRendererFailed(";
        jsonString(js, ex.what());
        js += ")";
        emscripten_run_script(js.c_str());
        return 1;
    }
    play::setLighting(*g->engine, true);

    play::SessionOptions o;
    o.slot = kSlot;
    o.browser = true;
    o.afterSave = [] { emscripten_run_script("if (Module.flushSaves) Module.flushSaves()"); };
    g->session = std::make_unique<play::Session>(*g->engine, o);

    g->last = emscripten_get_now() / 1000.0;
    emscripten_run_script("if (Module.onGameReady) Module.onGameReady()");
    emscripten_set_main_loop_arg(frame, nullptr, 0, false);
    return 0;
}
