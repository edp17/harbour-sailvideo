# Third-party notices

## LLs Video Player

The developer of LLs Video Player has granted permission for SailVideo to
reuse portions of that project under the BSD 3-Clause conditions described
in `LICENSES/BSD-3-Clause-LLs.txt`.

For every reused source file or substantial source section, SailVideo must:

1. retain the original copyright notice;
2. retain the applicable BSD 3-Clause terms and disclaimer;
3. record the origin and modification in `docs/LLS_CODE_REUSE.md`;
4. avoid any wording which suggests that the original developer or LLs
   Video Player endorses SailVideo.

SailVideo 1.0 contains no source code copied, adapted, modified or rewritten
from LLs Video Player. The notice and licence infrastructure are retained so
that any future reuse cannot be introduced without the required attribution.

## libsmb2

SailVideo can use `libsmb2` for SMB2/SMB3 NAS access. The app first looks for
an app-private bundled shared library and then falls back to system library
names.

The upstream libsmb2 `lib` and `include` directories are licensed under
LGPL-2.1-or-later. SailVideo includes the LGPL-2.1 text in
`LICENSES/LGPL-2.1-libsmb2.txt`. When bundled source is imported, the upstream
`COPYING` and `LICENCE-LGPL-2.1.txt` files must remain in `third_party/libsmb2`
and are installed into SailVideo's private library licence directory.

The default import helper uses the upstream `libsmb2-6.2` tag from:

https://github.com/sahlberg/libsmb2
