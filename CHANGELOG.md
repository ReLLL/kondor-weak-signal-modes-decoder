# Changelog

## 1.0.56 (2026-11-01)

Includes the unpublished 1.0.54 and 1.0.55 changes below: hidden startup, unresolved-sender filtering, bundled DSP runtimes and host compatibility checks. Settings and saved receptions are preserved during installation.

## 1.0.55 (unreleased development)

- Bundle matching private VOLK/FFTW runtimes so compatible SDR++ upgrades can change their library filenames without breaking Kondor. Reinstalling restores the complete verified runtime payload.
- Check the real host architecture, dependency/symbol loading, ImGui layout and decoder helper startup before installation. Keep the installer responsive and cancel safely if the host updates during replacement.
- Start with the decoder window hidden when the SDR host launches. Enabled decoding, logging and notifications continue in the background; Open Kondor Decoder shows the current data.
- Keep the SDR# worker's native window hidden during creation so startup does not briefly flash it onscreen.

## 1.0.54 (unreleased development)

- Make Remove likely falses hide unresolved sender hashes before checking country/grid evidence, including existing history, new receptions, statistics, bearings and alerts. Previously empty countries and hash senders bypassed the check.
- Keep valid senders with unresolved targets, uniquely resolved sender hashes and bracketed full callsigns. Preserve reception history and CSV logs; unchecking the filter restores hidden receptions.

## 1.0.53 (2026-10-19)

- Rename the project to Kondor Weak Signal Modes Decoder: FT8 / FT4 / WSPR / FST4 / FST4W / JS8 / Q65. Compact controls and installer filenames use Kondor Weak Signal Modes Decoder.
- Migrate previous sidebar registrations while retaining receiver settings, enabled state, history, data folders and saved window layout.
- Add macOS/Windows clock synchronization instructions and a companion invitation to Maverick Text Modes Decoder.

## 1.0.50 (2026-10-19)

- Add a Windows SDR# x64 .NET 9 adapter, separate native interface and isolated FT8/FT4, WSPR/FST4/FST4W/Q65 and JS8 workers. Reuse existing maps, tables, settings and history. Use SDR#'s native downconverter, resampler and filters on private IQ copies.
- Add a Windows package and installer with dependency checks, runtime licenses, file hashes and upgrade backups. Keep Windows data separate from the macOS hosts.
- Keep the decoder running when its window is hidden. Bound capture queues and worker deadlines, invalidate missing samples and clean up workers after host exit.
- Prevent table-layout initialization from saving preferences before stored settings finish loading. This could otherwise overwrite a CSV path with an empty value at first startup.
- Document supported radio applications, installer and manual file-copy installation for both platforms, and feedback through GitHub or @FoxOnTheRunTr. Keep operating-system requirements in the download section.

## 1.0.49 (2026-10-19)

This release includes the development changes from 1.0.46 through 1.0.48 below: filtered live/history DX bearings, country-grid bearing estimates, elapsed history alongside uptime and a home-centered startup map.

- Fix country hover names showing opposite-side countries, such as Mexico over India or Canada over Russia. Keep each polygon continuous across longitude wrapping and use the shared containment function for map tooltips.
- Cover eastern/western countries, oceans, repeated world copies, dateline holes and Antarctica with bundled-outline regression cases. The corrected shared geometry also applies to country checks and distance estimates without changing saved reception evidence.

## 1.0.48 (unreleased development)

- Start the map centered on the configured home grid at the Home button's zoom level. Apply once per plugin start or settings reset; closing and reopening the window keeps the current map view.

## 1.0.47 (unreleased development)

- Remove the hidden 15-minute cap from DX activity bearing. Use the shared time filter exactly: All with saved history includes the farthest retained stations; Last 15 minutes and custom date ranges use their selected receptions.
- Keep mode, band, other filters, history/session boundaries and country-estimate labels consistent. Show the active time filter, scope and sample dates in the bearing tooltip; describe historical selections as past activity.
- Select only the requested farthest unique senders in SQL before reading activity payloads. Add a regression reproducing the WSPR history case: MI0KOA at 3,273 km and 313 degrees must outrank a recent 339 km station when All is selected.

