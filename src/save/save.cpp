#include "save/save.h"

#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <type_traits>

#include "save/sha256.h"

namespace fs = std::filesystem;

namespace kindling {

namespace {

constexpr char kMagic[8] = {'K', 'I', 'N', 'D', 'L', 'I', 'N', 'G'};
constexpr uint64_t kMaxCount = 1u << 24;  // no list in a save is longer; anything longer is damage

// Writes little-endian.
struct Writer {
    std::vector<uint8_t> out;
    void raw(uint64_t v, int bytes) {
        for (int i = 0; i < bytes; ++i) out.push_back(static_cast<uint8_t>(v >> (8 * i)));
    }
    template <class T>
    void value(const T& v) {
        if constexpr (std::is_same_v<T, bool>) {
            raw(v ? 1 : 0, 1);
        } else if constexpr (std::is_same_v<T, float>) {
            uint32_t bits;
            std::memcpy(&bits, &v, 4);
            raw(bits, 4);
        } else if constexpr (std::is_enum_v<T>) {
            raw(static_cast<uint64_t>(static_cast<int64_t>(v)), 4);
        } else {
            raw(static_cast<uint64_t>(v), static_cast<int>(sizeof(T)));
        }
    }
    void count(size_t n) { raw(n, 8); }
    void bytes(const std::vector<uint8_t>& b) {
        count(b.size());
        out.insert(out.end(), b.begin(), b.end());
    }
};

// Reads little-endian, refusing to read past the end.
struct Reader {
    const uint8_t* p;
    size_t n, at = 0;
    bool ok = true;
    uint64_t raw(int bytes) {
        if (!ok || n - at < static_cast<size_t>(bytes)) {
            ok = false;
            return 0;
        }
        uint64_t v = 0;
        for (int i = 0; i < bytes; ++i) v |= static_cast<uint64_t>(p[at + i]) << (8 * i);
        at += static_cast<size_t>(bytes);
        return v;
    }
    template <class T>
    void value(T& v) {
        if constexpr (std::is_same_v<T, bool>) {
            const uint64_t b = raw(1);
            if (b > 1) ok = false;
            v = b != 0;
        } else if constexpr (std::is_same_v<T, float>) {
            const auto bits = static_cast<uint32_t>(raw(4));
            std::memcpy(&v, &bits, 4);
        } else if constexpr (std::is_enum_v<T>) {
            v = static_cast<T>(static_cast<int32_t>(raw(4)));
        } else {
            v = static_cast<T>(raw(static_cast<int>(sizeof(T))));
        }
    }
    size_t count() {
        const uint64_t c = raw(8);
        if (c > kMaxCount) ok = false;
        return ok ? static_cast<size_t>(c) : 0;
    }
    void bytes(std::vector<uint8_t>& b) {
        const size_t c = count();
        if (!ok || n - at < c) {
            ok = false;
            return;
        }
        b.assign(p + at, p + at + c);
        at += c;
    }
};

// One description of the save's fields, used both ways.
template <class IO>
struct Fields {
    IO& io;
    static constexpr bool kWriting = std::is_same_v<IO, Writer>;

