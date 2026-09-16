# SailVideo release testing checklist

This checklist describes the minimum device testing expected before tagging a
public SailVideo release.

## Startup and sandboxing

- Start from launcher icon.
- Start with `sailjail /usr/bin/harbour-sailvideo`.
- Confirm local video picker is populated.
- Confirm Settings shows storage paths under `org.edp17/SailVideo`.

## Local playback

- Play a known-good H.264/AAC MP4 file.
- Pause and resume.
- Seek with the timeline.
- Seek backward/forward using the configured skip interval.
- Rotate while playing.
- Go back to the main page and confirm audio stops.
- Reopen Current video and confirm video returns, not a black screen.

## HTTP bridge

- Open a local file through the HTTP bridge.
- Seek near the beginning, middle and end.
- Let the video finish naturally.

## SMB/NAS

- Browse a saved NAS source.
- Confirm saved password works after app restart.
- Confirm source survives app restart.
- Play a NAS video.
- Seek near the beginning, middle and end.
- Close and reopen the app, then resume Current video from NAS.
- Test Forget password from the NAS source context menu.
- Test wrong password handling.
- Test missing share or unreachable host.

## Settings

- Change video scaling to Fit, Fill/crop and Stretch.
- Change skip interval and confirm the player buttons use it.
- Toggle keep-display-on.
- Toggle NAS video-only filtering.
- Toggle hidden-file visibility if test data is available.
- Clear recent videos.
- Reset settings.

## Packaging

- Verify `libsmb2.so*` is packaged under `/usr/lib*/harbour-sailvideo/`.
- Confirm there are no unpackaged-file errors from RPM.
- Confirm no passwords are written to JSON files.


## Phase 7 r2 NAS browser filter regression test

1. Enable the NAS browser video-file preference.
2. Browse an SMB folder that contains at least one known playable video.
3. Confirm folders are still visible.
4. Confirm the folder never appears empty merely because the filter cannot
   recognise the file extension.
5. Use "Show all files" if the filter hides every entry.
6. Tap the previously hidden file and confirm playback still works.


## Phase 8 r1 functional tests

1. Open the revamped main page and confirm Continue watching, Open media and
   Recently played are readable.
2. Open NAS sources -> Search local network. Confirm the scan finishes.
3. If your NAS is found, tap it and confirm the Add SMB source page opens with
   the host pre-filled.
4. Browse a NAS folder containing videos and pictures.
5. Tap a picture file and confirm the picture viewer opens instead of the video
   player flashing briefly.
6. Tap a video from a folder with multiple videos.
7. Confirm the player shows separate previous-video, skip-back, play/pause,
   skip-forward and next-video controls.
8. Confirm skip buttons use the configured skip interval.
9. Confirm next/previous video moves through videos in the current SMB folder.
10. Confirm local playback, SMB resume and remembered password still work.


## Phase 8 r2 regression tests

1. Save an SMB source for `public`.
2. Save a second SMB source for `public/Shared Videos` by using Share `public`
   and Folder `Shared Videos`.
3. Restart SailVideo and confirm both saved entries remain visible.
4. Open each entry and confirm it starts in the correct folder.
5. Browse a folder with image files and tap an image.
6. Confirm the picture viewer opens immediately without using the context menu.
7. Use previous/next picture controls.
8. Start slideshow and confirm it advances through the pictures.
9. Browse a folder with multiple videos and confirm previous/next video controls
   still work separately from skip-back/skip-forward.


## Phase 8 r3 network discovery regression test

1. Open NAS sources.
2. Tap Find NAS or Search local network.
3. Confirm the page does not log `ReferenceError: networkDiscovery is not defined`.
4. Confirm the page either scans the local network or reports that no LAN
   interface / no SMB servers were found.
5. If a NAS is found, tap it and confirm Add SMB source opens with the host
   pre-filled.


## Phase 8 r4 NAS browser usability tests

1. Browse an SMB subfolder and confirm a `..` entry appears at the top.
2. Tap `..` and confirm the browser goes up one folder.
3. Tap the visible Up button and confirm it does the same.
4. Long-press a folder and choose Save folder as source.
5. Return to NAS sources and confirm the folder was saved as a separate source.
6. Open the saved folder source and confirm it starts inside that folder.
7. Edit or add a source with SMB path `/public/Shared Videos`.
8. Confirm it saves as share `public` and folder `Shared Videos`.
9. Enable Prefer media files and confirm both videos and pictures remain visible.