## 1.0.46 (unreleased development)

- Include the existing country-grid approximation in DX activity bearings for every supported mode when country estimates are enabled. Prefer received/cached grids, mark aggregates containing estimates with [C], and show the estimate count in the tooltip.
- Preserve shared reception filters, live/history updates and the 15-minute activity window. Country fallback does not change reception evidence, map positions, QSO arcs or farthest records.
- Explain empty activity samples with No recent stations and cover all seven modes, setting changes, shared filters and live arrivals in regression tests.
- When either saved-history option is enabled, append an amber clock and elapsed history duration in parentheses after normal uptime. Calculate from the oldest retained reception to now and explain the start date, gaps and retention in the tooltip.

## 1.0.45 (2026-10-17)

This release includes the development changes from 1.0.40 through 1.0.43 below.

- Add optional receive-only JS8 in the existing map, tables, filters and history, with a separate decoder helper and four supported speeds. JS8 starts off.
- Add country-based distance estimates when received/cached grid evidence is missing, including country-name variants. Mark estimates with [C] and keep them separate from station positions, farthest records and DX activity bearings.
- Add a remembered Hide home location icon option in Map settings. Preserve home-based calculations and navigation.
- Make DX activity bearing and its compass follow shared reception filters and history/clear scope. Narrow the 15-minute activity limit with shorter time windows and custom date bounds; capture/alert settings no longer override the selected bands and modes.
- Bundle matching installers for SDR++ and SDR++Brown, upstream credits and corresponding source. Keep local agent instructions out of the repository and source package.

## 1.0.43 (unreleased development)

- Add Hide home location icon in Map settings. Apply immediately and remember the choice; keep home-based distances, bearings and navigation. Restore defaults shows the icon again.

## 1.0.42 (unreleased development)

- Resolve country-distance naming variants for European/Asiatic Russia and Turkey, East/West Malaysia, Türkiye, Congo, Korea, Bosnia, Trinidad and Côte d'Ivoire. Regional labels use the whole-country outline for this coarse estimate only; keep original country names and saved radio evidence.
- Share cached geometry results between aliases. Keep ambiguous names and entities without matching mapped areas unknown.

## 1.0.41 (unreleased development)

- Add Approximate unknown distances based on country in General settings, enabled by default. Use a near-side six-character grid center inside the matching country outline when received/cached grid evidence is missing. Mark estimates with [C], preserve raw history and exclude estimates from location, distance-record and propagation claims.
- Apply the setting to existing and new views, distance sorting/filters, details, latest heard, country averages and corner notifications. Identify estimated distances explicitly in table exports.
- Keep project agent instructions local and out of future corresponding-source archives.

## 1.0.40 (unreleased development)

- Add optional receive-only JS8 using pinned JS8Call decoder and Varicode source. JS8 starts off; Speed: All receives Normal, Fast, Turbo / JS8 40 and Slow. Exclude experimental JS8 60.
- Share existing map, tables, filters, home distances and saved history. Use octagonal markers and show speed/completeness in details. Keep incomplete or ambiguous messages explicit; never guess sender identity from prose.
- Isolate JS8 in a bounded worker and helper with cancellation, watchdog and restart recovery. Preserve FT8/FT4 and the other modes' workers and defaults. Bundle static Qt Core, licenses and corresponding source for both host installers.

## 1.0.39 (2026-10-15)

This release includes the development changes from 1.0.33 through 1.0.38 below.

