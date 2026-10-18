# OpenTyrian2000

<img src="linux/icons/tyrian2000-128.png" width="128" height="128" align="right" alt="OpenTyrian2000 icon">

OpenTyrian2000 is an open-source port of the DOS game Tyrian.  It is a fork of
[OpenTyrian](https://github.com/opentyrian/opentyrian), with the end goal being
to replicate the experience of Tyrian 2000 as closely as possible.

Tyrian is an arcade-style vertical scrolling shooter.  The story is set
in 20,031 where you play as Trent Hawkins, a skilled fighter-pilot employed
to fight MicroSol and save the galaxy.

Tyrian features a story mode, one- and two-player arcade modes, and networked
multiplayer.

## Configuration

Configuration and saved game files are kept in one of the following locations:

| Platform | Location |
|---|---|
| Windows | `%APPDATA%\OpenTyrian2000` |
| macOS / Linux | `$XDG_CONFIG_HOME/opentyrian2000` or `~/.config/opentyrian2000` |

On Windows and Linux, if `opentyrian.cfg` exists in the same directory as the
executable, the configuration and saved game files will be stored there instead.

## Smooth Motion (70 Hz)

Tyrian was made for the 70 Hz VGA mode of DOS: the game runs at about 34.8
frames per second in normal speed (every second display refresh) and at about
14 frames per second in Slug Mode (every fifth).  On a 60 Hz display these
frame rates do not divide evenly into the refreshes, so some frames stay on
screen longer than others, and motion looks uneven; the slower the game speed,
the more noticeable this is.  Running the display at 70 Hz with VSync makes
every frame stay on screen equally long again.

The relevant settings are in **Setup > Graphics** (and in the `video` section
of `opentyrian.cfg`):

| Setting | `opentyrian.cfg` | Description |
|---|---|---|
| Full-Screen | `fullscreen_mode` | `Desktop` (borderless window at the desktop's resolution and refresh rate) or a full-screen mode such as `3840x1600@70`, which switches the display to that resolution and refresh rate.  The menu offers the desktop's resolution at every refresh rate and any mode at about 70 Hz (or a multiple of it). |
| VSync | `vsync` | Presents each frame in step with the display's refresh.  Turn it on for even motion. |
| Renderer | `renderer` | The graphics API.  On Windows the default is `direct3d11`, which variable refresh rate needs. |

To play at 70 Hz:

1. If the display offers a 70 Hz mode, choose it under **Full-Screen**, set
   **Display** to the display, and turn **VSync** on.
2. Otherwise a 70 Hz mode can often be added, at the display's native
   resolution, as a custom resolution.  The graphics driver's control panel
   may offer this; on Windows, [CRU (Custom Resolution
   Utility)](https://www.monitortests.com/forum/Thread-Custom-Resolution-Utility-CRU)
   works with most drivers:
   1. Start `CRU.exe` and select the display.
   2. Open the existing entry for the native resolution under *Detailed
      resolutions* and note its timing (front porch, sync width, back porch,
      totals, polarity).
   3. Click *Add...*, choose the *Manual* timing, enter the same values, and
      set the refresh rate to 70 Hz.  Keeping the totals and only raising the
      pixel clock usually works if the display supports more than 70 Hz at
      that resolution; check that the horizontal frequency and pixel clock
      stay within the display's limits (shown in CRU's *Range limits*).
   4. Run `restart64.exe` (in the CRU folder) to restart the graphics driver,
      then choose the new mode under **Full-Screen**, or set it as the
      desktop's refresh rate and keep **Full-Screen** at `Desktop`.

   If the display stays black, wait: Windows reverts an unconfirmed mode
   change after 15 seconds.  `reset-all.exe` followed by `restart64.exe`
   removes all changes made with CRU.  Some drivers (Intel's in particular)
   may ignore custom resolutions.
3. 70 Hz suits both speeds well: in normal speed a frame stays on screen for
   3 refreshes instead of 2 about every 2 seconds, and Slug Mode (14.04
   frames per second, 5 refreshes each) fits almost exactly.  For a perfect
   fit in normal speed, use 69.56 Hz instead.

Variable refresh rate (FreeSync, G-Sync, Adaptive-Sync) only helps if the
display's range reaches down to the game's frame rate, or if it can repeat
frames below its range (Low Framerate Compensation, which needs a range whose
maximum is at least about twice its minimum).  Displays with a range of, for
example, 48 to 75 Hz cannot do that, so a fixed 70 Hz is the better choice
there.

## Game Data

OpenTyrian2000 requires the Tyrian 2000 data files, which have been released
as freeware.  Download [Tyrian 2000](https://www.camanis.net/tyrian/tyrian2000.zip)
(or run `./get_data.sh`) and extract the archive so that the files (with
lowercase filenames) are in one of the following locations, searched in order:

1. the directory given with `--data=DIR`
2. a `data` directory next to the executable (inside `Contents/Resources`
   for the macOS app)
3. the system directory the build was configured with
   (`/usr/local/share/games/tyrian` by default; `C:\TYRIAN` on Windows)
4. the current directory

## Building

Requirements: a C99 compiler, GNU make, pkg-config, SDL2, and, for network
play, SDL2_net.

    make

Network play is enabled automatically when SDL2_net is found.

A Visual Studio solution is provided in `visualc/`.

### Web Build

OpenTyrian2000 can be built as WebAssembly to run in a web browser, with the
[Emscripten SDK](https://emscripten.org/docs/getting_started/downloads.html):

    ./get_data.sh data
    emmake make

This produces `opentyrian2000.html`, `.js`, `.wasm`, and `.data` (the game
data, bundled from `data/`; another directory can be given with
`DATA_DIR=dir`).  Serve them from a web server, for example with
`python3 -m http.server`, and open `opentyrian2000.html`; browsers do not run
WebAssembly from `file://` pages.

Configuration and saved games are stored in the browser (IndexedDB).  While
the game is not running (before starting it, or after quitting), saved games
can be downloaded as `tyrian.sav`, the same format as Tyrian 2000's
`TYRIAN.SAV`, and uploaded from such a file or from playtyrian.com's
`opentyrian2000_saves.json`.  Network play is not available.  The page around
the game is in `web/`.

The `Web` GitHub Actions workflow builds the same files and can publish them
with GitHub Pages: enable Pages with *GitHub Actions* as the source and set the
repository variable `DEPLOY_WEB` to `true`.

## Command-Line Options

    -h, --help                   Show help about options
    -s, --no-sound               Disable audio
    -j, --no-joystick            Disable joystick/gamepad input
    -x, --no-xmas                Disable Christmas mode
    -X, --xmas                   Enable Christmas mode
    -t, --data=DIR               Set Tyrian data directory
    -n, --net=HOST[:PORT]        Start a networked game
    --net-player-name=NAME       Set local player name in a networked game
    --net-player-number=NUMBER   Set local player number in a networked game
                                 (1 or 2)
    -p, --net-port=PORT          Set local port to bind (default is 1333)
    -d, --net-delay=FRAMES       Set lag-compensation delay (default is 1)

## Network Multiplayer

Currently OpenTyrian2000 does not have an arena; as such, networked games must
be initiated manually via the command line simultaneously by both players.

    opentyrian2000 --net HOSTNAME --net-player-name NAME --net-player-number NUMBER

where `HOSTNAME` is the IP address of your opponent, `NUMBER` is either 1 or 2
depending on which ship you intend to pilot, and `NAME` is your alias.

OpenTyrian2000 uses UDP port 1333 for multiplayer, but in most cases players
will not need to open any ports because OpenTyrian2000 makes use of UDP hole
punching.

Note that network play has not been tested for OpenTyrian2000.

## Links

For OpenTyrian2000:

- project: <https://github.com/KScl/opentyrian2000>

For OpenTyrian:

- project: <https://github.com/opentyrian/opentyrian>
- irc:     <ircs://irc.oftc.net/#opentyrian>
- forums:  <https://tyrian2k.proboards.com/board/5>
