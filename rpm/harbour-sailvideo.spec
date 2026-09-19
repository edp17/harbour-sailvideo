Name:       harbour-sailvideo
Summary:    Local and network video player for Sailfish OS
Version:    1.1.0.11
Release:    1
License:    GPL-3.0-or-later AND LGPL-2.1-or-later
URL:        https://github.com/edp17/harbour-sailvideo
Source0:    %{name}-%{version}.tar.bz2

BuildRequires: cmake
BuildRequires: pkgconfig(Qt5Core)
BuildRequires: pkgconfig(Qt5Gui)
BuildRequires: pkgconfig(Qt5Qml)
BuildRequires: pkgconfig(Qt5Quick)
BuildRequires: pkgconfig(Qt5Multimedia)
BuildRequires: pkgconfig(Qt5Network)
BuildRequires: pkgconfig(Qt5Concurrent)
BuildRequires: pkgconfig(sailfishapp)
BuildRequires: pkgconfig(sailfishsecrets)

Requires: sailfishsilica-qt5
Requires: sailfish-components-pickers-qt5
Requires: qt5-qtdeclarative-import-multimedia
Requires: nemo-qml-plugin-thumbnailer-qt5

%description
SailVideo is a native Sailfish OS video player designed for local files,
HTTP/HTTPS media URLs, SMB/NAS media, and Google Chromecast media casting.

%prep
%setup -q

%build
%cmake
%cmake_build

%install
%cmake_install

%files
%license LICENSE
%license LICENSES/BSD-3-Clause-LLs.txt
%license LICENSES/LGPL-2.1-libsmb2.txt
%doc README.md
%doc THIRD_PARTY_NOTICES.md
%doc docs/LLS_CODE_REUSE.md
%doc docs/NAS_BACKEND_PLAN.md
%doc docs/SMB_TESTING.md
%doc docs/BUNDLED_LIBSMB2.md
%doc docs/PLAYBACK_HISTORY.md
%doc docs/RELEASE_TESTING.md
%doc docs/NETWORK_DISCOVERY.md
%doc docs/CHROMECAST.md
%{_bindir}/harbour-sailvideo
%{_libdir}/harbour-sailvideo
%{_datadir}/applications/harbour-sailvideo.desktop
%{_datadir}/icons/hicolor/172x172/apps/harbour-sailvideo.png
%{_datadir}/harbour-sailvideo/qml

%changelog
* Sat Sep 19 2026 edp17 <edp17@pm.me> - 1.1.0.11-1
- Finalise the tested Chromecast hotfix as the polished SailVideo 1.1.0.11 release
- Fix Chromecast discovery on fresh installations with no remembered receiver
- Keep long local/SMB Cast streams responsive to Cast control and heartbeat traffic
- Automatically rejoin the Cast control channel after transient receiver disconnects
- Preserve active TV media and the local/SMB LAN bridge while the sender reconnects
- Refresh release metadata and third-party reuse statements

* Fri Sep 18 2026 edp17 <edp17@pm.me> - 1.1.0.10-1
- Fix Chromecast discovery on fresh installations with an empty receiver cache
- Keep long local/SMB Cast streams from starving Cast control processing
- Automatically rejoin the Cast control channel after transient receiver disconnects
- Preserve active TV media and the LAN bridge while the control channel reconnects

* Fri Sep 18 2026 edp17 <edp17@pm.me> - 1.1.0.9-1
- Release SailVideo 1.1 with native Google Chromecast support
- Integrate Cast discovery/device selection and controls into Player and Picture Viewer
- Cast HTTP/HTTPS directly and expose local/SMB media through the LAN HTTP Range bridge
- Add Cast transport, seek, skip, previous/next, volume/mute, stop/continue and disconnect
- Add Chromecast picture display, slideshow and mixed picture/video folder navigation
- Respect Sailfish system media volume and preserve receiver volume/mute state correctly
- Reject unsupported MOV Cast playback cleanly instead of starting broken receiver playback
- Improve cold SMB playback, transient folder retries and HTTP Range seek stability
- Add NAS breadcrumbs, scroll restoration and hard saved-source browsing roots
- Polish portrait Cast controls and pulley-menu interaction
- Remove obsolete Cast page, tracked QML backups and generated libsmb2 build artifacts
- Finalise SailVideo 1.1 release documentation and legal/reuse metadata

