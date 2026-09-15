# Playback history and last-played video

SailVideo stores recently played videos in:

```text
$HOME/.local/share/org.edp17/SailVideo/playback-history.json
```

This path deliberately matches the Sailjail app-data directory generated from:

```ini
OrganizationName=org.edp17
ApplicationName=SailVideo
```

The history file stores:

- source URL;
- display title;
- last known duration;
- last resume position;
- last played timestamp.

For SMB videos, the history file stores an `smb://` source URL but no password.
SMB passwords are stored separately through Sailfish Secrets when the user enables
"Remember password".

On application startup, the latest history entry is restored as Current video.
Playback does not start automatically; the user taps Current video to resume.


## Phase 6 r2

The history path now uses `QStandardPaths::GenericDataLocation` plus
`OrganizationName/ApplicationName`, matching the NAS source store that was
verified under Sailjail. Earlier JSON locations are migrated when visible.

Remembered SMB entries store only the `smb://` location, title, duration and
resume position. The stream bridge resolves the current SMB file size with
`libsmb2` before playback, so the video can resume without first browsing the
folder.


## Phase 6 r3

Playback history is now constructed with the exact storage directory used by
the NAS source model. The Settings page displays both files so they can be
checked together on-device.

SMB history entries also persist `sourceSize`, which lets the stream bridge
construct seekable HTTP Range responses immediately after app restart.


## Phase 6 r4

The QML layer no longer depends on calling `updateSourceSize()` on the history
model. The file size is treated as an optional optimization only. If no stored
size is available, the SMB stream bridge resolves it internally before playback.
