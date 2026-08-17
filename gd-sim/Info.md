# GD-Sim

This is a physics simulator for Geometry Dash, optimized for speed. View `Level.hpp` to start your journey of understanding how it works.

## State history

`Level` keeps player states in a bounded ring buffer (`StateHistory.hpp`) rather
than a vector of every frame. The physics only reads one frame backwards
(`prevPlayer`) or forwards (`nextPlayer`), so a small window is enough, and it
keeps branching cheap for the search.

Tools that need the entire trajectory -- the editor overlay, the `gd-sim-test`
CLI -- call `setFullHistory(true)` first. Rolling back beyond the retained
window asserts rather than silently returning the wrong state.

## Testing

Any change to collision or movement must be checked against the golden traces:

```bash
cmake -S . -B ../build-sim -DCMAKE_BUILD_TYPE=Release
cmake --build ../build-sim -j"$(nproc)"
../build-sim/gd-sim-golden verify
```

The traces record exact per-frame values, so an unintended change of even one
ULP will fail and name the frame. When a change is deliberate, re-record with
`gd-sim-golden record` and review the diff.
