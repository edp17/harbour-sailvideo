# SailVideo

SailVideo is a native Sailfish OS media player for local, web and home-network media.

**Current package version:** 1.1.0.17  
**Release line:** SailVideo 1.1

## Features

- Native Sailfish Silica / QtMultimedia video playback.
- Local video library grouped by folder with Sailfish thumbnails.
- Resume positions and recently played history.
- Configurable skip interval, new-video volume and brightness.
- Fit, Crop and Stretch scaling.
- Swipe controls for video brightness and playback volume.
- Keep-display-on support while video is playing.
- Persistent HTTP/HTTPS video sources.
- SMB2/SMB3 NAS sources using bundled app-private libsmb2.
- Secure SMB password storage through Sailfish Secrets.
- NAS folder browsing with media filtering, sorting and saved subfolders.
- NAS picture viewer with previous/next and configurable slideshow interval.
- Native Google Chromecast discovery and remembered receivers.
- Video and picture casting directly from the Player and Picture Viewer pages.
- Remote Cast play/pause, seek, skip, previous/next, mute, stop/continue and disconnect.
- Rejoin of an active Cast receiver session where the source remains reachable.
- AVI casting with lossless H.264/AAC/MP3 remuxing and VP8/Vorbis WebM transcode fallback.

## Playback architecture

Phone playback uses the Sailfish QtMultimedia/GStreamer stack:

```text
Sailfish Silica / QML
        |
QtMultimedia MediaPlayer + VideoOutput
        |
Sailfish GStreamer stack
```

SMB video playback is exposed to QtMultimedia through SailVideo's local HTTP
Range bridge:

```text
SMB2/SMB3 NAS
        |
bundled libsmb2
        |
127.0.0.1 HTTP Range bridge
        |
QMediaPlayer
```

The bridge supports GET, HEAD and byte ranges and is also reused when local or
SMB media needs to be exposed to a Chromecast over the LAN.

## Chromecast

Open a video or picture, pull down the media-page pulley menu, then select a
remembered Chromecast or choose **Scan for Chromecast**. Casting stays on the
same Player or Picture Viewer page.

The compact Cast overlay shows the active receiver and Cast-specific controls.
Normal video/picture navigation remains on the ordinary media controls.

Direct HTTP/HTTPS media is handed to Chromecast as its original URL. Local and
SMB media is served through a temporary LAN HTTP Range endpoint restricted to
the selected receiver.

QuickTime/MOV is not transcoded. MOV files are rejected before Chromecast LOAD
and SailVideo shows a clear unsupported-format state instead of starting broken
receiver playback.

See `docs/CHROMECAST.md` for implementation details.

## NAS discovery

SailVideo can scan the current IPv4 `/24` network for hosts accepting SMB on
TCP port 445. Discovery identifies possible SMB servers; share names are still
entered manually.

Manual NAS configuration remains available when discovery is unavailable or
cannot identify a server.

## Build

The repository vendors libsmb2 6.2 under `third_party/libsmb2`, so a normal
checkout contains the source required for an SMB-enabled build.

Build with the normal Sailfish SDK CMake/RPM workflow for a supported target
such as aarch64 or armv7hl.

A local-only developer build may be configured with
`-DSAILVIDEO_ALLOW_MISSING_LIBSMB2=ON`, but release and NAS testing builds
should use the bundled libsmb2 source.

Before updating the vendored libsmb2 source, run:

```sh
sh tools/verify-vendored-libsmb2.sh
```

## Sandboxing and credentials

The application uses Sailjail and requests the permissions required for media
indexing, local files, networking and Sailfish Secrets.

SMB passwords are stored through Sailfish Secrets rather than the JSON source
configuration. SailVideo should not log passwords or credential-bearing SMB
URLs.

## Testing

The current release checklist is in `docs/RELEASE_TESTING.md`.

At minimum, test:

- launcher and command-line startup;
- local H.264/AAC MP4 playback;
- pause/resume, seeking, rotation and cover controls;
- SMB browsing, playback and resume;
- wrong-password/missing-share handling;
- URL playback;
- picture browsing and slideshow;
- Chromecast video and picture workflows;
- mixed picture/video folder navigation;
- unsupported MOV Cast handling;
- cold-NAS resume from Recently played.

## Licensing

Original SailVideo code is licensed under GPL-3.0-or-later.

Bundled libsmb2 is licensed under LGPL-2.1-or-later. See:

- `LICENSES/LGPL-2.1-libsmb2.txt`
- `docs/BUNDLED_LIBSMB2.md`

The repository also retains BSD 3-Clause licence material and a reuse ledger for
LLs Video Player. No LLs Video Player source code has been incorporated through
SailVideo 1.1.0.12. See:

- `LICENSES/BSD-3-Clause-LLs.txt`
- `THIRD_PARTY_NOTICES.md`
- `docs/LLS_CODE_REUSE.md`

## SailVideo 1.1.0.6 — media volume and NAS navigation

- Playback now uses Sailfish's normal `x-maemo` media-volume role and no longer
  changes the PulseAudio default-sink volume from SailVideo's app control.
- The app-level QtMultimedia volume is reapplied after playback enters
  PlayingState, keeping it separate from the system media-volume master.
- NAS subfolders show a relative path below the source title, for example
  `..edp17/sailvideo`.
- The NAS browser remembers each visited folder's scroll position and restores
  it when returning to that folder.

## SailVideo 1.1.0.9 — SailVideo 1.1 release

SailVideo 1.1 adds native Google Chromecast support while keeping the normal
Player and Picture Viewer as the primary controls. Remembered receivers can be
selected from the media-page pulley menu; HTTP/HTTPS media is sent directly,
while local and SMB/NAS media is exposed through SailVideo's LAN HTTP Range
bridge.

The Cast workflow supports video play/pause, seeking, restart, skip,
previous/next, receiver volume and mute, Stop/Continue and disconnect. NAS
pictures can be displayed on Chromecast with previous/next and configurable
slideshows, and mixed picture/video folders can move naturally between media
types.

The release also improves Sailfish media-volume integration, cold NAS playback,
SMB directory retries, Range-seek robustness, NAS breadcrumbs and scroll
restoration, saved NAS source boundaries, portrait Cast controls and pulley-menu
behaviour. Unsupported MOV files are rejected before Chromecast LOAD instead of
starting broken receiver playback.

## SailVideo 1.1.0.11 — Chromecast stability release

SailVideo 1.1.0.11 promotes the tested Chromecast hotfix into the normal 1.1
release line.

- Fixes Chromecast discovery on a fresh installation with no remembered device.
- Keeps long local/SMB Cast transfers from monopolising the Qt event loop used
  by Cast control and heartbeat traffic.
- Automatically retries and rejoins the Cast control channel after transient
  receiver disconnects.
- Preserves the media already playing on the TV instead of sending a duplicate
  LOAD during reconnect.
- Keeps the local/SMB LAN HTTP bridge available while the Cast sender reconnects.
