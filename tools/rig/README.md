# Rack rig (Windows, dev only)

Drives an isolated VCV Rack 2 Free to check the module without touching %LOCALAPPDATA%\Rack2 (`-Extra` only copies
plugins out of it). Run from the repo root. Copied from My lil Looper's rig.

1. `pwsh -File tools/mk.ps1 dist`, then `pwsh -File tools/rig/launch.ps1 -Patch tools/rig/patches/smoke.vcv -Extra Fundamental` → prints `pid=` and `dir=`.
2. `pwsh -File tools/rig/cap.ps1 -ProcId <pid> -Name <name>` → full-window PNG in Desktop\screenshots.
3. `click.ps1 -ProcId <pid> -ImgX <x> -ImgY <y> [-Button Right]`, `type.ps1 -ProcId <pid> -Text 5` (Enter is sent too).
4. `stop.ps1 -Dir <dir>` closes that Rack and deletes the dir.

click.ps1 moves the real cursor for ~0.4 s: don't run it while someone is using the mouse. Never trigger a native
dialog: it blocks the window and the scripts cannot close it.
`patches/smoke.vcv`: Fundamental VCO sine (C4) into Mouth IN; Mouth NOTE and GATE into Fundamental Scope.
Pass: Scope channel 1 reads 0.00 V (C4), channel 2 reads 10.00 V, the C lamp is lit.
Close any other running Rack first: Rack is single-instance, so launch.ps1 refuses while another Rack.exe is up, and
click.ps1 only clicks a rig Rack (its -rig- user dir) showing its own window, never the user's Rack or a message box.