* Fri Sep 18 2026 edp17 <edp17@pm.me> - 1.1.0.8-1
- Treat each saved NAS folder as a hard browsing root
- Hide the Parent folder entry at the saved NAS source root
- Preserve deepest folder names by left-eliding long relative paths
- Keep Last browsed and password-resume browsing inside the correct saved root
- Resolve multiple saved folders on one SMB share by the longest matching path

* Fri Sep 18 2026 edp17 <edp17@pm.me> - 1.1.0.7-1
- Use the native PageHeader description for the current NAS subfolder path
- Keep NAS folder layout stable while directory requests are in progress
- Retry one failed fresh SMB folder connect/open before showing an error
- Harden seek target validation and abandon stale HTTP Range transfers promptly

* Fri Sep 18 2026 edp17 <edp17@pm.me> - 1.1.0.6-1
- Route playback through Sailfish's x-maemo media-volume policy
- Stop changing the global PulseAudio default-sink volume from the player
- Reapply app-level QtMultimedia volume after playback starts
- Show the current relative path under the NAS source title
- Restore each NAS folder's scroll position when navigating back

* Thu Sep 17 2026 edp17 <edp17@protonmail.com> - 1.1.0.5-1
- Keep Player and Picture Viewer pulley menus above Chromecast controls
- Use portrait-safe Chromecast control geometry
- Show a black unsupported-video placeholder for blocked MOV Cast items
- Keep mixed-folder navigation moving past unsupported Cast videos
- Dim playback/Cast controls while a pulley menu is open
- Keep player controls visible for the full lifetime of an open pulley menu
- Finalise release UI text and current Chromecast documentation
- Remove obsolete Cast-page/back-up QML and generated libsmb2 build artifacts

* Thu Sep 17 2026 edp17 <edp17@protonmail.com> - 1.1.0.4-1
- Move Chromecast selection and controls onto the video/picture pages
- Keep Settings directly accessible from media-page pulley menus
- Reject MOV before Chromecast LOAD with an unsupported-format message
- Open SMB streams before HTTP success and retry one cold NAS open automatically

* Wed Sep 16 2026 edp17 <edp17@protonmail.com> - 1.1.0.3-1
- Clear and confirm Chromecast mute state when handing a new video to the receiver
- Add persistent 3/5/10/15/30 second picture slideshow interval setting
- Use the slideshow interval for both local and Chromecast picture slideshows
- Advertise MOV files as MP4-family media for Chromecast-compatible MOV content

* Wed Sep 16 2026 edp17 <edp17@protonmail.com> - 1.1.0.2-1
- Separate requested Cast source from receiver-confirmed media state
- Add explicit Cast current video / Cast current picture handover actions
- Keep Cast Previous/Next controls in a stable position across picture/video transitions
- Keep local video controls local while a picture remains on Chromecast
- Wait for receiver volume confirmation before sending the initial video LOAD
- Preserve mixed-folder and picture-slideshow behaviour across media-type switches

* Wed Sep 16 2026 edp17 <edp17@protonmail.com> - 1.1.0.1-1
- Start Chromecast video playback at SailVideo's current volume instead of receiver maximum volume
- Preserve the exact remote position across Stop media -> Start / continue
- Simplify picture-casting controls by hiding video-only mute/stop/detach actions
- Add Return to picture/player workflow from Chromecast and Main pages
- Add unified SMB folder media navigation so Cast previous/next traverses mixed picture/video folders
- Add Cast picture slideshow that continues while navigating away from the picture page
- Keep picture slideshows image-only while manual Cast previous/next follows the complete folder media order
- Improve Cast workflow state handling and diagnostics

* Wed Sep 16 2026 edp17 <edp17@protonmail.com> - 1.1-1
- Add native Google Chromecast discovery using mDNS
- Add Cast V2 TLS sender with built-in protobuf envelope framing
- Launch and control Google Default Media Receiver without external protobuf/Avahi dependencies
- Cast direct HTTP/HTTPS sources and expose local/SMB media through a LAN HTTP Range bridge
- Reuse the SailVideo player UI for remote play/pause, seek, skip, volume and queue controls
- Add Cast device/status UI, proper receiver-session disconnect and remote position tracking
- Remember discovered Chromecast devices and provide explicit refresh/retry actions
- Add stop/continue without requiring a disconnect and recast
- Rejoin an ongoing Default Media Receiver session after SailVideo restarts where possible
- Add Chromecast display for SMB/NAS pictures, including previous/next and slideshow changes
- Improve local/SMB previous-next queue matching
- Keep direct Cast playback running when SailVideo detaches/exits where the receiver can access the source independently
- Add Chromecast diagnostics and device-test documentation

