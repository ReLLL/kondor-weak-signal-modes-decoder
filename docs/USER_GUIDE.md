# User guide

Read when: setting up reception, understanding timestamps or distances, or configuring maps, logs and GridTracker.

## Start receiving

Set your own home Maidenhead grid in Brown. Distances and bearings originate at its center. Start the SDR and place a supported FT8 or FT4 dial inside the captured RF range. The plugin creates its own receiving channels for eligible presets; the ordinary Radio module can keep its selected mode. Check **Receiving**, level in dBFS, slot progress and decoded messages. **Time correction** follows queued/skipped counts on the same row, including zero. Brown shows the manual Source > Time correction offset applied to capture timestamps; standard SDR++ uses the Mac UTC clock with no plugin offset. This value is separate from a received signal's DT. Additional dial frequencies can be added in Settings > Reception.

FT8 slots last 15 seconds; FT4 slots last 7.5 seconds. A complete slot is needed after tuning or starting. Keep the computer clock synchronized. SNR is the decoder's estimate measured at this receiver; dBFS is the input audio level, a different measurement.

## Statistics and saved history

Countries and stations count unique received identities. Country names come from the transmitting callsign through Brown's local `cty.dat` DXCC database. Exact callsign exceptions take priority, then the longest matching prefix; portable operating prefixes can override a home callsign. FT8/FT4 does not transmit a separate country field. This identifies a callsign's DXCC entity and cannot establish exact transmitter coordinates. **Farthest** includes the country in parentheses. **UPTIME** shows active decoding time for this session, excluding stopped time. **Last 5 min** counts distinct recent callsigns; **Latest heard** shows the latest three distinct calls, country, approximate distance and SNR.

**Use saved history for stats** starts unchecked. Off counts only receptions received since this session started or session statistics were reset. On uses all retained database receptions, including imported history. The worker calculates the new values in the background and the previous values remain visible until ready. Uptime remains session-based. This preference is saved; turn it off to return to session statistics. It does not restore table/map entries or first-heard times.

**Use saved history for everything** also starts unchecked. Enable it to restore retained receptions into statistics, map markers and table rows together. First-heard columns use the first retained reception timestamps. Loading runs in the background; filters and table sorting still apply. Enabling this option initially selects All for both views. Time windows remain selectable while saved history is enabled, and uptime remains this session's active decoding time. With either history option on, an amber clock value in parentheses follows normal uptime and shows elapsed time from the oldest retained database reception to now. This includes reception gaps and stopped periods, uses the full retained database regardless of display filters, and moves forward when older records are deleted. Hover for the oldest reception's local date/time and the explanation. The preference is saved, so enabling it also restores history after restart. Turning it off returns to session views and the previous statistics preference. Both checkboxes are available above the map/table and in Settings > History. Live decoding continues with either option checked: new receptions update the table, map and statistics and are saved to SQLite history and the CSV log.

## Window and reception views

The decoder window starts hidden each time the host launches. Enabled reception, logging and notifications continue in the background. Click Open Kondor Decoder in the host's plugin panel to show current data.

Closing the decoder window hides its interface. Reception, uptime, logging, alerts, GridTracker forwarding and snapshots continue while the sidebar switch is enabled, the SDR is running and reception is not paused. The sidebar shows Receiving in background. Open brings the window to the front, refreshes current filters and preserves its size and position. The sidebar switch disables reception; closing the window does not reset statistics or data.

The table area has three tabs with shared time, search and reception filters:

- Unique stations keeps one latest matching row per sender. Its default sort is newest Station first heard.
- All messages shows each retained reception, including repeated messages and senders, newest first. Equal timestamps use arrival order. It adds local date/time and Source to the decoder fields; the count column is hidden because each row is one reception. Its green/gold values and nine-second arrival highlight match Unique stations.
- Countries heard groups the latest matching reception of each distinct sender by country. It shows station count, mean distance, mean SNR, the latest five distinct senders and time since the last reception. Unknown distances are excluded from the distance mean. SNR is the arithmetic mean of receiver estimates in dB, not combined signal power. Repeated messages do not add weight. Unresolved sender hashes are excluded from station averages. The tooltip states how many senders supplied distance evidence.

Each tab remembers its sort, column order, widths and visibility. Countries heard initially sorts latest reception first. Exports follow the active tab. Clear and its confirmation still apply to the table/map views and optional statistics reset, across all tabs.

