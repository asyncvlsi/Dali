# Dali command language

Dali supports three execution styles built on the same command processor:

1. The ordinary CLI runs the default placement flow.
2. A `.dali` recipe runs a reproducible sequence of commands.
3. Interactive mode accepts those commands from a terminal.

Existing command lines remain unchanged:

```sh
dali -lef design.lef -def design.def -o placed
```

## Recipes

Pass a recipe with `-script` or its alias `-command_file`:

```sh
dali -lef design.lef -def design.def -script placement.dali -o placed
```

An explicit recipe owns stage execution, so include `run placement` when the
full placement flow should run:

```text
# dali-script 1
set target_density 0.70
set num_threads 4
set is_standard_cell true

run placement
check-io
```

Commands execute in order and a recipe stops at its first error. Blank lines,
`#` comments, single or double quotes, backslash escaping, and a trailing `\`
for line continuation are supported. A recipe can include another recipe:

```text
source "common_settings.dali"
```

## Interactive mode

Open an existing placement without running placement implicitly:

```sh
dali -lef design.lef -def placed.def -interactive -o signed_off
```

The session is recoverable: a failed command is reported and the next prompt
still appears. This supports inspect, edit, and recheck signoff loops:

```text
dali> show-io clock
dali> check-io
dali> move-io clock 120.0 400.0 N
dali> check-io
dali> history
dali> quit
```

`quit`, `exit`, or end-of-file finishes the session and exports the current
design through the normal Dali output path.

Run a recipe first and then retain control:

```sh
dali \
  -lef design.lef \
  -def design.def \
  -script placement.dali \
  -interactive \
  -o signed_off
```

When `-gui_debug` is also present, the terminal remains the command surface and
Qt remains the visualization surface. Successful placement-changing commands
publish a fresh snapshot, and the GUI stays open until the interactive session
ends and its window is closed.

## Commands

Flow control:

- `set <option> <value>` configures a later placement run.
- `show settings` reports the resolved runtime settings.
- `run placement` runs the configured placement pipeline.
- `source <file.dali>` executes another recipe.
- `help` reports the command list.
- `history`, `quit`, and `exit` are available at the interactive prompt.

Manual I/O signoff:

- `show-io [pin]` reports current pin placement.
- `move-io <pin> <x> <y> [orientation]` moves and fixes a pin in microns.
- `unfix-io <pin>` releases a pin for later automatic placement.
- `check-io` runs Dali's lightweight placement checks.
- `place-io ...` exposes automatic, constrained, grouped, mirrored, and
  area-array I/O placement.

The legacy `place-design <density> [threads]`,
`global-place <density> [threads]`, and `add-welltap ...` commands remain
available for existing integrations.

Supported `set` options are listed in the top-level
[README](../../README.md#command-recipes). Boolean values accept
`true`/`false`, `on`/`off`, and `1`/`0`.

## Embedding

Command hosts should use Dali's public API instead of implementing a second
parser or placement path:

```cpp
bool ExecuteCommand(const std::vector<std::string>& arguments);
bool ExecuteCommandLine(const std::string& command_line);
bool RunCommandFile(const std::string& file_name);
```

`ExecuteCommand` accepts both `place-io ...` and namespaced
`dali:place-io ...`. This lets a future `interact` adapter forward argv-style
commands directly:

```text
dali:init 3
dali:source "io_flow.dali"
dali:export-phydb
dali:close
```

The `.dali` file remains the portable placement recipe; the host language only
manages Dali's lifecycle and data exchange.