* Tue Sep 15 2026 edp17 <edp17@protonmail.com> - 1.0-1
- First public SailVideo release
- Add native local-video library grouped by folder with Sailfish thumbnails
- Support local playback with resume positions and recent history
- Add SMB2/SMB3 NAS source management, browsing and seekable playback
- Store SMB passwords through Sailfish Secrets
- Add persistent HTTP/HTTPS video sources
- Add Fit, Crop and Stretch scaling plus brightness and volume swipe controls
- Add configurable playback defaults, skip interval and keep-display-on support
- Add NAS picture browsing, navigation and slideshow
- Finalise About and third-party licence pages for release

* Tue Sep 15 2026 edp17 <edp17@protonmail.com> - 0.9.31-1
- Use Sailfish/Nemo cached video thumbnails on local video tiles
- Keep thumbnails compact at the previous play-icon footprint
- Keep a play-icon fallback if thumbnail generation fails
- Allow two-line video titles with safe horizontal margins

* Tue Sep 15 2026 edp17 <edp17@protonmail.com> - 0.9.30-1
- Fix LocalVideoModel::entries linker failure by making the accessor inline
- Remove the out-of-line entries() implementation
- Preserve the r13 category/folder video browser design

* Tue Sep 15 2026 edp17 <edp17@protonmail.com> - 0.9.29-1
- Fix LocalVideoModel header to expose the category metadata added in r13
- Add folderPath, folderName and sizeText fields to LocalVideoModel::Entry
- Expose read-only entries() access for local category/folder projection models
- Fix r13 category-view build failure

* Tue Sep 15 2026 edp17 <edp17@protonmail.com> - 0.9.28-1
- Redesign Local videos around folder/category tiles
- Show video count on each local category tile
- Add a dedicated category page with spaced video tiles
- Show video name, size and modified date/time below each video tile

* Tue Sep 15 2026 edp17 <edp17@protonmail.com> - 0.9.27-1
- Fix Local videos Tracker query to use org.freedesktop.Tracker3.Miner.Files
- Query the Tracker video graph instead of an unspecified local database
- Add Tracker query diagnostics and shorter timeout handling
- Keep filesystem scan as a limited fallback only
- Clarify that SailVideo's row list is the primary local-video UI

* Mon Sep 14 2026 edp17 <edp17@protonmail.com> - 0.9.26-1
- Use Tracker/MediaIndexing as the primary source for the redesigned Local videos page
- Keep filesystem scanning only as a fallback when Tracker is unavailable or empty
- Clarify that the system video picker is a fallback, not the intended local-video UI

* Mon Sep 14 2026 edp17 <edp17@protonmail.com> - 0.9.25-1
- Replace the default two-column local video picker with an app-native local video list page
- Add asynchronous local video scanning for Videos, Downloads, Documents and removable media
- Add local video search, file details and a system picker fallback in the local videos page

* Mon Sep 14 2026 edp17 <edp17@protonmail.com> - 0.9.24-1
- Fix UrlDownloadCache header so the new C++ helper is reliably compiled
- Add startup diagnostics confirming URL helper exposure and URL source storage path
- Avoid raw QML references to urlDownloadCache when the binary/QML installation is mismatched
- Harden Settings percentage labels against old-binary/new-QML mixtures

* Mon Sep 14 2026 edp17 <edp17@protonmail.com> - 0.9.23-1
- Fix undefined default volume/brightness values in QML
- Guard URL cache QML connections when the helper is unavailable
- Persist URL sources in SailVideo app-data JSON instead of QSettings
- Remove invalid PlayerPage onMetaDataChanged handler
- Add clearer URL helper error if the installed binary is stale

* Mon Sep 14 2026 edp17 <edp17@protonmail.com> - 0.9.22-1
- Add configurable new-video default volume and brightness
- Apply default volume 30% and brightness 50% whenever a new video starts
- Cache HTTP/HTTPS URL sources to app storage before playback
- Play cached URL downloads through the existing local HTTP Range bridge
- Avoid handing remote HTTPS URLs directly to QMediaPlayer after observed native playback crash

* Mon Sep 14 2026 edp17 <edp17@protonmail.com> - 0.9.21-1
- Add player gesture guide overlays for brightness and volume
- Show left/right coloured hint zones with up/down arrows while player controls are visible