## Phase 8 r5 SMB browser regression tests

1. Open an authenticated SMB source with a remembered password and confirm no
   password field or remember-password toggle is shown on the browser page.
2. Open an authenticated SMB source without a remembered password and confirm a
   password dialog appears when browsing is required.
3. Browse into a subfolder and confirm `..` appears at the top.
4. Confirm there are no permanent Up or Save folder buttons in the browser body.
5. Long-press a folder and save it as a source.
6. Quickly tap through folders or refresh while loading and confirm
   `Another SMB request is still running` is not shown.
7. Test Sort by name, Sort by type, Sort by size and reverse sorting.
8. Confirm pictures still open in the picture viewer and videos still play.


## Phase 8 r5b crash regression test

1. Open a saved SMB source.
2. While a folder is loading, tap a video or picture from the currently visible
   list if one is visible.
3. Confirm SailVideo waits instead of crashing.
4. After the folder request finishes, tap a video.
5. Confirm the video opens and plays.
6. Return to the main page and confirm the last-played NAS video still resumes.


## Phase 8 r6 tests

1. Open an SMB source. If a folder listing error appears, tap Retry and confirm
   it starts the same folder request again.
2. Confirm the SMB browser pulley contains Refresh only for browser actions.
3. Use the on-page Media/All, Name/Type/Size and A→Z/Z→A controls.
4. Play a large SMB video and seek with one tap on the seek bar. Confirm the app
   does not crash.
5. On the player page, use restart, scaling, right-side volume swipe, and
   left-side visual brightness swipe.
6. Return to the main page and confirm Recently played shows only three items.
7. Browse a NAS folder, return to main, and confirm Last browsed NAS folder is
   shown and opens the same folder.


## Phase 8 r6a stability tests

1. Open Settings.
2. Tap each row several times: Skip interval, Keep display on, Prefer media
   files, Show hidden files.
3. Confirm the app does not crash.
4. Navigate repeatedly between Main, Settings, NAS sources and one NAS folder.
5. Confirm the app does not randomly close.
6. Play a video and reduce playback volume to 0%.
7. Confirm the video becomes silent.
8. Increase playback volume again and confirm sound returns.
9. Swipe on the left side and confirm the app changes visual video brightness.
10. Seek once in a large SMB video and confirm the app does not crash.


## Phase 8 r7 tests

1. Open Settings and confirm the toggles look like normal Sailfish controls.
2. Tap each Settings control several times and confirm the app does not crash.
3. Set system media volume above zero, play a video, then swipe right side down
   until app playback volume reaches 0%.
4. Confirm the video is silent at app volume 0%.
5. Swipe up on the right side and confirm sound returns.
6. Open NAS sources and confirm the Find NAS/Add source body buttons are gone.
7. Open URL sources and confirm the Add/Open body buttons are gone.
8. Confirm the Main page shows Last browsed NAS as a section and uses the source
   name rather than the full network path.
9. Add or edit an SMB source with username/password and Save and browse.
10. Confirm the next page opens without asking for the password again.
11. On the SMB folder page, confirm the full network path and password-saved
    status are no longer shown.
12. Confirm the All/Media, Name/Type/Size and A-Z/Z-A controls are visibly
    tappable with small icons.


## Phase 8 r8 tests

1. Open Add SMB source and confirm only pulley Save / Save and browse actions
   remain.
2. Confirm the password field has the inline Sailfish show/hide affordance.
3. Open an SMB folder and confirm the filter/sort/order icons align with the
   label text.
4. Set system media volume above zero, play a video, then swipe on the right
   half down to 0%. Confirm the device becomes silent.
5. Swipe up on the right half and confirm sound returns without noticeable UI
   blocking.
6. Tap the player scaling control and confirm Fit, Crop and Stretch visibly
   change the VideoOutput geometry.


## Phase 8 r8a player page load regression test

1. Start SailVideo.
2. Open any local or SMB video.
3. Confirm the player page loads instead of showing "Could not load page".
4. Confirm the console no longer reports duplicate `Component.onCompleted`.
5. Confirm the controls still hide after opening the player page.
6. Confirm Fit/Crop/Stretch still changes the visible video geometry.


