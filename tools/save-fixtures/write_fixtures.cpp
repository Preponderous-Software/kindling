// Writes the frozen save fixtures in tests/fixtures/ with the game's own save
// code (writeSave), from the autopilot's playthrough of a new game. Built
// against a checkout of the release whose format the fixtures stand for (see
// tests/fixtures/README.md); never part of the normal build.
//
//   write_fixtures OUT_DIR
//
// writes OUT_DIR/v<format>-repair.save (the first tick of the Repair step:
// surveyed and cleared, nothing mended yet) and OUT_DIR/v<format>-done.save
// (the restoration finished), and prints each one's tick, step and fingerprint.
#include <cstdio>
#include <filesystem>
#include <string>

#include "save/save.h"
#include "sim/autopilot.h"
#include "sim/progress.h"
#include "sim/world.h"

using namespace kindling;
namespace fs = std::filesystem;

namespace {

bool write(const World& w, const fs::path& out, const std::string& name) {
    const fs::path slot = out / ("slot-" + name);
    fs::remove_all(slot);
    std::string why;
    if (!writeSave(slot.string(), w, &why)) {
        std::fprintf(stderr, "writeSave failed: %s\n", why.c_str());
        return false;
    }
    const fs::path file = out / ("v" + std::to_string(kSaveVersion) + "-" + name + ".save");
    fs::copy_file(savePath(slot.string()), file, fs::copy_options::overwrite_existing);
    fs::remove_all(slot);
    std::printf("%s: tick %lld, step %s, fingerprint %016llx\n", file.filename().string().c_str(),
                static_cast<long long>(w.tick), stepName(progress(w).step),
                static_cast<unsigned long long>(fingerprint(w)));
    return true;
}

}  // namespace

int main(int argc, char** argv) {
    if (argc != 2) {
        std::fprintf(stderr, "usage: write_fixtures OUT_DIR\n");
        return 2;
    }
    const fs::path out = argv[1];
    fs::create_directories(out);
    World w = newWorld(1);
    Autopilot pilot;
    const int64_t limit = 60LL * 60 * kTickRate;
    bool repair = false;
    while (progress(w).step != Step::Done && w.tick < limit) {
        tick(w, pilot.next(w));
        if (!repair && progress(w).step == Step::Repair) {
            if (!write(w, out, "repair")) return 1;
            repair = true;
        }
    }
    if (!repair || progress(w).step != Step::Done) {
        std::fprintf(stderr, "the autopilot did not finish the restoration\n");
        return 1;
    }
    return write(w, out, "done") ? 0 : 1;
}
