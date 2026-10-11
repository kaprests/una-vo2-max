# Performance Stats App (UNA Watch)

## Setup

Requires [mise](https://mise.jdx.dev). Linux (Arch or Debian/Ubuntu).

```sh
mise trust
mise bootstrap   # OS packages, una-sdk clone, tools, LVGL submodule + Python venv
mise doctor      # optional health check
```

`UNA_SDK`, the ARM toolchain and the venv are set automatically inside this directory.

### Individual steps

```sh
mise bootstrap packages status   # which OS packages are missing
mise bootstrap packages apply    # install only the OS packages (asks for sudo)
mise bootstrap repos apply       # clone una-sdk at the pinned tag
mise install                     # ARM toolchain, cmake, uv
mise run bootstrap               # LVGL submodule + uv sync
```

`mise bootstrap` also applies any `[bootstrap]` sections in your global mise config.
To apply only this project's parts: `mise bootstrap --only packages,repos,tools,task`.
Package declarations always merge with the global config; to install strictly this
project's packages, hide it:
`MISE_GLOBAL_CONFIG_FILE=$(mktemp --suffix .toml) mise bootstrap packages apply`.

## New app (once)

Scaffold from any TouchGFX example or tutorial in the SDK
([docs](https://www.developers.unawatch.com/latest/sdk-setup.html#creating-new-apps)):

```sh
mise run new-app MyApp --from una-sdk/Examples/Apps/Alarm
```

## Build and simulate

```sh
mise run build   # .uapp -> Output/
mise run sim     # build + run the Linux simulator (keys 1-4 = L1 L2 R1 R2, 5 = wrist)
mise run clean
```

- Firmware build: https://www.developers.unawatch.com/latest/sdk-setup.html
- Linux simulator: https://www.developers.unawatch.com/latest/Simulator.html#linux-gcc