## Phase 8 r8b player tests

1. Start SailVideo and play a local video.
2. Swipe right side down until Media volume reaches 0%.
3. Confirm playback continues; it must not pause or show "Muted; playback paused".
4. If `pactl` is available on the device, confirm the sound is silent at 0%.
5. Swipe up and confirm sound returns.
6. Tap the scaling control through Fit, Crop and Stretch.
7. Confirm the visible video geometry changes.
8. Repeat scaling with a video whose aspect ratio differs from the screen.
9. Repeat with an SMB video.


## Phase 8 r8c scaling tests

1. Play a video whose aspect ratio differs from the screen.
2. Select Fit and confirm the whole frame is visible; black bars are expected.
3. Select Crop and confirm the screen is filled; left/right or top/bottom edges
   may be clipped depending on video/screen aspect ratio.
4. Select Stretch and confirm the screen is filled without black bars; the image
   may be distorted.
5. Rotate the device and repeat Fit/Crop/Stretch.


## Phase 8 r8d scaling tests

1. Play the same test video in portrait.
2. Cycle Fit, Crop and Stretch.
3. Rotate to landscape.
4. Cycle Fit, Crop and Stretch again.
5. Confirm Stretch fills the player surface vertically and horizontally.
6. If bars remain in Stretch, test another video to check whether the bars are
   encoded into the movie frame itself.


## Phase 8 r8e player-scaling regression tests

1. Play a video.
2. Cycle Fit, Crop and Stretch.
3. Confirm the player never turns black after toggling scale.
4. Confirm Crop still fills the screen.
5. Confirm Fit still shows the whole frame.
6. Check Stretch again in portrait and landscape.


## Phase 8 r8f scaling tests

1. Play a video.
2. Cycle Fit, Crop and Stretch in portrait.
3. Rotate to landscape.
4. Cycle Fit, Crop and Stretch again.
5. Confirm toggling scale never turns the player black.
6. Confirm Crop still fills the screen.
7. Confirm Stretch attempts to fill the whole screen without preserving aspect.
8. If Stretch still leaves bars, compare with Crop to determine whether those
   bars are encoded in the movie frame or caused by the native video surface.


## Phase 8 r9 page-polish tests

1. Open the main page pulley menu and confirm Clear recent videos is gone.
2. Confirm Settings and About still appear in the main page pulley menu.
3. Confirm the current/last video appears under "Last played video".
4. Confirm "Last browsed NAS" remains a separate section.
5. Open Settings and confirm "Forget saved NAS passwords" is no longer there.
6. Open NAS sources and confirm the pulley menu contains "Forget saved NAS
   passwords".
7. Confirm Reset settings text says what will and will not be reset.
8. Confirm normal video playback and scale switching still work.


## Phase 8 r10 player gesture guide tests

1. Play a video.
2. Tap the player once to show controls.
3. Confirm the left half shows Brightness with up/down arrows.
4. Confirm the right half shows Volume with up/down arrows.
5. Confirm the hints disappear when controls auto-hide.
6. Swipe on the left half and confirm video brightness/dimming still changes.
7. Swipe on the right half and confirm volume still changes.
8. Confirm the overlay does not block play/pause, scaling, skip or seek controls.


## Phase 8 r11 URL/default-adjustment tests

1. Open Settings.
2. Confirm New video volume defaults to 30%.
3. Confirm New video brightness defaults to 50%.
4. Change each value, restart the app, and confirm the values persist.
5. Start a new local video and confirm the player starts at those defaults.
6. Start a new SMB video and confirm the player starts at those defaults.
7. Add/open a small HTTP/HTTPS MP4 URL.
8. Confirm the player shows "Downloading URL" progress first.
9. Confirm the downloaded URL then plays through the player without crashing.
10. Confirm the original URL appears in Recently played, not the cache path.


## Phase 8 r11a regression tests

1. Do a full clean rebuild and reinstall.
2. Open Settings and confirm New video volume is not undefined; expected default is 30%.
3. Confirm New video brightness is not undefined; expected default is 50%.
4. Add a URL source, restart SailVideo and confirm the source remains listed.
5. Open a URL source and confirm there is no `urlDownloadCache is not defined` warning.
6. Confirm PlayerPage no longer reports non-existent `onMetaDataChanged`.
7. Confirm URL playback either downloads and plays or shows a clear URL-cache error.