## Waterfall corner notifications

The main decoder window has a fixed bottom status bar for operation notices, errors and queue activity. Notifications fade in for two seconds, stay visible for four, then fade out for two. They do not move the map or tables. Errors remain visible until resolved; hover for the full text, and use Retry when available. Distance-record alerts use the same statistics scope as Farthest at the top. With Color based on SNR enabled, the compact SNR label and color scale appear immediately to the right of Fit on the map toolbar.

Settings > General > Corner notifications on waterfall starts on and saves its preference. The corner remains visible when the decoder window is closed. Every new reception gets its own row, including repeated messages and senders: flag, country, sender, approximate distance, receiver SNR and the full quoted message in italics. Countries use green, senders blue and distances orange; SNR and messages use neutral text. New rows glow in and out for two seconds. Newest arrivals appear first. Rows expire after 10 seconds by default; Notification duration in General settings allows 2-120 seconds. There is no four-row limit.

The farthest reception in the selected statistics scope stays below the notification list. Its row includes a trophy and country flag. Only the word Farthest is gold; the remaining fields use the usual colors. Flags use ISO region codes from the bundled Natural Earth data and existing DXCC outline aliases; unmapped entities use a neutral flag. A flag identifies the displayed region, not an operator's nationality.

Long reception lists scroll vertically; long full messages scroll horizontally and are also available on hover. Corner scrolling does not tune the waterfall. Expiry removes only notifications; table, map, statistics and persistent logs retain the reception. Saved history can restore the farthest record but does not replay old receptions as notifications. Clear with Reset stats clears the notification feed too.

## Flags, details and distance units

Bracketed values such as `<1:265>` and `<2:454382>` are unresolved callsign hashes. The decoder uses 10-, 12- or 22-bit hashes depending on the message format. The plugin compares them with distinct resolved sender callsigns recorded during the preceding twelve minutes, including the exact twelve-minute boundary. A single matching candidate supplies the identity; missing or ambiguous matches stay unresolved. Future receptions are excluded. A unique match is evidence, not proof, because different callsigns can share a hash. Country lookup requires a resolved sender identity; a received grid can still supply distance without it.

Country names in Unique stations, All messages, Countries heard and the details history have flags before them. Existing green/gold country highlights remain unchanged.

Station details show separate colors for sender, country, distance, received signal, mode/grid and message. The close button stays at the right. Drag the horizontal divider above details to adjust its height; the split is remembered. Receiver SNR values use the same continuous red/yellow/green scale as map markers, regardless of whether marker SNR coloring is enabled. Reception history is a table with local date/time, sender, target, country, grid, SNR, Distance, mode/band, full message and source. Right-click anywhere on a details row for the same station popup as the main table. It searches the sender on Google, QRZ or HamQTH; Look up addressed station offers the same providers for the target. The menu retains the clicked reception while new rows arrive.

Double-click a Countries heard row to open all its matching messages in the details pane, newest first, including repeats. The current table filters, time window and saved-history choice apply. New matching receptions update this pane in the background. Older messages loads the next 500 receptions.

Settings > General > Distance units offers Kilometers (km), the default, and Miles (mi). The choice is remembered. Distance columns, country averages, farthest statistics, latest-heard summaries, map tips, details, alerts and waterfall notifications use the selected unit. Distance filters show and accept that unit; changing units preserves their physical limits. Sorting uses the same underlying distances. CSV/JSON exports and reception logs keep their explicitly named kilometer fields for compatibility. Distances remain approximate received-grid-center short paths in either unit.

## Table timestamps and sorting

| Column | Meaning |
| --- | --- |
| Station first heard | First reception of any message from that callsign during the visible session, across bands and modes. |
| Msg age | Time since the latest received message. Repeated receptions refresh it. |
| Msg first heard | First reception of this exact message from this callsign during the visible session. Repeats retain it. |

