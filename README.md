# SailVideo

SailVideo is a native Sailfish OS video player intended to support:

- local video files;
- ordinary HTTP/HTTPS media URLs;
- SMB2/SMB3 NAS shares through bundled libsmb2;
- additional media-server protocols in later phases.

## Current release: SailVideo 1.0

This checkpoint provides:

- a CMake and RPM project foundation;
- local video selection through `Sailfish.Pickers`;
- playback through `QtMultimedia.MediaPlayer` and `VideoOutput`;
- play, pause, configurable skip controls and timeline seeking;
- a basic active cover;
- command-line opening of a local file or URL;
- Sailjail permission declarations, including MediaIndexing for the picker;
- resume position storage;
- a recently played list;
- stable stop/suspend behaviour when leaving the player page;
- display keep-awake while video is playing;
- a local HTTP Range bridge for local-file and SMB-file playback;
- persistent HTTP/HTTPS network source management;
- direct playback of saved or one-off HTTP/HTTPS media URLs;
- manually configured SMB/NAS source management;
- asynchronous SMB directory browsing when `libsmb2` is available at runtime;
- SMB file playback through the local HTTP Range bridge when `libsmb2` is available at runtime;
- optional app-private bundled `libsmb2` build support;
- the legal structure required before LLs Video Player code is reused.

SailVideo 1.0 contains no source code copied, adapted, modified or rewritten from
LLs Video Player. The BSD licence/reuse documentation remains in the repository
only so any possible future reuse is properly attributed.

## Build

Open the project in Sailfish IDE or build it using the usual Sailfish SDK
CMake/RPM workflow for armv7hl or aarch64.

The repository vendors the `libsmb2-6.2` source under
`third_party/libsmb2`, so a normal clone contains everything required for an
SMB/NAS-enabled build. No dependency bootstrap step should be necessary.

`tools/import-libsmb2.sh` is a maintainer/recovery helper for re-vendoring the
pinned upstream source. It also stages the complete vendored tree and verifies
that `third_party/libsmb2/CMakeLists.txt` is tracked.

Before pushing a vendoring update, run:

```sh
sh tools/verify-vendored-libsmb2.sh
```

That check verifies that the required libsmb2 files are present in the current
Git commit, not merely in the local working tree.

A local-video-only developer build can still be configured with
`-DSAILVIDEO_ALLOW_MISSING_LIBSMB2=ON`, but do not use that option for NAS
testing.

## Test target

The first test should use a known-good local H.264/AAC MP4 file. Confirm:

1. the video picker opens;
2. the video starts;
3. audio and picture are present;
4. pause/resume works;
5. the skip controls use the configured interval;
6. tapping the timeline seeks;
7. orientation changes do not terminate playback;
8. cover play/pause works;
9. returning to the main page stops playback cleanly;
10. recently played entries show real filenames;
11. reopening a recent item resumes near the saved position;
12. opening the same file through the HTTP bridge plays and seeks;
13. adding a saved HTTP/HTTPS media URL works;
14. opening a one-off HTTP/HTTPS URL without saving works;
15. saved network sources survive an app restart;
16. a fresh repository clone already contains `third_party/libsmb2/CMakeLists.txt`;
17. the bundled build installs `libsmb2` under `/usr/lib*/harbour-sailvideo`;
18. NAS sources can be added and edited;
19. the NAS page reports whether `libsmb2` is available;
20. with `libsmb2` available, a manually configured SMB share can be browsed;
21. with `libsmb2` available, a video file on SMB can be opened and seeked.

## Licensing

Original SailVideo code is licensed under GPL-3.0-or-later.

Any source code reused from LLs Video Player remains subject to its
BSD 3-Clause terms. See:

- `LICENSES/BSD-3-Clause-LLs.txt`
- `THIRD_PARTY_NOTICES.md`
- `docs/LLS_CODE_REUSE.md`

Bundled libsmb2 is licensed under LGPL-2.1-or-later. See:

- `LICENSES/LGPL-2.1-libsmb2.txt`
- `docs/BUNDLED_LIBSMB2.md`

The SMB/NAS architecture is tracked in `docs/NAS_BACKEND_PLAN.md` and the test
procedure is tracked in `docs/SMB_TESTING.md`.


## SailVideo 1.0

SailVideo 1.0 is the first public release checkpoint. It provides local video
library browsing and playback, SMB2/SMB3 NAS browsing and playback through the
local HTTP Range bridge, persistent HTTP/HTTPS URL sources, resume/history,
player scaling and gesture controls, Sailfish Secrets credential storage, and
NAS picture browsing.