- Apply Remove likely falses to existing entries and new arrivals in reception statistics, Latest heard, country highlights, waterfall notifications and distance-record alerts, as well as map and table queries. Keep uptime as elapsed receiver time and preserve saved records. Update filtered summaries incrementally in the background, including retention cleanup.
- Limit the Latest heard bar to three stations. Make the SNR scale taller and reduce status bar padding while keeping its geometry fixed during notifications.

## 1.0.38 (unreleased development)

- Ensure Restore defaults resets existing table column widths and layouts before table initialization, including layouts saved for tabs not yet opened. Keep the Default sorting action limited to sorting.

## 1.0.37 (unreleased development)

- Rename the plugin to Advanced FT8/FT4/WSPR DX Decoder. Migrate existing sidebar entries while keeping settings and reception history.
- Move operation notices, errors and extra decoder queue activity into a fixed bottom status bar. Notifications fade in for two seconds, remain visible for four, then fade out for two; errors stay visible until resolved.
- Place the compact SNR label and continuous color scale immediately after Fit on the map toolbar.
- Keep Color based on SNR in Settings > Map only, enabled by default.
- Default alert coverage to All bands, preserving explicit saved choices.
- Nest History DB beneath History in settings. Group viewing, CSV and export preferences under History; show database size, automatic retention and confirmed one-off cleanups under History DB. Allow separate cleanup trigger and retained-record counts, defaulting to 100,000 and 95,000.
- Prefix the farthest station's gold distance with the trophy emoji and a space in station/message tables; keep numeric sorting and right alignment.
- Add Default sorting beside Clear for each table tab, restoring its newest-first order while keeping filters and column layout.
- Extend received-grid caching to seven days without refreshing evidence from nongrid messages. Show a station's older last-known received grid with a hollow map marker and its original grid date; keep that display fallback separate from reception payloads, distance records and exchange paths.
- Add the remembered, default-off Remove likely falses filter. Compare grid cells with the callsign country's Natural Earth outlines, allowing 300 km plus grid uncertainty by default. Keep unknown locations and explicit portable calls; preserve saved records and ignore rules. Explain legitimate portable/remote exceptions in its tooltip.
- Add a fixed Settings footer with Restore defaults and confirmation. Reset plugin preferences and layouts while preserving home setup, reception payloads and moderation rules; explain that normal retention limits apply afterward.
- Compare distance-record alerts with the same saved-history or session scope as the Farthest statistic. Latest messages using cached grids no longer erase farther receptions from the alert baseline.

## 1.0.36 (unreleased development)

- Place WSPR beside FT4 in the main reception controls. Default FT8, FT4 and WSPR to on, while remembering each saved preference.

## 1.0.35 (unreleased development)

- Show the total saved reception count and main SQLite file size in MB at the top of Saved messages. Refresh file metadata in the background, independently of the 30-minute automatic cleanup check.

## 1.0.34 (unreleased development)

- Add Sequence: All as the default for additional modes. Capture every supported period in parallel using shared audio blocks; retain individual period choices. Keep Q65 tone spacing explicit.
- Bound additional work to 32 pending jobs and 96 MiB of unique queued audio, with deadline ordering and a separate FT8/FT4 worker.
- Select table stations with a click or Up/Down and highlight them on the map. Open details on a table double-click or map click; closing details preserves selection.

## 1.0.33 (unreleased development)

- Add optional WSPR, FST4, FST4W and Q65 reception using pinned official WSJT-X decoder sources, with mode-specific periods and Q65 tone spacing in Reception settings. New modes start off; FT8/FT4 keep their existing controls and decoder.
- Use a separate bounded worker for additional modes, complete UTC captures, cancellation on retuning/stopping, bounded temporary files and bundled macOS 15-compatible Fortran runtimes. Q65 decodes individual sequences without cross-sequence averaging.
- Extend shared mode/Beacon filters, reception history, CSV logging and details, including reported beacon transmitter power separately from receiver SNR. Keep FT8/FT4 map circles; add squares, triangles, diamonds and hexagons for the new modes.
- Credit WSJT-X, the protocol designers and decoder authors in README, CREDITS.md and third-party notices; include credits in release downloads and the installer.

