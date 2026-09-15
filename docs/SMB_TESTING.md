# SMB/NAS testing checklist

Phase 4 r1 introduces manual SMB/NAS browsing and SMB file playback through the
local HTTP Range bridge.

## Important implementation detail

This revision loads `libsmb2` dynamically at runtime. It does not add a hard RPM
requirement and does not link to `libsmb2` during the Sailfish build. The expected
behaviour is:

- if `libsmb2` is present, NAS browsing and SMB playback are enabled;
- if `libsmb2` is missing, the app still starts and reports that the SMB backend
  is unavailable.

## Credential handling

Passwords are not saved in Phase 4 r1.

Saved NAS sources store:

- display name;
- host/IP;
- port;
- share;
- domain/workgroup;
- username;
- guest/non-guest flag.

The password is entered on the browser page and kept only in memory while the app
session is running. Sailfish Secrets should be added after the SMB path is proven
with at least one real NAS.

## Basic test

1. Launch SailVideo from the icon.
2. Open **NAS sources**.
3. Confirm the backend status line is visible.
4. Add an SMB source.
5. Enter host/IP, share name, port 445, and either guest access or username.
6. Save and browse.
7. For non-guest access, enter the NAS password on the browser page.
8. Confirm folders are listed.
9. Enter a subfolder.
10. Go up.
11. Tap an MP4/MKV/AVI file.
12. Confirm playback starts.
13. Seek near the beginning, middle and end.
14. Press Back from the player and confirm audio stops.
15. Tap Current video and confirm playback resumes.

## Failure cases to test

- wrong password;
- wrong host/IP;
- wrong share name;
- reachable share but missing/unsupported file;
- large video file;
- video where MP4 metadata is near the end;
- Wi-Fi interrupted during playback if practical.

## Logs to collect

Run the app outside Sailjail:

```bash
harbour-sailvideo
```

Run it inside Sailjail from the terminal:

```bash
sailjail /usr/bin/harbour-sailvideo
```

Also check whether a libsmb2 package or library is present:

```bash
rpm -qa | grep -i smb2
find /usr/lib /usr/lib64 -name 'libsmb2*' 2>/dev/null
```

Do not paste real NAS passwords into logs or issue reports.

## Phase 4 r2: bundled libsmb2 check

If the NAS browser reports that `libsmb2` is unavailable, populate the bundled
source before rebuilding:

```sh
sh tools/import-libsmb2.sh
```

During CMake configure, look for:

```text
Building bundled app-private libsmb2 from third_party/libsmb2
```

After installing the RPM on device, verify that an app-private library exists:

```sh
find /usr/lib /usr/lib64 -path '*harbour-sailvideo*' -name 'libsmb2*' -print 2>/dev/null
```

Then launch from the icon and open the NAS sources page. The status should say
that the SMB backend is available and should include the private SailVideo
library path. If the page still says unavailable, collect:

```sh
sailjail /usr/bin/harbour-sailvideo
find /usr/lib /usr/lib64 -path '*harbour-sailvideo*' -name 'libsmb2*' -print 2>/dev/null
ldd /usr/lib*/harbour-sailvideo/libsmb2.so 2>/dev/null
```


## Saved source persistence

From Phase 4 r5, saved NAS sources are stored in SailVideo's app-private data
directory as `nas-sources.json`. Passwords are not stored.

After adding a NAS source:

1. close SailVideo;
2. start it again from the launcher icon;
3. open NAS sources;
4. confirm the saved source is still listed;
5. browse it and enter the password again if required.


## Sailjail persistence check

From Phase 4 r6, the NAS sources page shows the exact JSON storage path. It
should be:

```text
/home/defaultuser/.local/share/org.edp17/SailVideo/nas-sources.json
```

or the equivalent path for the current Sailfish user.

To verify persistence:

```sh
sailjail /usr/bin/harbour-sailvideo
```

Add a NAS source, close SailVideo, then check:

```sh
ls -l ~/.local/share/org.edp17/SailVideo/nas-sources.json
cat ~/.local/share/org.edp17/SailVideo/nas-sources.json
```


## Password persistence with Sailfish Secrets

From Phase 5 r1, the SMB browser page has a “Remember password” switch.
When it is enabled and browsing succeeds, the password is stored with Sailfish
Secrets in the `SailVideoSmbCredentials` collection.

Test sequence:

1. start SailVideo from the launcher icon;
2. open NAS sources;
3. browse a saved username/password SMB source;
4. enter the password and keep “Remember password” enabled;
5. confirm folder listing works;
6. close SailVideo;
7. start SailVideo again;
8. open the same NAS source;
9. confirm the password is loaded and browsing starts without retyping it.

If Sailfish Secrets shows a confirmation prompt, accept it for SailVideo.

## Last played video restore

After playing a NAS video, close SailVideo and start it again. The main page
should show the last played video under “Current video”. Tapping it should resume
near the saved position. For SMB videos this requires the saved NAS source and a
remembered password, unless the share uses guest access.


## Password storage test

Phase 5 r1 uses Sailfish Secrets to store SMB passwords. The application must
have the `Secrets` Sailjail permission. After installing this build, grant the
updated permission set when Sailfish asks.

1. Start SailVideo from the launcher icon.
2. Open NAS sources and browse a password-protected source.
3. Keep `Remember password` enabled.
4. Enter the password and browse successfully.
5. Close SailVideo completely.
6. Start SailVideo again from the launcher icon.
7. Open the same NAS source.
8. Confirm the password is loaded and browsing starts without retyping it.

The source metadata remains in `nas-sources.json`; the password should not be
present in that JSON file.


## Saved subfolders

From Phase 8 r2, a saved NAS source may point either to the root of a share or to
a start folder inside the share. Example:

```text
Server: 192.168.1.10
Share: public
Folder in share: Shared Videos
```

This opens `//192.168.1.10/public/Shared Videos` while keeping the SMB share name
as `public`. Several folders from the same share can be saved separately.


## Phase 8 r4 SMB browsing workflow

The SMB browser behaves more like a file manager:

- subfolders contain a visible `..` row for parent-folder navigation;
- the current folder can be saved as a NAS source from the top buttons or pulley;
- long-pressing a folder offers Save folder as source and Edit as source;
- Add/Edit SMB source uses one SMB path field where the first segment is the
  share name and remaining segments are the starting folder.

Examples:

```text
/public
/public/Shared Videos
public/Shared Videos/Films
```

A bare `/` is reserved for future SMB share enumeration and cannot be browsed yet.


## Phase 8 r5 SMB browser changes

The SMB browser should no longer show permanent password, remember-password, Up
or Save folder controls while browsing. Password entry is a separate dialog that
appears only when needed.

The visible `..` row is the primary way to go up one folder. Long-press a folder
to save it as a separate NAS source. Sorting by name, type or size is available
from the pulley menu.

Overlapping browse requests are queued. Quickly tapping folders or refreshing
while a browse is still running should no longer show:

```text
Another SMB request is still running.
```


## Phase 8 r5b SMB request safety

SMB folder browsing and SMB media streaming should not be started at the same
time from the QML browser. If the backend is still busy listing a directory,
video/picture opening is deferred until the listing finishes. This avoids the
Phase 8 r5a crash where last-played resume worked but selecting a new video from
the SMB browser could terminate the app.


## Phase 8 r6 browse and retry behaviour

SMB folder-load errors now leave the user on the same page and show a Retry
button below the error. The browser also stores the last successfully browsed NAS
folder in `settings.json`, so the main page can reopen it directly.

Sort and media-filter controls are visible on the SMB browser page instead of
being hidden in pulley-menu actions. Refresh remains in the pulley menu.
