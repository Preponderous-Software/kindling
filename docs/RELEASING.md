# Releasing

A release is a bare version tag on `main` (`0.1.0`, never `v0.1.0`) with a GitHub release on it.
Publishing the release starts [`.github/workflows/release.yml`](../.github/workflows/release.yml),
which builds the desktop game for Windows x64 and Linux x64 and attaches the downloads to the
release. macOS is not built. The browser build is separate: it deploys to the arcade on every push
to `main` ([WEB.md](WEB.md)).

## Steps

0. **Playtest the candidate.** Play `main` with the AI playtest kit
   ([playtester](https://github.com/dmccoystephenson/playtester), Kindling profile) and commit the
   evidence under `docs/playtests/<version>/`. A playtest of an unreleased build is filed under the
   next expected version, with the exact commit in its README. Its findings do not block the release
   on their own; fix or file the confirmed blocker and major ones.

1. **Set the version.** On a branch, set the version in `project(kindling VERSION ...)` at the top
   of [`CMakeLists.txt`](../CMakeLists.txt) and open a pull request. It is the version the game
   reports (the playtest probe's `version`), and the release workflow refuses a tag that does not
   match it.

   In the same pull request, make sure the playtest evidence is named for the version being
   released: if `docs/playtests/` holds a directory for a different version than the one this
   release turned out to be, rename it with `git mv` and update the version inside its README. The
   release workflow fails a tag whose evidence directory is missing, or when any evidence directory
   is named for a later version.

2. **Create the release on the merged commit.** Pass the full SHA, not a branch name, so the tag
   lands on exactly the commit that was checked:

   ```bash
   git fetch origin
   sha=$(git rev-parse origin/main)
   gh release create 0.1.0 --target "$sha" --prerelease \
     --title "Kindling 0.1.0" --notes-file notes.md
   ```

3. **Wait for the builds.** The `Release` workflow runs on the published release. After about ten
   minutes the release has four more assets:

   | Asset | Contents |
   | --- | --- |
   | `Kindling-<version>-win-x64.zip` | `kindling.exe`, `README.txt`, `LICENSE.txt`, `THIRD-PARTY.md` |
   | `Kindling-<version>-linux-x64.tar.gz` | `kindling` and the same three files |
   | `*.sha256` | The SHA-256 of each archive |

   If the workflow fails, fix the cause and re-run it (`gh run rerun <id>`); uploads replace assets
   of the same name.

Pushing a version tag without creating a release also works: the workflow builds the tag and
creates a pre-release for it with generated notes.

## What is in a download

Strata and SDL3 are linked into the executable, and the Windows build links the C++ runtime
statically (`/MT`), so each download is the one executable,
[`README-download.txt`](README-download.txt) as `README.txt`, the licence and
[`THIRD-PARTY.md`](THIRD-PARTY.md). The player needs an OpenGL 4.5 graphics driver on Windows 10/11
x64 or Linux x64, and on Linux glibc 2.34 or newer (Ubuntu 22.04, Debian 12, Fedora 35 and later; the build
runs on Ubuntu 22.04) and the X11 (or XWayland) client libraries every desktop has.

What each run checks:

- **Linux:** the packaged binary links only the C and C++ system libraries, and plays the whole
  restoration headless on Mesa llvmpipe (`--headless --autopilot`), reporting the release's version
  through the playtest probe.
- **Windows:** the runner has no OpenGL 4.5 driver, so the game cannot draw there. `dumpbin
  /dependents` checks that `kindling.exe` imports no Visual C++ runtime or SDL DLL.

## Checking the packaging without a release

A pull request that changes `release.yml`, `CMakeLists.txt` or `docs/playtests/` runs the same
builds as a dry run, keeping the archives as workflow artifacts. So does running it by hand, once
the workflow is on `main`:

```bash
gh workflow run release.yml --ref <branch>             # dry run
gh workflow run release.yml --ref main -f tag=0.1.0    # build a tag and attach to its release
```