    template <class T>
    void v(T& x) { io.value(x); }
    void v(Vec3& p) {
        v(p.x);
        v(p.y);
        v(p.z);
    }
    void v(std::vector<uint8_t>& b) { io.bytes(b); }
    template <class T, class F>
    void list(std::vector<T>& xs, F each) {
        size_t c = xs.size();
        if constexpr (kWriting) io.count(c);
        else {
            c = io.count();
            xs.assign(c, T{});
        }
        for (T& x : xs) each(x);
    }
    void debris(Debris& d) {
        v(d.id);
        v(d.kind);
        v(d.pos);
        v(d.yaw);
        v(d.hauled);
    }
    void item(Item& it) {
        v(it.id);
        v(it.kind);
        v(it.place);
        v(it.pos);
        v(it.yaw);
    }
    void hearth(Hearth& h) {
        v(h.ash);
        v(h.kindlingLaid);
        v(h.logsLaid);
        v(h.lit);
        v(h.fire);
        v(h.fuelMinutes);
        v(h.candleLit);
        v(h.matchesStruck);
    }
    void settler(Settler& s) {
        v(s.state);
        v(s.x);
        v(s.z);
        v(s.yaw);
        v(s.waypoint);
        v(s.line);
        v(s.lineSeconds);
        v(s.briefMet);
        v(s.done);
    }
    void keyframe(Keyframe& k) {
        v(k.tick);
        v(k.step);
        v(k.lantern);
        v(k.playerX);
        v(k.playerZ);
        v(k.playerYaw);
        for (std::vector<uint8_t>& g : k.grime) v(g);
        list(k.debris, [this](Debris& d) { debris(d); });
        for (PlankState& p : k.planks) v(p);
        for (int32_t& b : k.windowBoards) v(b);
        v(k.roofPatched);
        v(k.chair);
        v(k.table);
        v(k.snow);
        list(k.items, [this](Item& it) { item(it); });
        hearth(k.hearth);
        v(k.woodpile);
        settler(k.settler);
    }
    void world(World& w) {
        v(w.tick);
        v(w.seed);
        v(w.player.x);
        v(w.player.z);
        v(w.player.yaw);
        v(w.player.pitch);
        v(w.tool);
        v(w.carry);
        v(w.carryIndex);
        v(w.workKind);
        v(w.workIndex);
        v(w.workSeconds);
        list(w.debris, [this](Debris& d) { debris(d); });
        for (PlankState& p : w.planks) v(p);
        for (int32_t& b : w.windowBoards) v(b);
        v(w.roofPatched);
        v(w.chair);
        v(w.table);
        v(w.snow);
        v(w.debrisHauled);
        for (std::vector<uint8_t>& t : w.grime.texels) v(t);
        list(w.items, [this](Item& it) { item(it); });
        hearth(w.hearth);
        v(w.wood.rounds);
        v(w.wood.roundOnBlock);
        v(w.wood.splitByBlock);
        v(w.wood.kindlingByBlock);
        v(w.wood.woodpile);
        v(w.lantern);
        v(w.horse.dirt);
        v(w.horse.hunger);
        v(w.horse.eatingSeconds);
        v(w.horse.brushedSeconds);
        v(w.horse.greeted);
        settler(w.settler);
        list(w.history, [this](Keyframe& k) { keyframe(k); });
        v(w.cleared);
        v(w.repaired);
        v(w.kindled);
        v(w.surveyed);
    }
};

// What a decoded world must look like before the rules may run on it.
bool plausible(const World& w, std::string* error) {
    auto fail = [error](const char* why) {
        if (error) *error = why;
        return false;
    };
    for (int s = 0; s < kSurfaceCount; ++s) {
        const SurfaceDef& d = surfaceDef(static_cast<Surface>(s));
        if (w.grime.texels[static_cast<size_t>(s)].size() != static_cast<size_t>(d.width * d.height))
            return fail("a grime surface has the wrong size");
    }
    if (w.carry == Carry::Debris && (w.carryIndex < 0 || static_cast<size_t>(w.carryIndex) >= w.debris.size()))
        return fail("carrying debris that does not exist");
    if (w.carry == Carry::Item && (w.carryIndex < 0 || static_cast<size_t>(w.carryIndex) >= w.items.size()))
        return fail("carrying an item that does not exist");
    if (w.carry == Carry::Firewood && (w.carryIndex < 0 || w.carryIndex > 2)) return fail("carrying unknown firewood");
    if (static_cast<int>(w.carry) < 0 || static_cast<int>(w.carry) > static_cast<int>(Carry::Hay))
        return fail("carrying something unknown");
    if (w.items.size() != newItems().size()) return fail("the wagon's load has the wrong number of things");
    if (static_cast<int>(w.tool) < 0 || static_cast<int>(w.tool) >= kToolCount) return fail("an unknown tool");
    return true;
}

bool readFile(const std::string& path, std::vector<uint8_t>& out) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return false;
    out.assign(std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>());
    return true;
}

}  // namespace

std::vector<uint8_t> encodeSave(const World& w) {
    Writer payload;
    Fields<Writer> fields{payload};
    fields.world(const_cast<World&>(w));  // the Writer only reads

    Writer file;
    file.out.insert(file.out.end(), kMagic, kMagic + 8);
    file.value(kSaveVersion);
    file.value(static_cast<uint64_t>(payload.out.size()));
    file.out.insert(file.out.end(), payload.out.begin(), payload.out.end());
    const std::array<uint8_t, 32> digest = sha256(payload.out.data(), payload.out.size());
    file.out.insert(file.out.end(), digest.begin(), digest.end());
    return file.out;
}