Age values through 600 seconds show elapsed seconds followed by ago, such as `30s ago` or `600s ago`. Older values show the original date and time in the computer's time zone, including its UTC offset, such as `2026-10-14 17:30:00 +03:00`. The same rule applies to both reception tables, Countries heard > Last heard and map age tooltips. Unknown times show `--`; future timestamps show `0s ago`. Hover still shows the original local date/time. Sorting always uses the underlying timestamp; age filters continue to use elapsed seconds regardless of the displayed format. With saved history for everything enabled, the first-heard times use retained history rather than this session. **Station first heard** appears before **Msg age**. All three align right. The default sort places newly heard stations first, using their first-heard timestamps. Click any column header to change the sort. Drag headers to reorder columns and drag their edges to resize. Right-click a header to show or hide columns. Order, widths, visibility and sorting are saved across restarts; horizontal scrolling reveals extra columns. Sender identifies the transmitter. Target shows the addressed callsign, or -- for a general call or no target. Mod shows FT8 or FT4 for the latest matching reception. Target and Mod initially appear last, in that order.

The table shows **one row per callsign** across all matching bands, modes and dial frequencies. Its message, SNR, band, mode, distance and other reception fields come from the latest matching decode. Rows without a transmitter identity group by exact message text. Individual receptions remain in saved logs and station details. Right-click a row for lookup, copying, watch-list and other station actions. A click or Up/Down selects the station on the map; double-click opens details. A single map click also opens details. **Close details** sits at the right edge. **Default sorting**, beside Clear, restores newest station first heard, newest message, or most recently heard country first for the active tab. Column widths, order and filters are kept.

The Message cell updates with the newest matching reception from that station. If timestamps tie, the last arrival wins. Filters apply before selecting the latest row: filtering to 40m shows that station's latest 40m reception. Two receptions on nearby FT4 dial frequencies appear in one station row. All individual receptions remain logged.

## Colors and distance evidence

- Green country: any station from one of the three most recently discovered countries. Discovery order follows each country's first arrival in the statistics session; repeats and new stations from an already heard country do not promote it. A fourth new country returns the oldest marker to normal text. With saved history for everything enabled, discovery uses all retained receptions. Every displayed station row from those three countries stays green as messages update. Gold takes precedence for the farthest station.
- Bold gold country and distance: the farthest station in the selected statistics scope. Its distance starts with 🏆 and a space in Unique stations and All messages. Gold takes precedence over green.
- Fading green row: just received message, fading over nine seconds.
- Light-green age and map halo: the latest reception batch, until a newer batch arrives.
- Callsign and grid text remain neutral. Teal SNR indicates at least -10 dB; this is a display threshold, not a calibrated quality grade.

Distances are approximate great-circle distances between Maidenhead grid centers. A four-character grid represents an area, so its center is not a surveyed transmitter location. No tilde is shown. `*` before a distance means the same callsign's most recent directly received grid was reused, for up to seven days. Repeated nongrid messages never renew the evidence time.

**Approximate unknown distances based on country**, in **Settings > General**, starts on and is remembered. When received or recent cached grid evidence is unavailable, `[C]` marks an estimate from your home grid center to a nearby six-character grid center inside the identified country's mapped border. The search uses the near side of the country, including islands and polygon holes, rather than its center. This is a coarse geographic estimate; the actual station can be much farther away. Borders come from the bundled generalized map, and the selected grid center adds cell-size uncertainty. Missing home coordinates, unknown countries, country areas too small for a mapped grid center, and DXCC subdivisions without a matching outline stay unknown. Explicit aliases recognize European/Asiatic/Asian Russia and Turkey, East/West Malaysia, Türkiye, abbreviated country names and several older names. Regional labels use the whole-country outline for this coarse estimate, so a Russia estimate can fall outside the named European/Asiatic region. The displayed DXCC country name stays unchanged. Ambiguous names such as Congo, and unrelated islands/subdivisions without a matching outline, stay unknown. United States estimates exclude Alaska/Hawaii, and France excludes French Guiana, which have separate DXCC identities.

The setting updates existing and new visible receptions on the background worker. Distance sorting, filters, latest-heard summaries, details, notifications and country averages use the displayed estimate; `[C]` also marks averages containing country estimates. It never supplies a received grid, map position, QSO arc, distance record or farthest trophy. DX activity can use the same country grid as a coarse fallback, explicitly marked `[C]`. Saved reception payloads and the persistent reception CSV keep their original evidence. Table exports identify country estimates separately using DistanceSource/CountryEstimateGrid or the corresponding JSON fields. Country exports include the count of estimated senders. Turning the setting off restores unknown values immediately without deleting data.

If a visible station has no usable current grid, the map can still show its last directly received location from retained history. A hollow marker identifies this display fallback; its tooltip shows the original grid date and last-known distance from home. This does not change the reception's saved grid/distance, statistics or exchange paths. Stations with no received location evidence remain unplottable; a country prefix alone is insufficient. Unresolved hashes never reuse old locations.

