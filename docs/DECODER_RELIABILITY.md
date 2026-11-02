# FT8/FT4 reception reliability

Read when: changing FT8/FT4 parsing, confidence policy, location evidence or decoder depth.

## Confidence and map safety

Kondor retains the MSHV decoder's `?` flag, AP flags and relative quality score. `?` marks a low-confidence whole message. It does not identify which bits are wrong: sender, target, grid and report may all be affected. The score is not a percentage probability, a calibrated accuracy measure or SNR. An unflagged message can still be wrong.

Low-confidence FT8/FT4 receptions remain in SQLite and CSV. By default they are hidden from the reception tables. **Filters > Include questionable decodes** reveals them in Unique stations and All messages for inspection. They remain excluded from map positions, communication arcs, country summaries, statistics, bearings, alerts, waterfall notifications and GridTracker forwarding. They never seed callsign-hash matches or received-grid caches. The policy also reads flags from retained historical raw records; no history deletion is required.

The reception tables and details show **Confidence**. A number such as `0.9` is the relative score with no low-confidence marker; `Low 0.1` is flagged. `--` means the source supplies no comparable score, including the other supported modes and older records without decoder diagnostics. Sorting and numeric confidence filters use the underlying score. Existing column order, widths and visibility are preserved when the new column is appended.

**Remove likely falses** remains a separate, reversible filter for unresolved sender hashes and country/grid mismatches. It can hide legitimate portable or remote stations. Neither filter proves that a reception is false. Long distance alone is never a reason to reject a message.

## Parser and decoding policy

- Only recognized exchange forms establish FT8/FT4 station/grid identity. Unsupported text stays available as text instead of acquiring a guessed sender or cached location. Bracketed full callsigns and structured contest exchanges remain supported; unresolved hashes remain unresolved unless the existing unique recent-match rule succeeds.
- `/R` is treated as a rover suffix, not as a Russian operating prefix. Country lookup still identifies a DXCC entity rather than a precise station location.
- Helper frames require valid finite numeric fields, supported modes, complete PCM lengths and bounded parameters. Malformed output is rejected rather than partially interpreted.
- **Settings > Reception > FT8/FT4 decode depth** offers Fast, Normal and Deep. Deep is the default, matching the checked defaults in WSJT-X, SDRoxide and Brown's existing decoder. Fast reduces search effort; Normal uses moderate recovery; Deep performs more thorough searches with more CPU/time and potentially more false decodes. All three have visible descriptions. Saved choices remain respected. Existing LDPC and CRC checks and Kondor's low-confidence protections remain active at every depth. Kondor does not replace them with distance or SNR cutoffs.
- QSO-specific a priori decoding is explicitly disabled in the general monitoring helper. Upstream general-CQ AP passes can still occur, notably in FT4; AP flags are retained rather than treated as proof of an error.

## Source comparison

Reviewed source snapshots:

| Project | Commit | Relevant behavior |
| --- | --- | --- |
| [WSJT-X](https://github.com/WSJTX/wsjtx) | `967c85a6119110788b0df3782cf796485b2cee14` | Codeword/CRC checks, typed unpacking, relative quality and low-confidence handling. |
| [SDRoxide](https://github.com/dividebysandwich/sdroxide) | `dde3ece178a0dc5a6c47f55b5126b5219071c034` | Main FT8/FT4 path uses mfsk-core 0.14; configurable decoding passes/subtraction. Supplemental EU VHF code also checks CRC and message type. |
| [ft8_lib](https://github.com/kgoba/ft8_lib) | `9fec6ca39886edbf96f4f5e71edc76da5074e871` | LDPC/CRC validation and typed 77-bit message unpacking. |
| [SDR++Brown / MSHV adaptation](https://github.com/sannysanoff/SDRPlusPlusBrown) | `679f48deee5299a199d15d30e6e785fac6054896` | Kondor's pinned FT8/FT4 decoder; supplies `?`, AP flags and a rounded quality score. |

The [WSJT-X user guide](https://wsjt.sourceforge.io/wsjtx-main_en.html) describes low-confidence decodes and excludes them from PSK Reporter spots. Kondor applies a conservative map/statistics policy for the same reason: doubtful identity or grid data should not establish a geographic result. Defaults differ across applications; SDRoxide defaults to Deep. No claim of equal sensitivity follows from source comparison.

## Validation and limits

The investigation used read-only online backups of reception history. Of 171,545 retained FT8/FT4 receptions in the larger snapshot, 5,960 carried the upstream low-confidence marker. This is a marker count, not a measured false-decode rate. The history also reproduced a free-text identity-parsing bug. A coherent, repeated New Caledonia reception remained accepted as the farthest station after confidence filtering.

Tests cover historical flags, malformed output, supported identities/exchanges, cache isolation, reversible inspection, confidence sorting/ranges, CSV preservation, alerts and forwarding. Generated FT8/FT4 structured and free-text audio runs at all three depths, with deterministic noise-only slots and isolated temporary storage.

Reception text cannot establish whether a radio decode was correct. IQ/audio recordings and matched reference-decoder settings are required to compare sensitivity or investigate an individual unflagged distant decode. Strong synthetic fixtures and a few noise slots do not measure real-world false-alarm rates, crowded-band sensitivity or antenna performance.

Local validation on 2026-11-02: all 39 checks passed for Brown, standard SDR++ and the AddressSanitizer build. The affected storage/pipeline checks were repeated after the metadata migration; focused checks also passed after the final contest-parser adjustment. An independent ft8_lib build decoded the generated structured and free-text fixtures in both modes. The isolated SDR++ UI showed confidence values, low-confidence details and trusted-only statistics; its synthetic database/CSV were removed after shutdown. Native Windows targets cross-compiled; this does not constitute Windows runtime validation. Original Brown and Beige reception IDs/payloads still matched their online backups, and both databases passed SQLite quick_check.
