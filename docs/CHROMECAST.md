# Chromecast support

SailVideo 1.1 includes a native Google Cast sender integrated directly into the
video player and picture viewer.

## User workflow

For a video or picture:

1. Open the media in SailVideo.
2. Pull down the Player or Picture Viewer menu.
3. Tap a remembered Chromecast, or choose **Scan for Chromecast**.
4. Continue controlling the media from the same page.

There is no separate Chromecast control page in the normal workflow.

While casting, a compact overlay identifies the receiver and exposes
Cast-specific controls. Normal video/picture navigation remains on the standard
media controls. Settings also remains available from the media-page pulley
menu.

## Discovery

Discovery uses multicast DNS for:

```text
_googlecast._tcp.local
```

The implementation uses Qt 5 `QUdpSocket` APIs compatible with Sailfish OS 5.

Discovered devices are cached in SailVideo's application-data directory. A
remembered device can therefore appear without starting a new scan. An explicit
**Scan for Chromecast** action refreshes known receivers and discovers new ones,
including on a fresh installation with an empty receiver cache.

## Cast V2 control channel

SailVideo connects to the receiver over TLS on port 8009 and implements the
small Cast V2 protobuf envelope directly. No external protobuf runtime is
required.

The sender launches Google's Default Media Receiver:

```text
CC1AD845
```

If the receiver transiently closes the Cast V2 TLS control channel while media
is still active, SailVideo retries the sender connection and rejoins the
existing receiver session rather than issuing a new LOAD. This keeps compatible
TV playback running and preserves the local/SMB LAN bridge during recovery.

## Media URLs

### HTTP/HTTPS video

The original source URL is sent directly to Chromecast.

### Local video

SailVideo exposes the local file through a temporary LAN listener backed by the
existing HTTP Range server.

### SMB/NAS video and pictures

The same libsmb2/HTTP Range bridge used by SailVideo playback is reused for
Chromecast. There is no second SMB implementation.

The LAN listener:

- binds an ephemeral IPv4 port;
- chooses a phone address reachable from the selected Chromecast;
- accepts requests only from the selected receiver address;
- supports GET, HEAD and byte ranges.

## Video handoff

Video casting begins near SailVideo's current playback position.

Before LOAD, SailVideo applies the current playback-volume percentage to the
receiver, clears the receiver's separate mute flag and waits for receiver status
to confirm the requested audio state.

While remote video is active, SailVideo's player controls operate on the Cast
session for play/pause, seek, restart, skip and previous/next.

## Stop and continue

Stopping a video leaves the Default Media Receiver connected. SailVideo records
the last remote position, and **Continue** reloads the same media at that
position rather than requiring a disconnect/recast cycle.

## Pictures and slideshow

SMB/NAS pictures are loaded into the Default Media Receiver as image media.

Previous/next picture navigation sends a new LOAD for the selected picture.
Picture slideshow uses SailVideo's picture queue and the slideshow interval from
Settings. In mixed folders, slideshow remains picture-only.

Manual Previous/Next during Cast can use SailVideo's unified folder-media queue,
allowing picture-to-video and video-to-picture transitions without returning to
the NAS browser.

## Unsupported MOV files

SailVideo does not transcode QuickTime/MOV for Chromecast.

MOV is rejected before a receiver LOAD. The phone shows an explicit unsupported
video placeholder while the receiver keeps the previous valid media. In a mixed
folder the logical queue position still advances, so Previous/Next can continue
past the unsupported item.

## Disconnect and detach

**Disconnect** terminates the Cast receiver session and, for video, resumes
phone playback near the latest remote position.

**Leave playing on TV** detaches the sender without stopping receiver media.
Direct HTTP/HTTPS media can continue independently. Local/SMB media still
depends on SailVideo remaining alive to serve the LAN URL.

## Rejoin after restart

When SailVideo has recorded a detached Cast endpoint, a later application launch
attempts to rejoin the running Default Media Receiver in place. The Main-page
**Now casting** card returns to the active Player or Picture Viewer workflow.

Rejoin is most reliable for direct HTTP/HTTPS media. Local/SMB Cast media
depends on SailVideo's LAN bridge and therefore cannot be guaranteed to survive
process exit.

## Diagnostics

Useful terminal prefixes are:

```text
SailVideo Cast:
SailVideo Cast bridge:
SailVideo SMB bridge:
```

These cover discovery, TLS, receiver/media status, LOAD, session stop and LAN
HTTP Range activity.
