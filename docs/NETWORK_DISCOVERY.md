# Network discovery

SailVideo includes a simple NAS discovery helper.

The scanner:

- enumerates active non-loopback IPv4 interfaces;
- scans the local `/24` network for hosts with TCP port 445 open;
- lists reachable addresses as possible SMB/NAS servers;
- opens Add SMB source with the selected host pre-filled.

Discovery does not enumerate SMB share names. The user still enters the share
and optional folder path on the SMB source page.

Limitations:

- no WS-Discovery, NetBIOS or SMB share enumeration;
- firewalls or NAS security settings may hide otherwise valid servers;
- unusual or larger subnets are intentionally not scanned beyond the local
  `/24` heuristic.

Manual NAS entry remains available and is the authoritative fallback.

`NetworkDiscoveryPage.qml` prefers the application-wide discovery model and can
instantiate its own model if the root-context object is unavailable. This
fallback is internal and is not exposed as a user-facing diagnostic.
