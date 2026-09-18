# SailVideo 1.1 release testing checklist

This is the minimum device checklist before publishing SailVideo 1.1
(package version 1.1.0.7).

## Startup and Sailjail

- Start from the launcher icon.
- Start from the command line.
- Confirm the local media library is populated.
- Confirm NAS sources and Settings survive restart.
- Confirm there are no Sailjail permission regressions.

## Local video

Use a known-good H.264/AAC MP4.

- Play from the Local library.
- Pause and resume.
- Seek with the timeline.
- Test skip back/forward.
- Test previous/next within a local folder.
- Test restart.
- Test Fit, Crop and Stretch.
- Test left-side brightness and right-side volume gestures.
- Rotate during playback.
- Test cover play/pause.
- Leave the player and reopen the last video.
- Confirm resume position is retained.

## Settings

- Change skip interval.
- Change picture slideshow interval.
- Change new-video volume and brightness.
- Toggle keep-display-on.
- Toggle NAS media preference and hidden-file visibility.
- Restart and confirm settings persist.
- Reset playback/browser settings and verify the documented defaults.

## HTTP/HTTPS

- Add a saved URL source.
- Open a one-off URL without saving.
- Confirm a valid media URL downloads/opens and plays.
- Confirm the original URL, not a cache path, appears in Recently played.
- Restart and confirm saved URL sources remain.

## SMB/NAS

- Open a saved guest source.
- Open an authenticated source using a remembered Sailfish Secrets password.
- Test a wrong password.
- Test an unreachable server.
- Test a missing share and missing file.
- Browse folders and use the `..` parent row.
- Long-press a folder and save it as another NAS source.
- Test media filtering and Name/Type/Size sorting.
- Play an SMB video.
- Seek near the beginning, middle and end of a large video.
- Leave the app/NAS idle, restart SailVideo and tap the NAS entry under
  Recently played once. Confirm it resumes without needing a second tap.

## NAS pictures

- Open JPEG/PNG pictures from an SMB folder.
- Test previous/next.
- Test slideshow.
- Change slideshow interval in Settings and confirm it affects the slideshow.
- Test rotation.

## Chromecast discovery

- Open a video or picture.
- Pull down the media-page menu.
- Confirm remembered receivers appear.
- Run Scan for Chromecast.
- Confirm the known receiver refreshes without creating duplicates.
- Test an unavailable/stale receiver and confirm a clear connection error.

## Chromecast video

- Cast a known-good video without opening a separate Cast control page.
- Confirm playback starts near the phone position.
- Confirm initial receiver volume follows SailVideo and is not forced to 100%.
- Test play/pause, seek, restart and skip.
- Test Mute/Unmute.
- Test Stop/Continue and confirm position is retained.
- Test Previous/Next.
- Test portrait and landscape Cast controls.
- Pull down the media-page menu while Cast controls are visible; underlying
  controls should dim and the menu must stay open beyond the normal control
  timeout.
- Test Disconnect and confirm phone playback resumes near the remote position.
- Test Leave playing on TV with a direct HTTP/HTTPS source.

## Chromecast pictures and mixed folders

- Cast an SMB picture.
- Test previous/next and slideshow.
- Start with video on the TV, open a picture and use Cast picture.
- Start with a picture on the TV, open a video and use Cast video.
- In a mixed folder, navigate picture -> video -> picture.
- Confirm slideshow skips videos and remains picture-only.

## Unsupported MOV Cast

- Attempt to cast a known problematic `.mov`.
- Confirm the phone shows the black unsupported-video state.
- Confirm the receiver does not start black/garbled playback.
- In a mixed folder, confirm the receiver keeps the previous valid media.
- Press Next again and confirm navigation continues past the MOV.

## Main page and rejoin

- Confirm Recently played shows at most three entries.
- Confirm Last browsed NAS opens the remembered folder.
- While casting, return to Main and confirm the Now casting card returns to the
  active media workflow.
- For a direct HTTP/HTTPS Cast, use Leave playing on TV, restart SailVideo and
  confirm rejoin works where the receiver session remains active.

## Error presentation

- Open an unsupported or damaged local/NAS file and confirm a readable playback
  error is shown.
- Confirm errors never expose saved passwords or credential-bearing SMB URLs.

## Packaging

- Perform a clean RPM build.
- Confirm there are no unpackaged-file errors.
- Confirm libsmb2 is installed only under SailVideo's app-private lib directory.
- Confirm no `.qml.orig`, generated `libsmb2.so*`, `smb2_autogen`, generated
  `config.h` or generated CMake config files are present in the source package.
- Confirm SMB passwords are not written to JSON configuration files.

## 1.1.0.7 NAS/seek stability checks

- Enter nested NAS folders and confirm the relative path is shown as the
  PageHeader description, aligned with the title.
- Enter and leave folders repeatedly; the file list must not jump vertically
  while the request starts.
- If the NAS produces a transient first connect/open failure, SailVideo should
  retry once automatically and only show an error if the retry also fails.
- Seek repeatedly in a large SMB video, including near the beginning, middle
  and end. Confirm playback resumes and the app remains stable.
