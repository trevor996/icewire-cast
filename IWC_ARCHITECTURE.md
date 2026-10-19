# ICEWIRE CAST architecture

ICEWIRE CAST is a cloud-connected desktop production system.

## Desktop engine

The Windows app uses the real OBS scene, source, audio mixer, transition, encoder, recording, browser-source, and WebSocket engines. Cameras, capture cards, microphones, audio interfaces, local files, and Stream Deck integrations remain native desktop capabilities.

## ICEWIRE cloud

The default IWC CLOUD dock opens the production site at:

https://icewirelive.com/iwc/desktop

Broadcasters sign in with the same ICEWIRE account used on the website. League and team roles are resolved by the existing ICEWIRE authorization layer. Events, game/show templates, media, team branding, score data, YouTube destinations, fan delivery, comments, notifications, and archives remain in the cloud and follow the broadcaster to another IWC computer.

## Event formats

- Live Game loads matchup branding, the built-in IWC score bug, game clock controls, intermission scenes, sponsor media, and the team's saved game template.
- Coaches' Show loads desk, guest, intro, commercial, sponsor-break, and closing scenes from the team's saved show template. Game score and clock controls are hidden.

## Preview and Program

Scene selection changes Preview. Cut or Fade is required to move Preview to Program. The score bug is created automatically as an IWC browser source in each cloud template scene that requests it.

## Outputs

The IWC cloud dock provisions the private ICEWIRE RTMPS destination and loads it into the desktop engine. Cloudflare Stream powers the fan feed, automatic cloud recording, and server-side delivery to the team's saved YouTube destination when enabled.

## Desktop updates

ICEWIRE system administrators publish signed release metadata from the website Update Center. IWC checks the platform-specific stable channel at startup and from **Help → Check for ICEWIRE CAST Updates**. Windows installers and macOS disk images are downloaded over HTTPS, verified against the administrator-published SHA-256 checksum, and only then opened by the operating system. Required releases continue prompting until installed. macOS remembered sessions are stored in the user's Keychain; Windows sessions use DPAPI.