* Mon Sep 14 2026 edp17 <edp17@protonmail.com> - 0.9.20-1
- Remove Clear recent videos from the main page pulley menu
- Move the top continue card into a Last played video section
- Keep Last browsed NAS as a neighbouring main-page section
- Move Forget saved NAS passwords from Settings to the NAS sources pulley menu
- Clarify the Show hidden files setting description
- Clarify exactly what Reset playback/browser settings changes

* Mon Sep 14 2026 edp17 <edp17@protonmail.com> - 0.9.19-1
- Restore working Crop behaviour without destroying/recreating VideoOutput
- Implement Fit/Crop/Stretch by transforming a QML wrapper around the persistent VideoOutput
- Avoid direct VideoOutput transform and avoid Loader-based surface recreation
- Keep VideoOutput stable to prevent black player surface after scale changes

* Mon Sep 14 2026 edp17 <edp17@protonmail.com> - 0.9.18-1
- Fix black player surface after changing scaling mode
- Stop destroying/recreating VideoOutput while QMediaPlayer is already playing
- Return scaling changes to a persistent VideoOutput item with safe fillMode refresh

* Mon Sep 14 2026 edp17 <edp17@protonmail.com> - 0.9.17-1
- Fix landscape Stretch mode by recreating VideoOutput when scale mode changes
- Stop using QML Scale transforms for video scaling because the native surface can ignore vertical transform
- Return player scaling to explicit VideoOutput fill modes applied at item creation

* Mon Sep 14 2026 edp17 <edp17@protonmail.com> - 0.9.16-1
- Rework player scaling again so Stretch uses a non-uniform transform
- Keep the raw VideoOutput rectangle at video aspect ratio to avoid backend letterboxing
- Make Fit/Crop/Stretch behaviour explicit in player geometry code

* Mon Sep 14 2026 edp17 <edp17@protonmail.com> - 0.9.15-1
- Stop pausing playback when system volume control is unavailable
- Keep app/system mute attempts non-disruptive at media volume 0%
- Rework player scaling to use manual VideoOutput geometry for Fit/Crop/Stretch
- Avoid relying on dynamic VideoOutput.fillMode changes, which are ignored on some Sailfish multimedia stacks

* Mon Sep 14 2026 edp17 <edp17@protonmail.com> - 0.9.14-1
- Fix PlayerPage QML load failure caused by duplicate Component.onCompleted handlers
- Merge fill-mode initialisation into the existing player page completed handler
- Guard VideoOutput fill-mode refresh during page construction

* Mon Sep 14 2026 edp17 <edp17@protonmail.com> - 0.9.13-1
- Remove duplicate Save and Save and browse body buttons from Add/Edit SMB source
- Replace password show/hide switch with Sailfish PasswordField eye control
- Align SMB browser filter/sort/order icons with their text labels
- Make player right-side gesture drive system media volume through pactl when available
- Use non-blocking pactl calls to reduce volume gesture delay
- Force VideoOutput geometry refresh when changing scaling mode on the player page

* Mon Sep 14 2026 edp17 <edp17@protonmail.com> - 0.9.12-1
- Restore Sailfish-style Settings controls now that AppSettings saves are delayed
- Add SystemAudioController and PulseAudio mute safety net for app volume at 0%
- Remove top action buttons from NAS and URL source pages
- Show last browsed NAS as its own main-page section using source name
- Rework Edit SMB source credentials: username plus password with show/hide toggle
- Save SMB password directly from the Edit SMB source page when provided
- Remove full SMB path and password-saved status clutter from the SMB browser page
- Add clearer icons to SMB browser filter/sort/order controls

* Mon Sep 14 2026 edp17 <edp17@protonmail.com> - 0.9.11-1
- Make AppSettings::save() public so main.cpp can flush delayed settings on shutdown
- Fix build error caused by AppSettings::save() being private

* Mon Sep 14 2026 edp17 <edp17@protonmail.com> - 0.9.10-1
- Remove the out-of-line AppSettings destructor requirement
- Flush AppSettings on application shutdown through QGuiApplication::aboutToQuit
- Fix lingering linker failure for AppSettings::~AppSettings()

* Mon Sep 14 2026 edp17 <edp17@protonmail.com> - 0.9.9-1
- Add missing AppSettings destructor definition for the delayed settings-save timer
- Fix linker error: undefined reference to AppSettings::~AppSettings()