## Phase 8 r11b URL integration tests

1. Fully clean, rebuild and reinstall.
2. Start SailVideo from terminal.
3. Confirm terminal output contains:
   `SailVideo: URL download cache helper exposed`
4. Confirm terminal output contains the URL source storage path.
5. Open Settings and confirm New video volume/brightness do not show undefined%.
6. Add a URL source, close SailVideo, reopen it, and confirm it remains listed.
7. Play a small MP4 URL and confirm the player enters "Downloading URL" instead
   of "URL playback helper is unavailable".


## Phase 8 r12 local-video page tests

1. Open SailVideo from the launcher icon.
2. Tap Local on the main page.
3. Confirm the new Local videos list page opens.
4. Confirm videos appear as rows, not two-column square tiles.
5. Confirm search filters by video title or folder.
6. Pull down and tap Refresh; confirm scanning starts and finishes.
7. Pull down and tap System video picker; confirm the old picker remains
   available as a fallback.
8. Open a local video from the new list and confirm playback still works.
9. Repeat after rotating the device.


## Phase 8 r12a local-video index tests

1. Start SailVideo from terminal.
2. Tap Local.
3. Confirm the app-native Local videos page shows a one-column row list.
4. Confirm terminal output says entries were loaded from Tracker media index.
5. Confirm the System video picker is only in the pulley menu.
6. Search by a known video title.
7. Open a video from the row list.
8. Restart SailVideo and confirm Local still opens the app-native list first.


## Phase 8 r12b local-video regression test

1. Start SailVideo from the terminal and tap Local.
2. Confirm the page says "Loading videos from Sailfish media index…".
3. Confirm terminal output contains "querying Tracker video index".
4. Confirm it then reports the number of Tracker video URLs returned.
5. Confirm videos appear directly in SailVideo's one-column list.
6. Open a video from that list and verify playback.
7. Use "System picker (fallback)" only to compare the indexed content if needed.


## Phase 8 r13 local-video category tests

1. Open Main -> Local.
2. Confirm folders such as Camera/Videos/Downloads are shown as large spaced tiles.
3. Confirm each tile shows the number of videos it contains.
4. Tap a folder tile.
5. Confirm the next page shows the category title and total video count.
6. Confirm video tiles have visible spacing and do not touch.
7. Confirm each video shows its name, size and modified date/time.
8. Tap a video and confirm local playback starts normally.
9. Rotate both category and video-grid pages and confirm the grid reflows.


## Phase 8 r13c thumbnail/title tests

1. Open Main -> Local -> a video folder.
2. Confirm each video tile shows the cached video image used by Sailfish's
   thumbnailer instead of the plain play glyph.
3. Confirm the image remains compact and does not fill the whole tile.
4. Confirm a video without a generated thumbnail falls back to the play glyph.
5. Check long filenames and confirm the first character is fully visible.
6. Confirm longer titles can occupy two lines and then elide safely.
7. Tap the thumbnail/tile and confirm playback still starts.


## SailVideo 1.0 release check

Before publishing 1.0, verify launcher and command-line startup, local playback,
local category browsing, seeking, pause/resume, rotation, cover controls,
unsupported/missing-file errors, SMB browsing/playback/resume, saved credentials,
URL-source persistence/playback, player scaling, volume/brightness gestures and
NAS picture browsing.


## SailVideo 1.1 Chromecast first device test

Use a known-good video that already plays correctly in SailVideo.

1. Launch SailVideo from the icon and from the terminal.
2. Play a local video, tap Cast, and confirm the Chromecast appears.
3. Select the Chromecast and confirm the TV starts near the phone's current
   playback position.
4. Confirm the player page clearly identifies the active Cast device.
5. Test remote play/pause, seek bar, restart, skip backward and skip forward.
6. For an SMB folder with several videos, test previous/next while casting.
7. Test right-side volume swipe and mute from the Chromecast page.
8. Use `Disconnect and resume on phone`; confirm the Cast receiver closes on
   the TV and phone playback resumes near the remote position.
9. Start Cast again and use `Leave playing on TV`; confirm the TV continues.
10. Repeat with an HTTP/HTTPS source and confirm the receiver receives the
    original URL rather than a localhost URL.
