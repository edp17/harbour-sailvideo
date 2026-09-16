# Chromecast support

SailVideo 1.1 adds a native Google Cast sender.

## Protocol architecture

Discovery uses multicast DNS for:

```text
_googlecast._tcp.local
```

The implementation intentionally uses the classic Qt 5 `QUdpSocket`
`readDatagram()` API for Sailfish OS 5 compatibility.

The Cast V2 control channel uses TLS to the receiver on port 8009. SailVideo
implements the small Cast V2 protobuf message envelope directly and exchanges
JSON payloads in the standard Cast namespaces. There is no external protobuf
runtime dependency.

SailVideo launches Google's Default Media Receiver:

```text
CC1AD845
```

## Media URLs

HTTP/HTTPS video:
- the original source URL is sent directly to Chromecast.

Local video:
- SailVideo registers the local file with its existing HTTP Range server;
- a second LAN listener exposes that token using the phone's LAN address.

SMB/NAS video:
- the existing libsmb2/Range bridge is reused;
- SailVideo does not create another SMB implementation for Cast.

The LAN listener:
- binds an ephemeral IPv4 port;
- selects the phone address on the same subnet as the Chromecast where possible;
- only accepts the selected Chromecast peer address;
- supports GET, HEAD and byte ranges through the existing stream implementation.

## Remembered devices and reconnect

Discovered Chromecast devices are cached in SailVideo's application-data
directory. Opening the Cast page uses the remembered list immediately and
does not start a fresh mDNS scan when devices are already known. The pulley
menu provides an explicit scan/refresh action; a fresh discovery updates the
stored address for the same receiver UUID.

If a remembered receiver is unavailable, the TLS connection error is shown on
the Cast page with a Retry action. The user can also start a scan to refresh a
stale address or discover another receiver.

When SailVideo detaches from an active receiver, the last Cast endpoint is
recorded. On the next normal application launch SailVideo attempts to rejoin
the already-running Default Media Receiver and opens the Chromecast page. It
queries receiver/media status rather than launching or loading media again.
This is most reliable for direct HTTP/HTTPS media because the receiver can keep
fetching that source after SailVideo exits. Local/SMB media depends on the
phone's LAN bridge and therefore cannot be guaranteed to survive process exit.

## Stop and continue

`Stop media` stops the current media item but intentionally leaves the Default
Media Receiver session connected. The button then becomes `Start / continue`;
using it sends a fresh LOAD for the same remembered media at approximately the
last remote position (or from the beginning after a completed item). This
avoids a full disconnect/reconnect cycle.

## Picture display

SMB/NAS pictures opened by SailVideo can be displayed on Chromecast through
the same LAN HTTP bridge. Images are loaded into the Default Media Receiver as
image media. Previous/next picture navigation and SailVideo's slideshow issue
a new LOAD for each picture, so no Cast-side image queue is required.

## Disconnect semantics

`Disconnect and resume on phone` sends receiver namespace `STOP` with the
active Cast receiver `sessionId`, waits for receiver status to confirm that the
application session has terminated, then closes the TLS sender connection.
The local player then seeks to the last remote position and resumes.

`Leave playing on TV` only detaches the sender. Direct HTTP/HTTPS media can
continue after SailVideo closes. Local/SMB media can continue only while the
SailVideo process remains alive to serve its LAN URL.

## Diagnostic prefixes

Useful terminal logging is prefixed with:

```text
SailVideo Cast:
SailVideo Cast bridge:
```

These cover discovery, TLS, receiver status, LOAD/media status, receiver STOP,
LAN listener creation and HTTP Range requests.


## 1.1.0.1 workflow rules

### Initial receiver volume

For video transfers, SailVideo sends the current local playback-volume
percentage to the receiver immediately after the TLS Cast channel is ready and
before the media LOAD. Picture transfers do not alter receiver volume.

### Stop and continue position

Before issuing media `STOP`, SailVideo stores the last reported remote
position. Some receivers report `currentTime=0` while the STOP is being
processed; that transient value is ignored for resume purposes. `Start /
continue` sends a new LOAD using the captured stopped position.

### Folder media navigation

SailVideo keeps three related queues for an SMB folder:

- video queue: videos only;
- picture queue: pictures only;
- folder media queue: pictures and videos in the current displayed folder
  order.

Ordinary video playback and ordinary picture viewing continue to use their
specialised queues. During Cast playback, manual Previous/Next uses the unified
folder-media queue when available. This means a mixed folder can move from a
picture to a video, or vice versa, without returning to the NAS browser.

Picture slideshow intentionally uses only the picture queue. In a mixed folder
it skips videos and cycles through the images.

### Returning to an active Cast workflow

When a Cast session remains active and the user navigates back to the SailVideo
main page, the `Now casting` card returns to the active player or picture
viewer. The Chromecast page also provides an explicit `Return to player` or
`Return to picture` action.