* Mon Sep 14 2026 edp17 <edp17@protonmail.com> - 0.9.8-1
- Stabilise Settings by replacing ComboBox/TextSwitch option handlers with simple rows
- Delay AppSettings JSON writes so settings changes do not write during Silica control animations
- Fix playback volume at 0% by also setting MediaPlayer.muted
- Rename player gestures to playback volume and video brightness to avoid implying system-wide changes
- Throttle seek bar requests more defensively for large SMB files

* Mon Sep 14 2026 edp17 <edp17@protonmail.com> - 0.9.7-1
- Add retry action for failed SMB folder loads
- Move SMB browser sorting and media filter controls onto the page
- Remember and show the last browsed NAS folder
- Limit the main page recent list to the latest three videos
- Add player restart, live scaling switch, volume gesture and visual brightness gesture
- Make seek-bar seeking safer by avoiding repeated seeks while dragging
- Refresh About, Main, NAS sources, Network sources and Network discovery page headers

* Sun Sep 13 2026 edp17 <edp17@protonmail.com> - 0.9.6-1
- Remove the C++ queued SMB browse request path that could race with new media playback
- Keep browse queuing in QML only and defer media opening until folder requests finish
- Prevent opening SMB video/picture streams while an SMB directory request is still active
- Fix crash when playing a new video from the SMB browser while last-played resume still worked

* Sun Sep 13 2026 edp17 <edp17@protonmail.com> - 0.9.5-1
- Declare the pending SMB browse-request state used by the queued browser logic
- Fix Phase 8 r5 build failure in SmbBackend

* Sun Sep 13 2026 edp17 <edp17@protonmail.com> - 0.9.4-1
- Queue overlapping SMB folder browse requests instead of showing an error
- Revamp the SMB browser to remove inline password and toolbar buttons
- Move SMB password entry into a separate dialog page
- Add folder sorting by name, type or size from the SMB browser pulley menu
- Keep folder save actions on long-press and pulley rather than as permanent buttons

* Sun Sep 13 2026 edp17 <edp17@protonmail.com> - 0.9.3-1
- Add visible parent-folder navigation row to the SMB browser
- Add Up and Save folder buttons in the SMB browser
- Allow long-pressing an SMB folder to save it as a separate NAS source
- Simplify Add/Edit SMB source to a single SMB path field
- Change NAS filtering from video-only preference to media-file preference

* Sat Sep 12 2026 edp17 <edp17@protonmail.com> - 0.9.2-1
- Register NetworkDiscoveryModel as a QML type
- Make NetworkDiscoveryPage use a fallback model if the context property is unavailable
- Fix ReferenceError: networkDiscovery is not defined on the Find NAS page

* Sat Sep 12 2026 edp17 <edp17@protonmail.com> - 0.9.1-1
- Add optional start folder to saved SMB/NAS sources
- Allow multiple saved entries for different folders in the same SMB share
- Route SMB image taps directly to the picture viewer and add picture browsing queue
- Add previous/next picture controls and simple slideshow mode
- Broaden still-image extension detection
- Refresh the main page layout with larger source cards and a clearer Continue card

* Sat Sep 12 2026 edp17 <edp17@protonmail.com> - 0.9.0-1
- Add local network scan for possible SMB/NAS servers on port 445
- Add picture viewer for SMB images served through the localhost bridge
- Add per-folder SMB video queue with previous/next video controls
- Replace skip previous/next icons with explicit skip labels
- Revamp the main page around Continue watching and media sources

* Sat Sep 12 2026 edp17 <edp17@protonmail.com> - 0.8.1-1
- Make NAS browser filtering non-destructive so playable files are not hidden
- Re-check video extensions in QML instead of relying only on QVariantMap flags
- Add fallback to show all visible SMB items if no video-only matches are found
- Broaden recognised video file extensions
- Add a Show all files button when filters hide every SMB entry

* Sat Sep 12 2026 edp17 <edp17@protonmail.com> - 0.8.0-1
- Add persistent Sailjail-safe application settings
- Add video scaling modes: fit, fill/crop and stretch
- Add configurable seek/skip interval
- Add settings for keep-display-on behaviour
- Add NAS browser filters for video files and hidden files
- Add saved-password forget actions for NAS sources
- Add release testing checklist

* Sat Sep 12 2026 edp17 <edp17@protonmail.com> - 0.7.4-1
- Fix missing NasSourceModel storageDirectory declaration
- Fix PlaybackHistoryModel storagePath() shadowing compile error
- Keep Phase 6 r4 runtime diagnostics and SMB resume fixes

