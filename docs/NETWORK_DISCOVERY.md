# Network discovery

SailVideo Phase 8 r1 includes a simple first-pass NAS discovery page.

The scanner:

- enumerates active non-loopback IPv4 network interfaces;
- scans the local `/24` address range for hosts with TCP port 445 open;
- lists each reachable address as a possible SMB/NAS server;
- opens the Add SMB source page with the host pre-filled when tapped.

Limitations:

- it does not discover SMB share names yet;
- it does not use WS-Discovery, NetBIOS or mDNS yet;
- firewalls or NAS security settings can hide otherwise valid servers;
- large or unusual subnets are intentionally not scanned in this first version.

If discovery finds nothing, manual NAS entry remains the authoritative path.


## Phase 8 r3

The discovery page no longer depends solely on a root-context property named
`networkDiscovery`. The C++ `NetworkDiscoveryModel` is also registered as a QML
type, and `NetworkDiscoveryPage.qml` creates a fallback instance when required.
This fixes the device-side `ReferenceError: networkDiscovery is not defined`
failure.