Another decoder may use a country or regional reference point and report a different distance. Received-grid evidence takes precedence here; country estimates are explicitly marked `[C]`. Germany is displayed under that short name throughout the interface and new logs.

Hover over home to see the configured Maidenhead grid and its center coordinates; station hover shows its received grid center coordinates. Changing the home grid recalculates saved table distances, bearings and farthest statistics in the background. Existing CSV entries keep the values recorded at reception time.

For example, KM69 (39.5 N, 33.0 E) to NO35 (55.5 N, 87.0 E) is 4,296.4 km on the mean-radius sphere. The [ARRL grid-distance calculator](https://contests.arrl.org/griddistancecalc.php) gives 4,296.5 km using a slightly different Earth radius. These are geographic short-path distances; the radio signal's actual ionospheric path is not measured.

## Map and communication arcs

On startup, the map centers on your home grid at the same zoom as **Home**. If the home grid is not configured yet, this happens when it becomes available. Closing and reopening the plugin window keeps your current map view.

Drag the visible divider to give more space to the map or table. Drag the map to pan and scroll over it to zoom. Settings > Map selects the basemap. The default **Street by Esri (English)** provides the street/terrain style used by GridTracker's Esri option. Humanitarian OpenStreetMap and the bundled offline map remain available.

The house marks home. **Settings > Map > Hide home location icon** hides the marker and its tooltip immediately and remembers the choice. It starts unchecked; distances, bearings and Home/Fit navigation still use your home grid. Country labels on the offline/OpenStreetMap maps use small muted text and disappear if the label does not fit inside the country outline. Provider labels baked into tiles follow that provider's style. Optional heard-country shading starts faint green and has a color/opacity selector. Several DXCC entities can share one outline.

Night shading follows the solar terminator. The boundary indicates the day/night geometry; it does not predict reception conditions.

Dark-blue curved moving dots connect an addressed transmitter to the station it calls. General calls create no arc. There is one arc per station pair, dial and mode, with the latest reply setting its direction. Both stations need received grids, which may be cached. Arcs appear only while the message's RF frequency (dial plus received audio frequency) is inside the host's visible spectrum. Retuning, zooming or panning immediately hides out-of-view arcs; markers and saved receptions remain. Set **Latest exchanges** and **Animate paths** in Map settings. The curved display is a visual guide, not a measured ionospheric ray path.

Selecting a station highlights its marker without drawing a path from home.

Station hover shows age, distance, band, receiver SNR and the latest five distinct messages on that frequency/mode. Each line has its latest local date/time, quoted text and the receiver SNR measured when that message was received. Time and message text have separate colors; SNR uses the continuous red/yellow/green scale. Repeats refresh that message's time without using another row. The history loads in the background.

## Map caching

Visible online tiles download in the background with four parallel requests and reused connections. Previously viewed tiles are cached in memory and in a bounded 256 MB disk cache. Parent/child tile fallbacks keep useful map detail visible during zoom changes. Downloads outside the view are cancelled.

The cache respects provider expiry and no-store directives. Stale cached tiles remain usable during network failure. It is not an all-zoom offline download: only visited tiles are cached, with eviction as the limit is reached. The bundled world map works without a network. Provider credits stay visible on the map.

Right-click a station marker or table row for the same station menu: Google, QRZ, HamQTH, copying, favorites and Ignore transmitter. **Look up addressed station** opens the same three providers for the target callsign in an exchange message. General calls and unresolved target identifiers do not offer a callsign lookup. When nearby entries share a marker, the menu applies to the latest reception represented by that marker.

**Ignore transmitter** blocks matching future receptions on every band/mode and keeps existing rows, markers, statistics and logs. **Delete transmitter**, immediately below it, removes matching saved receptions from map/table, statistics, SQLite and known CSV logs while allowing new receptions. Both ask for confirmation. Each dialog has one unchecked checkbox: **Also delete saved receptions** for Ignore, or **Also ignore future receptions** for Delete. An existing ignore rule is preserved when using Delete. Exact unresolved or malformed identifiers are supported; an unresolved hash can be reused by other stations. With no transmitter identity, the action matches only repetitions of that exact message. Uptime is kept. Cancel changes nothing. **Unignore all** allows future receptions; deleted records stay deleted.

Log cleanup runs in the background. A failed file write remains visible and retries, including after restart. Cleanup covers the current/default reception log and prior log locations recorded by this version. It does not edit other decoders' input logs, exported snapshots or unmanaged CSV copies.

## History DB

History DB is nested beneath History in the settings sidebar. History contains viewing preferences, exports, ignored identities and the CSV file settings. History DB contains database size/count, automatic retention and confirmed one-off cleanup tools.

The top row shows the total number of saved receptions and the main SQLite file size in MB (1,000,000 bytes). Both update in the background and cover all saved history, independently of table/map filters. SQLite's active journal uses additional disk space; the displayed size is the main database file.

Open **Settings > History > History DB** for independent, one-off cleanup of this plugin's SQLite history and all known reception CSV logs. Choose Delete before/after/between local dates, Delete selected country, Keep only selected country, Delete records with no distance, or Delete records with no grid. Other table/map filters do not affect these operations. Keeping one country also deletes unknown countries.

Enter `YYYY-MM-DD` or `YYYY-MM-DD HH:MM:SS`, in the computer's time zone. A date alone means midnight for Before/Start and the end of the day for After/End. Between includes both endpoints. Local calendar days respect daylight-saving changes. Invalid dates and times skipped or repeated during a clock change are rejected; choose an unambiguous time. Missing distance means no numeric distance; the optional cached-grid checkbox also removes distances based on earlier grid evidence. Missing grid means an empty saved grid, keeping received and cached grids.

**Review deletion** counts matching database receptions in the background. The confirmation shows the exact rule and affected files. Confirm to update the database, logs, maps, tables and saved statistics; receptions added after the preview are kept. These actions do not add ignore rules. CSV cleanup and SQLite VACUUM run in the background, preserve reception IDs and retry interrupted cleanup after restart.

## Filters, clear and logs

**Automatic database limit:** Settings > History > History DB defaults to 100,000 receptions. Crossing the limit starts small background deletion batches, keeping the newest 95,000 at the start of that cleanup plus later arrivals. This leaves room between cleanups. You can set Start cleanup above from 10,000 to 1,000,000 and Keep newest after cleanup from 1 to one less than that limit, or turn automatic retention off. Press Enter to apply a typed count. Defaults remain 100,000 and 95,000. Larger limits make broad searches and custom sorting slower. The limit balances retained history and query speed; see [history performance](HISTORY_PERFORMANCE.md).

The threshold is checked every 30 minutes, immediately at startup, and when the retention policy changes. The check reads the stored record count and file-size metadata; it does not scan reception history. File size is shown in History DB. Small temporary overruns are expected between checks and while cleanup catches up.

Cleanup processes at most 64 records at a time, with at least 250 ms between maintenance steps. Reception writes and CSV sync run first. It does not wait for the decoder to become idle, so continuous reception cannot starve maintenance. Only records already passed by the current CSV append watermark are eligible. Records added after a cleanup starts are kept. Each batch commits its record deletions, summary updates and progress together; an interrupted batch rolls back and completed progress resumes after restart. Storage failures are shown in History DB, pause cleanup and retry after 30 seconds. Older legacy input cannot reimport records already expired by automatic cleanup. Long sessions use the same rolling limit; waking from sleep causes one check rather than a burst of missed checks.

History DB shows the actual oldest retained reception and the count automatically deleted. After the first automatic deletion, the existing table legend also shows the saved-history start date. Statistics, tables and maps describe retained records, rather than a lifetime total. Session filters still apply within retained history. First-heard times can move forward as older records expire. Session uptime is unchanged. CSV logs retain their own size policy; automatic database cleanup does not delete CSV records.

Freed database pages are reused. New databases, and databases compacted by History DB cleanup, also return free pages to disk in small incremental steps. An existing database is not rebuilt automatically while receiving. One-off History DB cleanup still compacts it after deletion and enables incremental compaction for later maintenance.

One shared filter bar sits above the map and all table tabs with Last hour, Last 6 hours, Last 24 hours, Last 7 days and Last month, plus shorter windows. Last month is the rolling last 30 days. The adjacent **All** button clears the time limit for both views; other filters remain. **Reset** clears all reception filters and the time limit without changing the saved-history scope. The bar fits on one row when space permits and wraps only as needed. Without saved history enabled, a long window still covers only this session.

Band, FT8/FT4, message-type, search and advanced filters apply to the map and every table tab together. Settings > Filters edits the same shared filters. Existing table filters are retained on upgrade and become the shared selection. CQ/general call includes CQ and QRZ. Two-way exchange means a message addressed to another station. Filters provide Sender, Target, Mod, prefix, country, continent, grid, band, dial frequency, message text, protocol meaning and exclusions. Column value ranges add optional minimum/maximum limits for SNR, distance, dial MHz, audio Hz, signed DT, bearing, reception count, Msg age and both first-heard ages. Age limits use seconds ago and update with time. Unknown values fail an enabled range; limits are inclusive. Text, band, mode and existing reception filters select matching receptions before choosing the latest row. Column value ranges then test the displayed row, so they never substitute an older matching decode. Reception count counts matching receptions; first-heard times retain their session/history scope. Alerts use reception values and omit station-count/age ranges. Reception filters always apply to both views.

**Clear** asks for confirmation. **Reset stats** is the first option and starts checked. Select table, map or both. Clearing hides prior rows and starts new visible first-heard times; it keeps the reception database and CSV. Reset stats turns off both saved-history options, so totals return to zero. Without Reset stats, earlier rows are hidden only in selected views and saved history still contributes to enabled history statistics. Turn Use saved history for everything off and on to restore all retained rows. Brown restart starts empty views by default; enabling history for everything restores retained rows instead. Settings > History also has individual restore buttons.

Settings > History selects the CSV location and maximum megabytes. At the limit, the oldest complete CSV records are trimmed; Clear does not trim it. CSV supports commas, quotes and embedded line breaks. Displayed and copied times use the computer's time zone; reception logs, stored epochs and GridTracker packets keep UTC for interoperability. It includes UTC date/time, reception ID, identities, country, grid evidence, band/mode, frequencies, SNR, DT, distance, bearing and full message/raw text. The retained SQLite database is separate from the CSV size limit.

## GridTracker and alerts

In GridTracker, configure a local WSJT-X UDP listener on port 2237. In Settings > Integrations, enable the feed using the filled-in `127.0.0.1:2237` values. The plugin sends receive-only heartbeat, status and decode packets, with one identity per dial/mode. It never transmits. A recent GridTracker reply confirms communication; sent packet counts alone do not prove delivery.

Recent live receptions can replay when enabled or on GridTracker's request. The default window is 30 minutes, limited to 5000 entries within the current UTC day. Imported history and recording playback are excluded. Stop a separate helper feed if it supplies the same receptions, to avoid duplicate sources. [GridTracker setup reference](https://docs.gridtracker.org/latest/Appendices/Appendix-B-Configuring-WSJT-X-and-JTDX-for-GridTracker.html).

Sound alerts start off. Settings > Alerts selects events and individual macOS sounds. Preview plays only when clicked. Enabled alerts can sound with the decoder window closed; a cooldown limits repeated alerts.

## DX activity bearing

The top statistic is labeled **DX ACTIVITY BEARING**. It supports FT8, FT4, WSPR, FST4, FST4W, Q65 and JS8. Received/cached station grids take priority. With **Approximate unknown distances based on country** enabled, gridless receptions can use the same nearest-country grid as the displayed distance. `[C]` before the bearing means at least one sampled station uses this coarse fallback. Hover to see the sample, individual `[C]` distances and country-estimate count. The station may be anywhere in that country, so an estimated bearing is less reliable than a received grid. Missing/ambiguous countries and unresolved senders remain unknown. Turning country estimates off removes these fallback inputs immediately.

The statistic takes short-path bearings from your home grid to the farthest matching unique stations and calculates their equal-weight circular mean. The bearing and compass use the shared reception filters, selected time window and session/saved-history scope, without a hidden age limit. For example, All plus WSPR with Use saved history for everything includes the farthest retained WSPR stations and new live receptions. Last 15 minutes plus 40m uses matching 40m receptions from those 15 minutes; Last 5 minutes uses only five minutes. Older custom date ranges use their matching historical receptions. No matching stations means no eligible station remains under the current filters and scope. The tooltip shows the selected time filter, session/history scope and each sampled station's local reception date/time. Five farthest stations are used by default; the station count is adjustable in Reception settings. All shared filters apply, including band, mode, message type, search and numeric ranges. Current capture bands and alert preferences do not replace these filters. A normalized resultant below 0.5 displays mixed directions instead of one unreliable bearing. The compact compass fits within the existing statistic height; its red needle points toward the estimated bearing from true north and the white end points opposite. Mixed or unknown bearings show no needle. Station details also show the compass direction after the degree bearing.

This measures heard geographic activity during the selected period. A historical sample describes past receptions rather than current conditions. Antenna pattern, transmit power, band, sampling, long-path propagation and ionospheric conditions can all affect it. It is not a measured arrival azimuth or proof that a particular propagation direction is open.

## Data and privacy

The default data directories are `~/Library/Application Support/SDR++Brown DX Notebook/` for Brown and `~/Library/Application Support/SDR++ DX Notebook/` for standard SDR++. The SQLite history, CSV and tile cache stay local. Online maps contact their tile providers; callsign lookups open the selected website in your default browser. GridTracker UDP is off until enabled. There is no account, product analytics or automatic log upload.

## SDR++ and SDR++Brown

Both hosts are supported through separate matching binaries. Use the native installer to select the detected application; each host keeps a separate profile, reception database and CSV log. Brown's integration remains native. Standard SDR++ uses standard host APIs and does not require Brown to be installed.

Set the home locator in the installer, in Brown's **Source > My Grid**, or in standard SDR++'s plugin **Settings > General > Maidenhead grid**. Without a valid home locator, the standard build leaves the home marker, distances and bearings unknown while station-grid mapping and decoding continue. Four- and six-character locators work in both; the standard build also accepts eight.

Standard SDR++ timestamps live capture against the Mac's UTC clock. Keep the computer clock synchronized. Its file source lacks Brown's recorded-stream timestamp API, so replay timestamps describe when the samples were processed rather than their original recording date.

## Remove likely falses

**Remove likely falses**, beside the history checkboxes in the main window, starts off and remembers its state. It hides unresolved sender hashes, such as `<2:675541>` or `<...>`, and suspected country/grid mismatches from existing entries and new arrivals in the map and all tables, including Countries heard. Reception statistics, Latest heard, country highlights, DX activity bearing and distance-record notifications also exclude those receptions; uptime remains elapsed receiver time. An unresolved target alone does not hide an identified sender. A sender hash that resolves unambiguously to a full callsign within the existing 12-minute window uses the normal country/grid check; bracketed full callsigns are not treated as unresolved hash placeholders. The location check uses the callsign's decoded DXCC country and bundled Natural Earth country outlines. A point inside any matching outline, or within 300 km of its border plus half the grid-cell diagonal, is kept. Settings > Filters adjusts the border tolerance from 0 to 2,000 km. Identified senders with missing grids or unmapped countries, and explicit portable calls containing `/`, are kept. Cached grids are checked too. Portable/remote stations and unresolved hash receptions can be legitimate; this confidence filter can hide valid receptions. Saved history and ignore rules are unchanged. Unchecking it restores those receptions to views and statistics within the selected session or saved-history scope.

## Restore defaults

**Restore defaults...** stays visible at the bottom of Settings. Its confirmation resets plugin preferences, modes, filters, table widths/order/visibility, sorting and map view. Reception history, CSV files, ignore rules and the host's home setup are not cleared by the reset. Normal automatic retention returns to 100,000 / 95,000 database records and 100 MB for CSV, so older data beyond those limits may subsequently be removed by background cleanup.

## JS8 reception

For JS8, enable the mode in **Modes...** or **Settings > Reception**, then tune a capture covering its dial frequency. **Speed: All** receives Normal, Fast, Turbo / JS8 40 and Slow together. JS8 starts off and keeps your choice. Its map marker is an octagon. Longer messages update as frames arrive; **incomplete** means a final frame, required checksum or preceding identity frame is missing. Unattributed fragments remain visible without a guessed sender or country. Details show the speed and completeness, alongside the raw received frames. See [JS8 reception](MULTIMODE.md#js8-reception) for limits.

## Color based on SNR

The checkbox is in **Settings > Map** only. It starts on and is remembered. You can turn it off at any time. When enabled, each map circle uses the latest receiver SNR represented by that marker: red at -30 dB, yellow at -10 dB and green at +10 dB. Colors interpolate continuously; values outside this fixed range use its endpoint colors. The map shows a gradient legend, and tooltips retain the exact numeric SNR. Latest-reception halos and gold selected-station outlines stay separate. This is a visual scale for the decoder's received SNR estimate, not a calibrated propagation or signal-quality grade. Country shading and table colors are unchanged.
