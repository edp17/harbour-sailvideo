# Bundled libsmb2 for SailVideo

SailVideo first tried to load a system-provided `libsmb2` runtime library, but
on the first NAS test device no compatible package was installed. The app now
supports an app-private bundled build.

## Vendored source

SailVideo vendors the upstream `libsmb2-6.2` source directly under:

```text
third_party/libsmb2/
```

This is intentional: a normal clone of `harbour-sailvideo` must contain all
source required for SMB/NAS builds. The build must not depend on network access
or an extra submodule/bootstrap step.

`tools/import-libsmb2.sh` is retained as a maintainer/recovery helper. It clones
the requested upstream tag into a temporary directory, removes the nested Git
metadata, copies the source into `third_party/libsmb2`, and writes
`SAILVIDEO_VENDOR.txt` with the exact upstream tag and commit.

To refresh the vendored source:

```sh
rm -rf third_party/libsmb2
sh tools/import-libsmb2.sh
git add third_party/libsmb2 tools/import-libsmb2.sh
```

## Build behaviour

When `third_party/libsmb2/CMakeLists.txt` exists, SailVideo's top-level CMake
builds libsmb2 as a shared library with:

```text
BUILD_SHARED_LIBS=ON
ENABLE_EXAMPLES=OFF
ENABLE_LIBKRB5=OFF
ENABLE_GSSAPI=OFF
```

Kerberos/GSSAPI are disabled intentionally for the first Sailfish package to
avoid extra runtime dependencies. Home NAS username/password use should go
through libsmb2's built-in NTLMSSP path.

The private library is installed below:

```text
/usr/lib*/harbour-sailvideo/
```

At runtime, SailVideo searches this private location before falling back to
system library names such as `smb2`, `libsmb2.so`, `libsmb2.so.1`, and
`libsmb2.so.0`.

## Licensing

libsmb2's `lib` and `include` directories are licensed under LGPL-2.1-or-later.
When bundling the library, keep the upstream licence files in the source tree
and package. SailVideo installs upstream `COPYING` and `LICENCE-LGPL-2.1.txt`
under its private library licence directory when bundled source is present.


## SMB-enabled build rule

An SMB-enabled build requires `third_party/libsmb2/CMakeLists.txt` to be
present before CMake is configured. In the public SailVideo source repository
this file is part of the vendored libsmb2 tree, so a fresh clone should build
normally without running an import command.

If the vendored directory has deliberately been removed during maintenance,
restore it with `tools/import-libsmb2.sh` before configuring CMake.

For a temporary local-video-only developer build, CMake can be configured with:

```sh
-DSAILVIDEO_ALLOW_MISSING_LIBSMB2=ON
```

Do not use that option for NAS testing.

After building the RPM, check that the private SMB library is packaged:

```sh
sh tools/check-libsmb2.sh
```

The installed device should contain at least one of:

```text
/usr/lib/harbour-sailvideo/libsmb2.so
/usr/lib64/harbour-sailvideo/libsmb2.so
```


## Phase 4 r4 packaging fix

The bundled libsmb2 shared library must not be installed into the global system
library directory. It must be app-private:

```text
/usr/lib*/harbour-sailvideo/libsmb2.so*
```

Phase 4 r4 redirects the libsmb2 subproject install destination by temporarily
setting `LIB_SUFFIX` before `add_subdirectory(third_party/libsmb2)` and then
restoring it for the rest of SailVideo.


## Verify before pushing

A local `test -f third_party/libsmb2/CMakeLists.txt` only proves that the file
exists in the current working tree. It does **not** prove that Git tracks it or
that a fresh clone will contain it.

After committing a vendoring update, always run:

```sh
sh tools/verify-vendored-libsmb2.sh
```

The verification checks both the working tree and `HEAD`. A successful result
means `third_party/libsmb2/CMakeLists.txt` is part of the actual commit that
will be pushed.
