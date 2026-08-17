# Building Path Finding Pro from your phone

You do not need a PC. GitHub Actions builds the mod on its own servers, and you
drive it from a browser. The whole thing is done from the GitHub website, or the
GitHub mobile app plus a file manager.

**Time:** about 5 minutes of tapping, plus 10-20 minutes of waiting for the build.

---

## One-time setup (needed once, ever)

The build workflow currently only runs when you press the button manually. That
button is disabled on this branch because the automated push could not edit
workflow files. So the first time, you need to enable it.

### Step 1 - turn CI on

1. Open the repo on github.com in your phone browser.
2. Switch to the branch **`arena/01a00dd3-pathfinder`** using the branch
   dropdown near the top of the file list.
3. Tap into `ci-workflows/`, open **`multi-platform.yml`**, and tap the pencil
   (edit) icon.
4. Select all the text and copy it.
5. Navigate to `.github/workflows/multi-platform.yml`, tap the pencil, select
   all, and paste over it.
6. Scroll down, tap **Commit changes**, and commit directly to
   `arena/01a00dd3-pathfinder`.
7. Repeat steps 3-6 for **`ci-workflows/tests.yml`**, pasting it into a new file
   at `.github/workflows/tests.yml`. (Use **Add file -> Create new file** and
   type that path.)

> The difference between the two files: `multi-platform.yml` builds the actual
> mod, `tests.yml` runs the simulator and search test suites. You only strictly
> need `multi-platform.yml` to get a `.geode` file.

Once those are committed, the workflow runs automatically on every push to the
branch, so you will not have to do this again.

---

## Every build after that

### Step 2 - start the build

If you did Step 1, the build already started on its own when you committed.
Otherwise:

1. Go to the **Actions** tab.
2. Pick **Build Geode Mod** in the left list.
3. Tap **Run workflow**, choose the branch `arena/01a00dd3-pathfinder`, and
   confirm.

### Step 3 - wait

The run appears at the top of the Actions tab. It builds Windows, macOS,
Android32 and Android64 in parallel, then merges them.

Expect **10-20 minutes**. The first run is slowest; later ones reuse caches. You
can close the app and come back.

A green tick means success. If it fails, see Troubleshooting below.

### Step 4 - download the mod

1. Open the finished run.
2. Scroll to the bottom to the **Artifacts** section.
3. Tap **Build Output** to download it.

You get a `.zip`. Inside is a single `.geode` file that contains **every**
platform, including both Android architectures, so you do not need to pick one.

### Step 5 - install it

1. Extract the `.zip` with any file manager. Most Android file managers can do
   this by long-pressing the file and choosing Extract. If yours cannot, ZArchiver
   is a common free option.
2. Copy the `camila314.pathfinder.geode` file to:

   ```
   /storage/emulated/0/Android/media/com.geode.launcher/game/geode/mods/
   ```

   In a file manager this is: **Internal storage -> Android -> media ->
   com.geode.launcher -> game -> geode -> mods**

3. Launch Geometry Dash through the Geode launcher.

The mod appears in the in-game mod list, and the Path Finding Pro button shows
up on level pages.

> If the `mods` folder does not exist, open GD through the Geode launcher once
> and let it finish loading. Geode creates the folder on first run.

---

## Faster: skip the download and use the mod index

If you would rather not move files around by hand every time, publish the mod to
the Geode index once and then install it in-game like any other mod. That is a
separate process documented at <https://docs.geode-sdk.org/mods/publishing/>,
and it also needs a maintainer to approve the listing, so it is only worth it
once the mod is stable.

---

## Troubleshooting

**"Run workflow" button is missing.**
The workflow file needs a `workflow_dispatch:` trigger. Confirm
`.github/workflows/multi-platform.yml` on your branch has it near the top. If
you completed Step 1, it does.

**Build fails in the "Build the mod" step.**
Open the failed job and read the red step. Real compile errors name a file and
line, for example `src/LibraryPopup.cpp:123`. Send me that text and I will fix
it.

**Build fails mentioning bindings or SDK version.**
The workflow builds against `sdk: nightly` while `mod.json` requires Geode
`5.9.0`. Those normally agree, but if nightly has moved ahead and something
broke, edit `.github/workflows/multi-platform.yml` and change:

```yaml
          sdk: nightly
```

to:

```yaml
          sdk: given
```

which pins the build to the exact version in `mod.json`.

**The mod does not appear in game.**
Check the file is directly inside `.../geode/mods/` and not in a nested folder
left over from extraction. The filename must end in `.geode`.

**Game crashes on launch after installing.**
Delete the `.geode` file from the mods folder to get back in, then send me the
crash log from
`/storage/emulated/0/Android/media/com.geode.launcher/game/geode/crashlogs/`.

---

## What about compiling directly on the phone?

Not realistically. The mod needs the Android NDK, CMake, the Geode CLI and the
generated GD bindings. Termux can install a toolchain, but the NDK cross-compile
setup is not supported by Geode's tooling on-device, and you would be fighting
it for hours. GitHub Actions does the same work on a real machine for free, so
the browser route above is genuinely the practical answer.

The one thing you *can* usefully do on-device is run the non-Geode test suites,
since the simulator and search engine are plain C++20 with no game dependency.
In Termux:

```bash
pkg install git clang cmake binutils
git clone https://github.com/kupal-coder/pathfinder
cd pathfinder
git checkout arena/01a00dd3-pathfinder
cmake -S gd-sim -B build-sim -DCMAKE_BUILD_TYPE=Release
cmake --build build-sim -j4
./build-sim/gd-sim-golden verify
./build-sim/gd-sim-searchtest
./build-sim/gd-sim-librarytest
```

All three should end with `0 failure(s)`. This will not produce an installable
mod, but it does verify the physics and the solver, which is where most of the
logic lives.

I verified this exact command sequence against a fresh clone of the branch. Two
caveats I could not test from here: Termux ships clang rather than gcc, and
`nproc` is not always present, which is why the build uses a fixed `-j4`. If
clang objects to something gcc accepted, send me the error.
