# Notes for Claude

## Merging and releasing

When the user asks to merge, release or "cut" a version, do it end to end
without asking them to run anything:

1. Commit and push the work to the session's branch, open a pull request
   against `master` (if there isn't one) and wait for the `Linux build`
   workflow (`.github/workflows/build-linux.yml`) to pass on it.
2. Merge the pull request (merge commit).
3. Cut the release by running the `Linux build` workflow on `master` with
   `workflow_dispatch`, inputs `release_tag` = the new version and
   `release_target` = `master`. This builds `doom3-linux-x86_64.tar.gz`,
   creates the tag and publishes the GitHub release with
   `dist/RELEASE_NOTES.md` as the description.
4. Wait for the run to finish and check that the release has the asset.

Versions are `vMAJOR.MINOR.PATCH`. Unless the user names a version, bump
the latest release tag: patch for fixes, minor for new features. Update
`dist/RELEASE_NOTES.md` first when the release adds something worth
mentioning.

## Building

See `BUILDING.md`. Linux: `cmake -S neo -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build -j`
(needs SDL2, GL and OpenAL headers).