## 1.0.32

- Enable Color based on SNR by default for new or unset preferences. The checkbox remains optional and saves the selected preference.

## 1.0.31

- Check history size and count every 30 minutes, at startup and on policy changes. Default to 100,000 receptions and gradually trim to 95,000; custom limits retain 95%. Reception and CSV protection remain unchanged.

## 1.0.30

- Maintain latest sender/country summaries transactionally and reuse session statistics between arrivals. Read the latest messages through the time index.
- Limit SQLite history to 50,000 receptions by default, with resumable background deletion to 90% of the limit. Settings shows the retained date range and supports changing or disabling the limit. CSV rotation remains separate.
- Check the stored count and database size once a minute, at startup and on a policy change. Run short cleanup batches during continuous decoding, protect the CSV watermark, and update long-session stats incrementally as records expire.
- Preserve reception IDs, CSV catch-up boundaries and later arrivals during automatic cleanup. Reuse freed pages and use incremental compaction where enabled.
- Add isolated large-history benchmarks and regression coverage for cleanup rollback, restart, summaries and ID continuity.

## 1.0.29

- Extend sender and target callsign-hash matching from three minutes to twelve minutes. Accept only one distinct matching callsign; retain unresolved identifiers when candidates are missing or ambiguous. Exclude future receptions and candidates older than twelve minutes.
- Add isolated tests for the twelve-minute boundary, all three hash sizes, sender/target matching, collisions and expiry. All displayed station distances continue to use the receiver's home grid, independently of station-to-station communication arcs.

## 1.0.28

- Combine time, band, mode, message type, search and advanced filters in one responsive bar for the map and all table tabs. Retain the previous table selection on upgrade; wrap controls only when necessary and add Reset for both views.
- Limit exchange arcs to the host's visible RF spectrum, including zoom and pan. Hide stale arcs immediately when the viewport changes and choose recent exchanges from the new range.
- Show each map-tooltip message's receiver SNR after its text. Color time and message separately, and use the continuous red/yellow/green signal scale for SNR in map message history and details.
- Add a horizontal splitter above details and remember its height independently of the map/table split.
- Add the compass direction after the degree bearing in station details, using the short-path bearing from the home location.
- Add a compact compass beside DX ACTIVITY BEARING within the existing two-row statistic height. Its red/white needle follows the estimated bearing; mixed or unknown directions show no needle.
- Display reception dates in the computer's time zone throughout tables, details, map tooltips and copied rows. Include the UTC offset; elapsed ages and chronological sorting keep their original behavior.
- Interpret custom filters and saved-message cleanup dates as local times. Respect 23-hour and 25-hour calendar days; reject invalid or ambiguous clock-change times before cleanup.
- Preserve existing custom-range instants when upgrading or changing the computer's time zone. Add local date/time to CSV exports; keep reception log timestamps and GridTracker messages in UTC.

## 1.0.27

- Separate Ignore transmitter and Delete transmitter in every station context menu. Each confirmation offers one unchecked checkbox for the other operation. Ignore alone keeps saved receptions; Delete alone permits future receptions.
- Add Settings > Saved messages with independent, confirmed, one-off cleanup actions: before/after/between UTC dates, selected country, keep only selected country, missing distance, and missing grid. Preview database matches asynchronously; clean known CSV logs and SQLite, update views/statistics and retain receptions added after the preview.
- Compact SQLite after deletion in the background. Migrate reception IDs to an explicit integer primary key before VACUUM; preserve CSV/import watermarks and resume failed log cleanup after restart.
- Enable time-window selection with saved history, add clearly named Last hour/6 hours/24 hours/7 days/month presets and an All button to cancel the time filter independently in each view. Last month means a rolling 30 days.
- Use Date in map tooltips for UTC timestamps and Age for seconds ago.
- Require isolated test profiles and removal of test receptions after testing in project instructions; clean owned test files when test engines close.

