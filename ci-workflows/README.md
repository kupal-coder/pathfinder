# CI workflows (apply manually)

These two files could not be pushed from the agent session: the GitHub App
authenticating the push lacks the `workflows` permission, so any commit that
adds or edits `.github/workflows/**` is rejected outright.

Everything else in the change is committed and pushed. To enable CI, copy these
into place and commit them yourself:

```bash
cp ci-workflows/tests.yml .github/workflows/tests.yml
cp ci-workflows/multi-platform.yml .github/workflows/multi-platform.yml
git add .github/workflows
git commit -m "Enable CI on push"
git push
```

- `tests.yml` is new: builds the simulator and search without the Geode SDK,
  then runs the golden traces, the search correctness tests and the gauntlet.
  It also runs the suite under AddressSanitizer and UBSan.
- `multi-platform.yml` is the existing mod build with its `push:` trigger
  uncommented, so it runs on every push instead of only on manual dispatch.

You can delete this directory once they are in place.
