---
read_when: Changing the share image, embedded background or social export layout.
---

# Reception share image

The export uses 2400 x 1350 pixels (16:9), matching CrispShare's X One image preset. The background is embedded into the native plugin/worker during the build, so sharing needs no download, network connection or additional installer resource. Native text remains separate from the photograph. Rendering uses one bounded background task and never queries the reception database.

Primary reading order: farthest distance and reception evidence; countries heard; unique stations; uptime; receiver location. Enabled modes appear under the heading. Bearing and recent-station count follow the primary statistics. White type and dark overlays protect contrast; verify both the full image and a 430-pixel-wide preview after layout changes. Small footer caveats remain secondary.

The heading is TODAY'S DX STATS BY followed by the entered callsign in gold. The callsign defaults to MY STATION and remembers the entered case and spaces. History or sessions spanning midnight use DX STATS BY. Modes use a pale-blue accent. The farthest country is larger than its callsign; the receiver row reads Receiver Location. Social captions begin with TODAY's DX STATS: and retain an explicit history qualifier when relevant. Scope, receiver privacy and distance units apply to both image and caption. Social buttons prepare a draft or clipboard content; they never post automatically.

Callsign and distance-unit changes update the preview asynchronously. Keep one renderer active and coalesce further edits into the latest request; never lose the last keystroke or enable export while an older image is being replaced. Keep the previous preview visible during rendering. While the dialog is visible, check the cached reception snapshot every three seconds; redraw only when displayed content changes or the image clock reaches a new minute. Manual refresh always requests a new image. Reuse receiver-country lookup when the grid stays unchanged. No refresh polling runs when the dialog is closed.

Kilometers/miles is the plugin's existing global preference, exposed here as well as in Settings. A new profile with a configured North American home grid initially selects miles, using the bundled country boundaries and continent metadata. Unknown/other locations use kilometers; an existing saved choice always wins. No IP or location-service request is made. The window fits its content height, preserves width resizing and limits its preview to the available display height. The invitation uses 1.5 times the normal font size, and the two input labels precede their controls.

## Background

Asset: `resources/share-hf-background.png`. Generated with the built-in Codex image generation tool on 2026-11-02. The original stays in Codex's generated-images folder. Local preparation used `imgsanitize clean --preset metadata`: full frame and pixels preserved; source metadata removed. Reconstruction was unavailable because the local model was missing and its download stalled. Pixel watermark removal and AI detection were not tested.

Generation prompt:

> Use case: ads-marketing. Asset type: background for an elegant amateur radio reception statistics card, wide landscape 16:9, intended final export 2400 by 1350. Create a refined cinematic editorial photograph of an HF amateur radio antenna installation at blue hour: a realistic slender dipole mast and fine antenna wires, distant hills, warm subtle horizon light, deep ink blue sky, restrained copper highlights. The rightmost quarter contains the antenna silhouette and atmospheric detail. Keep the left three quarters and lower middle calm, dark, low detail, with generous negative space so white statistics can be composited there later. The top area also needs dark readable negative space for a medium heading. Elegant and understated, natural photography, fine texture, no sci-fi effects. Full bleed, preserve frame, no borders. Absolutely no text, letters, numbers, logos, watermarks, UI, badges, labels, or charts. This is only the photographic backdrop; all typography and real reception data are rendered separately in code.

Keep the prepared PNG in source archives; CMake embeds it once in `dx_core`. Do not embed generated captions or synthetic reception records in a user's history. Preview test fixtures must remain isolated.
