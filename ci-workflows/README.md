# CI workflows (apply manually)

These files cannot be pushed from the agent session: the GitHub App
authenticating the push lacks the `workflows` permission, so any commit that
adds or edits `.github/workflows/**` is rejected outright:

```
! [remote rejected] refusing to allow a GitHub App to create or update
  workflow `.github/workflows/multi-platform.yml` without `workflows` permission
```

Everything else in the change is committed and pushed. Copy these into place
and commit them yourself:

```bash
cp ci-workflows/multi-platform.yml       .github/workflows/multi-platform.yml
cp ci-workflows/multi-platform-debug.yml .github/workflows/multi-platform-debug.yml
cp ci-workflows/tests.yml                .github/workflows/tests.yml
git add .github/workflows
git commit -m "Fix CI: authenticate the Geode SDK binary lookup"
git push
```

- `multi-platform.yml` -- the mod build. **Contains the fix for the currently
  failing build** (see below). Its `push:` trigger is also uncommented, so it
  runs on every push instead of only on manual dispatch.
- `multi-platform-debug.yml` -- the Debug variant of the same build, with the
  same fix. Still manual-dispatch only.
- `tests.yml` -- new: builds the simulator and search without the Geode SDK,
  then runs the golden traces, the search correctness tests and the gauntlet.
  It also runs the suite under AddressSanitizer and UBSan.

## The bug these fix

The mod build has been failing before a single file is compiled:

```
| Info | Installing binaries for 5.9.0
| Fail | Could not parse Geode release "v5.9.0": error decoding response body
##[error]Process completed with exit code 1.
```

`geode sdk install-binaries` looks up the Geode release on `api.github.com` to
find out which asset to download. It only sends an `Authorization` header when
`GITHUB_TOKEN` is present in the environment ([geode-sdk/cli, `src/sdk.rs`][cli]):

```rust
.header(
    AUTHORIZATION,
    std::env::var("GITHUB_TOKEN").map_or("".into(), |token| format!("Bearer {token}")),
)
```

`geode-sdk/build-geode-mod` does not set that variable, so the request goes out
anonymous and draws on the 60-requests-per-hour-per-IP allowance shared by every
other job on the runner. Once the allowance is spent the API replies with a
rate-limit JSON body, which does not deserialise into a release, and the CLI
aborts.

The fix is to pass the token to the build step:

```yaml
      - name: Build the mod
        uses: geode-sdk/build-geode-mod@main
        env:
          GITHUB_TOKEN: ${{ secrets.GITHUB_TOKEN }}
```

This raises the limit to 1000/hour for the repository. The jobs also declare the
`contents: read` permission they actually need.

### Why it was confusing

The step runs before CMake, so **no code is ever compiled** -- sccache reports
`Compile requests 0` and the "Run CMake and build" step is skipped. It presents
as a build failure with no compiler output.

It is also intermittent and platform-independent, because it depends on how much
of the shared per-IP allowance is left when the job happens to run:

| Run | Failing jobs | Release requested |
|---|---|---|
| 32039329070 | all four | `nightly` |
| 32039624698 | Windows only | `v5.9.0` |

So it can look Windows-specific, and re-running can appear to "fix" it. Nothing
in the mod sources was ever involved.

### Note on `sdk: nightly` vs `sdk: latest`

`.github/workflows/multi-platform.yml` was changed by hand in c8ffaa8 from
`sdk: nightly` to `sdk: latest`. That is carried over here so this copy does not
reintroduce the nightly pin. It is a separate concern from the token: `latest`
resolves to the current stable release rather than the tip commit, but either
value hits the same unauthenticated lookup.

[cli]: https://github.com/geode-sdk/cli/blob/main/src/sdk.rs

You can delete this directory once these are in place.