## 1.0.26

- Show ages through 600 seconds as seconds followed by ago; older values show UTC date/time. Apply the same formatter to Unique stations, All messages, Countries heard and map age tooltips. Keep timestamp sorting and numeric age filters unchanged.
- Rename the top direction statistic to DX ACTIVITY BEARING and fix its reception window at the last 15 minutes, independently of map/table history windows. Keep the station count adjustable.
- Show Time correction on the queued/skipped row, including zero. Brown uses the actual applied capture-time offset; standard SDR++ reports no plugin offset.

- Remove installer restrictions on exact host versions/core fingerprints; retain host identity, required-library, payload and backup checks.
- Suggest an empty home grid with macOS Location Services, showing accuracy and asking the user to confirm. Keep existing/manual grids, support denied/unavailable location, and add a locator-map button and beginner README instructions.

- Add Color based on SNR beside the history checkbox, default off and remembered. Map-circle fills use a continuous red/yellow/green dB scale with a legend, separate selection outlines and latest-reception halos.
- Add sender and target callsign lookup to the details-row popup; retain the clicked reception while live messages update.
- Fix saved-history restoration during reception: rolling refreshes no longer discard completed loads. Rank compact reception IDs and index first-heard queries to reduce work. Keep live reception and logging active while history is enabled.
- Hide ignored transmitters immediately from map/table/details while the worker deletes their saved receptions and known CSV logs; preserve confirmation and retry failed log cleanup.
- Remove the home-to-selected-station gold path. Keep dark-blue communication arcs between addressed stations.

## 1.0.25

- Add a native macOS installer for both SDR++ and SDR++Brown, with automatic host selection, optional home locator, automatic first-run preparation, safe host quit, custom-profile selection, backup access and Open host. End users do not need Python, Terminal, Homebrew, developer tools or administrator access.
- Verify payload hashes and the selected host core before writing. Stage replacements, preserve settings/logs, keep timestamped backups and recover from handled write failures. Refuse configuration changes or a restarted host during installation.
- Include the installer automatically in release ZIP and DMG packaging, with Developer ID/notarization options and verification gates. Include installer source in the corresponding-source archive.
- Add a separate standard SDR++ build using standard APIs and ImGui hooks, UTC capture timing, independent location settings, bundled country-prefix data and separate reception storage. Keep Brown's native integration intact.
- Verify live FT8 reception in standard SDR++ 1.2.0 (May 28, 2024 build), including countries, distances, map, tables and notifications. Add installer safety checks for both hosts. Document Brown's Source > My Grid and standard SDR++'s plugin location setting.

## 1.0.24

- Show each new reception in the waterfall corner, including repeats. Expire notifications after an adjustable 10 seconds by default while preserving history and the farthest row. Keep the two-second arrival glow. No four-row cap.
- Show both trophy and country flag on the farthest row. Color only its Farthest label gold; use regular field colors for its data.
- Add flags before country names in all tables. Reformat station details with colored fields and a reception-history table.
- Double-click a Countries heard row to open all matching receptions with sender, target, signal, distance, grid, message and source. Apply current filters and time/history scope; update asynchronously.
- Rename Approx. km to Distance. Add a saved kilometers/miles preference, default kilometers, across distance displays and filters. Preserve canonical kilometer logs and exports.
- Test notification expiry, repeats, ignored receptions, startup/history behavior, country detail filtering/paging and distance conversion.

## 1.0.23

