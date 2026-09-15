# NAS backend plan

SailVideo now has the planned core network architecture in place:

```text
SMB2/SMB3 NAS
    ↓
libsmb2 backend
    ↓
SailVideo localhost HTTP Range server
    ↓
QMediaPlayer
    ↓
Sailfish GStreamer playback
```

## Current SMB checkpoint: Phase 4 r1

Implemented:

- manual SMB source configuration;
- saved host/share/user metadata;
- guest or username-based connections;
- session-only password entry;
- asynchronous directory listing through runtime-loaded `libsmb2`;
- folder navigation;
- SMB file playback through the existing localhost HTTP Range bridge;
- SMB playback history entries, without saved passwords.

Not implemented yet:

- Sailfish Secrets password storage;
- private bundled `libsmb2` packaging;
- share/server discovery;
- SMB reconnect strategy after Wi-Fi interruption;
- thumbnail/metadata extraction from remote files;
- automatic subtitle discovery from SMB folders.

## Credential rule

Do not store passwords in URLs, logs or QSettings. Phase 4 r1 keeps passwords in
memory only. The next credential step should store only a secret identifier in
normal settings and put passwords in Sailfish Secrets.

## Runtime library strategy

Phase 4 r1 deliberately loads `libsmb2` dynamically instead of linking to it.
This makes the app buildable while NAS UI and stream integration are tested. If
most Sailfish devices do not already provide `libsmb2`, bundle it privately in a
later RPM revision.

## Streaming requirements

The SMB-backed stream must support the same behaviour as the local-file bridge:

- `GET`;
- `HEAD`;
- `Range: bytes=...`;
- `206 Partial Content`;
- `Content-Length`;
- `Content-Range`;
- `Accept-Ranges: bytes`;
- aborted reads;
- repeated demuxer probe requests;
- seeking near the beginning, middle and end of large MP4/MKV files.

## Next likely steps

1. Verify whether `libsmb2` exists on the target Sailfish device.
2. If missing, bundle `libsmb2` privately and add LGPL notices.
3. Replace session-only password entry with Sailfish Secrets.
4. Add better SMB read buffering if playback starts but stutters.
5. Add server/share discovery only after manual SMB playback is reliable.

## Phase 4 r2 decision: bundle libsmb2 privately

The first device test showed no system `libsmb2` runtime. SailVideo therefore
supports building a private shared copy from `third_party/libsmb2` and loading it
by absolute path before trying system library names.

This keeps the app independent from optional distribution packages while still
using dynamic loading. Password handling remains unchanged: passwords are
session-only until Sailfish Secrets is implemented.