11. Repeat with an SMB/NAS video; confirm terminal logs show a LAN bridge URL
    and HTTP GET/HEAD/Range requests from the Chromecast.
12. With SMB casting, seek near the beginning, middle and end of a large video.
13. Verify audio using a known-good H.264/AAC MP4 or another SailVideo/NAS video
    with confirmed audio.
14. Confirm ordinary local playback, NAS playback, URL playback, rotation and
    cover controls still work when Cast is not active.

Useful logs:

```text
SailVideo Cast:
SailVideo Cast bridge:
```


## SailVideo 1.1 Chromecast second device test

Run these after the first Chromecast milestone still passes:

1. Open Cast after a Chromecast has already been discovered. Confirm the
   remembered device appears immediately and a new scan does not start.
2. Use the pulley `Scan for Chromecast devices` action. Confirm the known
   receiver is refreshed and additional receivers would be added.
3. With the receiver deliberately unavailable, tap its remembered entry.
   Confirm a connection error and `Retry connection` action are shown rather
   than silently removing the device.
4. Cast a video, press `Stop media`, and confirm TV playback stops while the
   receiver remains connected. Confirm the button changes to `Start / continue`
   and resumes/reloads the same video without disconnecting first.
5. For a direct HTTP/HTTPS cast, choose `Leave playing on TV`, close SailVideo,
   reopen it, and confirm SailVideo opens the Chromecast page and rejoins the
   running receiver/media session without sending another LOAD.
6. Repeat the previous test while the Cast media is paused.
7. Confirm `Disconnect and resume on phone` still terminates the receiver
   session with receiver namespace STOP and resumes video near the remote
   position.
8. Open an SMB/NAS folder containing several videos from both the first/middle
   and last items. Confirm previous/next availability always corresponds to the
   actual neighbouring videos.
9. Repeat previous/next using the native Local videos folder/category page.
10. Open an SMB/NAS JPEG or PNG in the picture viewer, tap Cast, and confirm the
    picture is displayed on TV.
11. While displaying a picture, use previous and next. Confirm the TV follows
    each picture change.
12. Start the SailVideo picture slideshow and confirm successive pictures are
    also shown on TV.
13. Stop the displayed picture from the Chromecast page, then use
    `Start / continue` and confirm the same picture returns.
14. Disconnect picture casting and confirm the Cast receiver closes without
    trying to start video playback on the phone.
15. Re-test ordinary local/NAS/URL playback and local picture viewing with no
    Chromecast active.

For restart/rejoin testing, direct HTTP/HTTPS media is the authoritative case.
Local/SMB Cast media depends on SailVideo's LAN HTTP bridge, so closing the app
removes the server that the Chromecast is reading from.


## SailVideo 1.1.0.1 Chromecast workflow tests

### Video volume and stop/continue

1. Set SailVideo playback volume to a non-100 value such as 30%.
2. Start casting a known-good video.
3. Confirm Chromecast starts at approximately the same receiver volume and
   does not jump to 100%.
4. Let the video play for at least 30 seconds.
5. On the Chromecast page choose `Stop media`.
6. Confirm the receiver remains connected and `Start / continue` appears.
7. Choose `Start / continue`.
8. Confirm playback resumes near the stopped position rather than from 0:00.

### Picture controls and return workflow

1. Cast an SMB picture.
2. Confirm the Chromecast page does not show Mute, Stop media or Leave playing
   on TV.
3. Confirm `Return to picture` returns to the current picture viewer.
4. Go back to Main while the picture remains on TV.
5. Tap the `Now casting` card and confirm the current picture workflow opens
   again without browsing the folder or recasting.

### Folder workflow matrix

Test three SMB folders:

1. Video-only folder:
   - Cast a middle video.
   - Confirm previous/next follow all videos in the displayed folder order.

2. Picture-only folder:
   - Cast a middle picture.
   - Confirm previous/next follow all pictures.
   - Start slideshow; confirm it advances automatically and wraps at the end.
   - Navigate to Main/Cast page while slideshow runs and confirm TV continues
     to advance.

3. Mixed picture/video folder:
   - Start with a picture and use Next/Previous across a neighbouring video.
   - Start with a video and use Next/Previous across a neighbouring picture.
   - Confirm media type switches on the TV without reconnecting Chromecast.
   - Confirm picture slideshow skips videos and cycles only through pictures.
