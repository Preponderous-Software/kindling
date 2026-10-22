// M6 Saves: a save made at any step loads back to the identical world (same
// fingerprint), a damaged save is refused and the previous one loaded, and
// SHA-256 matches the standard's test vectors.
#include <cstdio>
#include <filesystem>
#include <fstream>

#include "check.h"
#include "save/save.h"
#include "save/sha256.h"
#include "sim/autopilot.h"
#include "sim/progress.h"

using namespace kindling;
namespace fs = std::filesystem;

namespace {

void corrupt(const std::string& path, size_t offset) {
    std::fstream f(path, std::ios::in | std::ios::out | std::ios::binary);
    f.seekg(static_cast<std::streamoff>(offset));
    char c = 0;
    f.read(&c, 1);
    c = static_cast<char>(c ^ 0x10);
    f.seekp(static_cast<std::streamoff>(offset));
    f.write(&c, 1);
}

}  // namespace

int main(int argc, char** argv) {
    // SHA-256 test vectors (FIPS 180-2 appendix B).
    CHECK(toHex(sha256("abc", 3)) == "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
    CHECK(toHex(sha256("", 0)) == "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
    const std::string longer = "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq";
    CHECK(toHex(sha256(longer.data(), longer.size())) == "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1");

    // ---- a save taken at every step loads back identical
    World w = newWorld(1881);
    Autopilot pilot;
    Step last = progress(w).step;
    int roundTrips = 0;
    const int64_t limit = 60LL * 60 * kTickRate;
    while (progress(w).step != Step::Done && w.tick < limit) {
        tick(w, pilot.next(w));
        const Step now = progress(w).step;
        if (now != last || w.tick % 2000 == 0) {
            last = now;
            const std::vector<uint8_t> bytes = encodeSave(w);
            World back;
            std::string why;
            const bool ok = decodeSave(bytes, back, &why);
            CHECK(ok);
            if (!ok) std::fprintf(stderr, "decode failed at tick %lld: %s\n", static_cast<long long>(w.tick), why.c_str());
            CHECK(fingerprint(back) == fingerprint(w));
            ++roundTrips;
            // And the game goes on identically from the loaded copy.
            if (ok && roundTrips % 5 == 0) {
                World a = w, b = back;
                Autopilot pa = pilot, pb = pilot;
                for (int i = 0; i < 300; ++i) {
                    tick(a, pa.next(a));
                    tick(b, pb.next(b));
                }
                CHECK(fingerprint(a) == fingerprint(b));
            }
        }
    }
    CHECK(progress(w).step == Step::Done);
    CHECK(roundTrips > 6);
    std::printf("%d save round trips, final save %zu bytes\n", roundTrips, encodeSave(w).size());

    // ---- files: the previous save is kept, and a damaged save falls back to it
    const fs::path slot = fs::path(argc > 1 ? argv[1] : ".") / "save-test-slot";
    fs::remove_all(slot);
    World early = newWorld(7);
    for (int i = 0; i < 100; ++i) tick(early, Input{});
    std::string why;
    CHECK(writeSave(slot.string(), early, &why));
    CHECK(writeSave(slot.string(), w, &why));
    CHECK(fs::exists(savePath(slot.string())) && fs::exists(previousSavePath(slot.string())));
    World loaded;
    LoadResult r = loadSave(slot.string(), loaded);
    CHECK(r.ok && !r.fromPrevious);
    CHECK(fingerprint(loaded) == fingerprint(w));

    // Every kind of damage is caught: a flipped bit anywhere, a truncated file.
    const size_t size = static_cast<size_t>(fs::file_size(savePath(slot.string())));
    for (size_t offset : {size_t{3}, size_t{10}, size_t{25}, size / 2, size - 5}) {
        // A fresh save over the damaged one: the damaged one must not become the backup.
        CHECK(writeSave(slot.string(), w, &why));
        corrupt(savePath(slot.string()), offset);
        World x;
        r = loadSave(slot.string(), x);
        if (!(r.ok && r.fromPrevious)) std::fprintf(stderr, "offset %zu: %s\n", offset, r.message.c_str());
        CHECK(r.ok && r.fromPrevious);
        CHECK(fingerprint(x) == fingerprint(w));
        CHECK(!r.message.empty());
    }
    fs::resize_file(savePath(slot.string()), size - 100);
    World t;
    r = loadSave(slot.string(), t);
    CHECK(r.ok && r.fromPrevious);

    // Both damaged: nothing loads, and the world passed in is left alone.
    corrupt(previousSavePath(slot.string()), size / 3);
    World untouched = newWorld(3);
    r = loadSave(slot.string(), untouched);
    CHECK(!r.ok);
    CHECK(fingerprint(untouched) == fingerprint(newWorld(3)));

    // Not a save at all.
    std::vector<uint8_t> junk(200, 0x42);
    World j;
    CHECK(!decodeSave(junk, j, &why));

    fs::remove_all(slot);
    TEST_MAIN_END();
}
