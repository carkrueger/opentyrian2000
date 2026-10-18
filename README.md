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

Configuration and saved games are stored in the browser (IndexedDB).  Network
play is not available.  The page around the game is in `web/`.

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
