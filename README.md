# fpp-RandomSongPicker

An FPP plugin that picks a random, non-recently-played song from a source playlist. Unlike FPP's built-in random selection, this plugin tracks play history so the same song is not repeated until a configurable number of other songs have played.

## Commands

Three FPP commands are registered and available in any playlist Command entry or under **Sequences → Command Presets**.

| Command | Args | What it does |
|---|---|---|
| **Insert Random Item with History** | Source Playlist, History Size | Picks randomly (skipping recent history) and immediately queues the item as the next entry in the calling playlist. FPP plays it, then the calling playlist resumes — Lead Out still runs. |
| **Write Random Item to Playlist** | Source Playlist, Output Playlist, History Size, Replace Previous | Picks randomly (skipping recent history) and writes the item to an output playlist. Does not start playback — add a separate Playlist entry after this command to play the output playlist. |
| **Clear Playlist Main** | Playlist | Clears only the main section of a playlist, leaving Lead In and Lead Out intact. |

## Typical setup

**Simple (play immediately):**

| Section | Entry |
|---|---|
| Lead In | Command: `WLED - Ensure Power On` |
| Main | Command: `Insert Random Item with History` — Source = your song library playlist |
| Lead Out | Command: `WLED - Turn Off` |

The plugin picks a random, non-recently-played song and FPP plays it inline before running Lead Out. No extra playlist needed.

**Write-then-play:**

| Section | Entry |
|---|---|
| Lead In | Command: `WLED - Ensure Power On` |
| Main | Command: `Write Random Item to Playlist` — Source = your library, Output = `RandomPick` |
| Main | Playlist: `RandomPick` |
| Lead Out | Command: `WLED - Turn Off` |

Useful when the output playlist has its own Lead In/Out items that need to run around the picked song.

## History

Play history is stored per source playlist at:

```
/home/fpp/media/logs/song_picker_<source>_history.txt
```

History resets automatically once every song in the source playlist has been played. Delete the file manually to reset it at any time.

## Installation

Install via the FPP Plugin Manager. The install script builds the C++ plugin and restarts `fppd` automatically.

**Requirements:** FPP 9.0+, Raspberry Pi or compatible Linux SBC.