* Fri Sep 11 2026 edp17 <edp17@protonmail.com> - 0.7.3-1
- Stop QML from calling updateSourceSize directly on PlaybackHistoryModel
- Let the SMB stream bridge resolve remembered SMB file sizes internally
- Add startup and history restore diagnostics for last-played video debugging
- Fix non-SMB currentSmbSize reset typo in the playback path

* Fri Sep 11 2026 edp17 <edp17@protonmail.com> - 0.7.2-1
- Store playback history next to the runtime-proven NAS source store
- Expose NAS source and history storage paths for debugging
- Persist SMB source file size in playback history
- Use the remembered SMB file size before falling back to smb2_stat on resume

* Fri Sep 11 2026 edp17 <edp17@protonmail.com> - 0.7.1-1
- Store playback history through the same Sailjail-safe app-data helper as NAS sources
- Migrate earlier playback-history JSON locations when visible
- Resolve SMB file size with smb2_stat before streaming remembered SMB videos
- Allow last-played SMB resume after app restart without requiring directory browsing first

* Fri Sep 11 2026 edp17 <edp17@protonmail.com> - 0.7.0-1
- Store playback history and last-played video in the Sailjail app-data directory
- Restore the last played video as Current video on startup
- Add Settings page with playback-history controls and storage diagnostics
- Update application icon with transparent-background SailVideo artwork
- Improve SMB resume behaviour when a remembered password is not available

* Fri Sep 11 2026 edp17 <edp17@protonmail.com> - 0.6.0-1
- Add Sailfish Secrets storage for SMB/NAS passwords
- Add Secrets Sailjail permission for SMB password storage
- Load saved NAS passwords automatically when browsing saved SMB sources
- Restore the last played video to the main page after app restart
- Allow saved SMB history entries to resume directly when credentials are available

* Fri Sep 11 2026 edp17 <edp17@protonmail.com> - 0.5.5-1
- Store NAS source definitions in the exact Sailjail-whitelisted app-data path
- Migrate NAS source JSON from the previous Qt AppDataLocation path when visible
- Show the NAS source store path on the NAS sources page for debugging

* Fri Sep 11 2026 edp17 <edp17@protonmail.com> - 0.5.4-1
- Store saved NAS sources in an explicit app-private JSON file
- Preserve one-time migration from the earlier QSettings NAS source store
- Report save/load failures in logs instead of silently losing NAS sources

* Fri Sep 11 2026 edp17 <edp17@protonmail.com> - 0.5.3-1
- Redirect bundled libsmb2 install output into the SailVideo private library directory
- Remove duplicate QML child-directory entries from the RPM file list

* Fri Sep 11 2026 edp17 <edp17@protonmail.com> - 0.5.2-1
- Require bundled libsmb2 source by default for SMB-enabled builds
- Explicitly install bundled libsmb2 target into the app-private library directory
- Improve runtime SMB backend diagnostics
- Sync saved NAS and network source settings immediately
- Add libsmb2 packaging check helper

* Fri Sep 11 2026 edp17 <edp17@protonmail.com> - 0.5.1-1
- Add app-private bundled libsmb2 build support
- Add libsmb2 import helper and LGPL licence notice
- Search SailVideo private library directory before system libsmb2 names
- Keep NAS browsing optional when bundled source is not present

* Fri Sep 11 2026 edp17 <edp17@protonmail.com> - 0.5.0-1
- Add manual SMB/NAS source management
- Add asynchronous SMB directory browsing through runtime libsmb2 loading
- Add SMB file playback through the existing localhost HTTP Range bridge
- Keep NAS passwords session-only until Sailfish Secrets is implemented
- Add NAS browser page and SMB testing documentation

* Fri Sep 11 2026 edp17 <edp17@protonmail.com> - 0.4.0-1
- Add persistent HTTP/HTTPS network source management
- Add direct playback for saved and one-off network media URLs
- Improve resume seek reliability after reopening a paused video
- Add resume history and stable player-page lifecycle
- Add local HTTP Range bridge test source
- Stop playback cleanly when leaving the player page
- Improve media title extraction from picker properties

* Fri Jun 26 2026 edp17 <edp17@protonmail.com> - 0.1.1-1
- Add MediaIndexing Sailjail permission for video picker
- Initial Phase 0 playback foundation
