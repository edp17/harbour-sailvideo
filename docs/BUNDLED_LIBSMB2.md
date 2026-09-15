# Bundled libsmb2 for SailVideo

SailVideo first tried to load a system-provided `libsmb2` runtime library, but
on the first NAS test device no compatible package was installed. The app now
supports an app-private bundled build.

## Import source

From the repository root:

```sh
sh tools/import-libsmb2.sh
```

By default this imports upstream tag:

```text
libsmb2-6.2
```

A different tag can be selected explicitly:

```sh
sh tools/import-libsmb2.sh libsmb2-6.1
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


## Phase 4 r3 build rule

From Phase 4 r3 onward, an SMB-enabled build requires the libsmb2 source tree
to be present before CMake is configured. This avoids producing an RPM that
installs successfully but cannot browse NAS shares.

Run this before building:

```sh
sh tools/import-libsmb2.sh
rm -rf CMakeFiles CMakeCache.txt Makefile cmake_install.cmake harbour-sailvideo_autogen install_manifest.txt
```

Then configure/build normally.

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