- Make window close/open explicitly independent of receiver enablement. Show Receiving in background in the sidebar; reopening refreshes data and focuses the existing window without resetting its size or position.
- Add Unique stations, All messages and Countries heard tabs with shared filters. Keep repeats in All messages, newest first; show UTC and source alongside decoder fields. Save each table's layout and sort.
- Add country station counts, mean distance/SNR, latest five distinct senders and last-heard age. Average each sender's latest matching reception once; omit unknown distances. Country exports follow the selected tab.
- Add Corner notifications on waterfall, initially on: every heard country, native flags, distinct field colors and full italic messages. Changed rows pulse for two seconds; a fixed gold trophy row shows the farthest reception. Long lists and messages scroll without tuning the waterfall.
- Add country aggregation, repeat ordering, native flag/trophy and pulse regressions. Use unique recovery-test directories to avoid stale files when process IDs are reused.

## 1.0.22

- Keep country cells green for stations from the three newest countries as their messages update; repeats do not promote older countries. Gold retains priority.
- Rename Call to Sender. Show Target and Mod as the final two default columns; Mod displays the latest FT8/FT4 mode.
- Save column drag order, widths, visibility and sort in plugin settings across restarts. Every column can move.
- Add numeric ranges for all signal, distance, frequency, count and age columns, alongside Sender/Target/Mod and other text filters. Apply ranges to displayed rows and refresh age filters as time passes. Filter controls scroll within the screen.

## 1.0.21

- Show one table row per callsign across bands, modes and dial frequencies. Message, SNR and reception fields use the latest matching decode; equal timestamps use arrival order. Keep every reception in the logs and station details.
- Enable Ignore transmitter for unresolved and malformed identities. With no transmitter identity, confirm the exact message to delete and ignore; preserve unrelated unidentified messages. Persist both ignore types and retry CSV cleanup after restart. Preserve quoted identifiers and messages during log cleanup.
- Add Look up addressed station to the shared table/map menu, with Google, QRZ and HamQTH choices for the target callsign.

## 1.0.20

- Limit green country markers to the three most recently discovered countries. Repeats and additional stations from an already heard country do not promote it; the fourth new country replaces the oldest marker. Gold keeps priority.
- Add Use saved history for everything above the views and in Settings > History. Restore retained statistics, map markers, table rows and first-heard times in the background, with existing filters and sorting. Starts off; saves the preference. Uptime stays session-based.
- Clear with Reset stats returns to session mode and zeroed totals. Clearing without Reset stats hides selected views without removing retained records. Update the legend, tooltips and help.

## 1.0.19

- Map markers and table rows share the same callsign lookup, copying, favorite and ignore menu.
- Ignore transmitter asks for confirmation, removes all of that callsign's receptions from views, statistics, saved history and known reception CSV logs, and blocks future receptions. Cleanup runs in the background and retries failed log writes after restart. Unignore allows future receptions; it cannot restore deleted history.
- Grouped station rows display the latest received message, with arrival order breaking equal UTC timestamps. First-heard timestamps retain their documented meaning.
- Verify RK9UE's NO35 distance from KM69 against the ARRL grid calculator: approximately 4,296 km. Expose station coordinates and the home grid in map tooltips. Recalculate saved distance and bearing values in the background when the home grid changes.

## 1.0.18

First public release of Advanced FT8/FT4 DX Decoder for the tested macOS Apple Silicon SDR++Brown build.

- Native FT8/FT4 decoding, reception statistics, map, sortable message table, help and organized settings.
- Clear visible views and fresh session statistics at startup, with persistent SQLite and CSV logs.
- Optional saved-history statistics, initially off, calculated in the background.
- Station first heard before Msg age, newest station first by default; right-aligned age/first-heard values.
- Germany country-name normalization, cached-distance asterisk before the value, table color legend and right-edge Close details button.
- Visible map/table splitter, country highlights, day/night overlay, bounded tile caching and dark-blue animated dotted exchange arcs.
- Last five distinct station messages in map hover, callsign website lookups, quiet alert defaults and individual sounds.
- Optional direct GridTracker UDP feed, initially off, with local defaults ready.
- Decoder helper isolation, FFT plan lifetime corrections, persistent CSV recovery and test coverage for session/history switching.