bool decodeSave(const std::vector<uint8_t>& bytes, World& out, std::string* error) {
    auto fail = [error](const std::string& why) {
        if (error) *error = why;
        return false;
    };
    if (bytes.size() < 8 + 4 + 8 + 32 || std::memcmp(bytes.data(), kMagic, 8) != 0) return fail("not a Kindling save");
    Reader head{bytes.data(), bytes.size()};
    head.at = 8;
    uint32_t version = 0;
    uint64_t length = 0;
    head.value(version);
    head.value(length);
    if (version != kSaveVersion) return fail("saved by a different version of Kindling (format " + std::to_string(version) + ")");
    if (length != bytes.size() - 8 - 4 - 8 - 32) return fail("the save is truncated or has extra bytes");
    const uint8_t* payload = bytes.data() + 20;
    const std::array<uint8_t, 32> digest = sha256(payload, static_cast<size_t>(length));
    if (std::memcmp(digest.data(), payload + length, 32) != 0) return fail("the save is damaged (its checksum does not match)");

    Reader r{payload, static_cast<size_t>(length)};
    Fields<Reader> fields{r};
    World w;
    fields.world(w);
    if (!r.ok || r.at != r.n) return fail("the save's contents do not parse");
    if (!plausible(w, error)) return false;
    // What is still on the wagon sits where the wagon's load puts it now
    // (0.1.0 saves have the older, crowded layout).
    const std::vector<Item> load = newItems();
    for (size_t i = 0; i < w.items.size(); ++i)
        if (w.items[i].place == ItemPlace::Wagon) {
            w.items[i].pos = load[i].pos;
            w.items[i].yaw = load[i].yaw;
        }
    out = std::move(w);
    return true;
}

std::string savePath(const std::string& slot) { return (fs::path(slot) / "kindling.save").string(); }
std::string previousSavePath(const std::string& slot) { return (fs::path(slot) / "kindling.save.prev").string(); }

bool writeSave(const std::string& slot, const World& w, std::string* error) {
    std::error_code ec;
    fs::create_directories(slot, ec);
    const std::vector<uint8_t> bytes = encodeSave(w);
    const fs::path tmp = fs::path(slot) / "kindling.save.tmp";
    {
        std::ofstream f(tmp, std::ios::binary | std::ios::trunc);
        f.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
        f.flush();
        if (!f) {
            if (error) *error = "could not write " + tmp.string();
            return false;
        }
    }
    // Keep what was there as the previous save, then move the new one in. A
    // damaged save never replaces a good previous one: it is discarded instead.
    const fs::path cur = savePath(slot), prev = previousSavePath(slot);
    if (fs::exists(cur, ec)) {
        std::vector<uint8_t> old;
        World check;
        if (readFile(cur.string(), old) && decodeSave(old, check, nullptr)) {
            fs::remove(prev, ec);
            fs::rename(cur, prev, ec);
            if (ec) {
                if (error) *error = "could not keep the previous save: " + ec.message();
                return false;
            }
        } else {
            fs::remove(cur, ec);
        }
    }
    fs::rename(tmp, cur, ec);
    if (ec) {
        if (error) *error = "could not move the new save into place: " + ec.message();
        return false;
    }
    return true;
}

LoadResult loadSave(const std::string& slot, World& out) {
    LoadResult r;
    std::vector<uint8_t> bytes;
    std::string why;
    const bool haveCurrent = readFile(savePath(slot), bytes);
    if (haveCurrent && decodeSave(bytes, out, &why)) {
        r.ok = true;
        r.message = "Loaded your save.";
        return r;
    }
    std::string whyPrev;
    if (readFile(previousSavePath(slot), bytes) && decodeSave(bytes, out, &whyPrev)) {
        r.ok = true;
        r.fromPrevious = true;
        r.message = haveCurrent ? "Your latest save was refused (" + why + "). Loaded the one before it."
                                : "Loaded your previous save.";
        return r;
    }
    r.message = haveCurrent ? "Your save was refused (" + why + ") and there is no good earlier save."
                            : "No save to load.";
    return r;
}

std::string defaultSlot() {
#ifdef _WIN32
    char* appData = nullptr;
    size_t len = 0;
    std::string base = ".";
    if (_dupenv_s(&appData, &len, "APPDATA") == 0 && appData) base = appData;
    std::free(appData);
    return (fs::path(base) / "Kindling" / "default").string();
#else
    const char* xdg = std::getenv("XDG_DATA_HOME");
    if (xdg && *xdg) return (fs::path(xdg) / "kindling" / "default").string();
    const char* home = std::getenv("HOME");
    return (fs::path(home ? home : ".") / ".local" / "share" / "kindling" / "default").string();
#endif
}

}  // namespace kindling
