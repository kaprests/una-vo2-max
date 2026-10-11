# Project structure

This app was scaffolded from the SDK's RunLVGL example and keeps the SDK's
example layout on purpose.

## Layout

```
performance_stats_app/
├── mise.toml, mise-tasks/new-app   # tooling, tasks, scaffold script
├── Resources/                      # icon_30x30.png, icon_60x60.png (packed into the .uapp)
├── Output/                         # firmware artifacts: VO2Max_<ver>.uapp, *.map (gitignored)
└── Software/
    ├── Libs/                       # service code + headers shared with the GUI
    │   ├── Header/common/          # Commands, Settings, Track, ActivitySummary (service <-> GUI contract)
    │   ├── Header/service/         # Service, ActivityWriter, serializers, WristTiltDetector
    │   ├── Sources/                # service sources
    │   └── libs.cmake              # -> LIBS_SOURCES, LIBS_{COMMON,SERVICE,}_INCLUDE_DIRS
    ├── Apps/
    │   ├── VO2Max-CMake/           # build entry 1: firmware (CMakeLists.txt only)
    │   └── LVGL-GUI/               # GUI source module ...
    │       ├── gui/{include,src}/  #   screens, widgets, model, theme, GuiApp
    │       ├── assets/             #   fonts/images as generated .c + gen_assets.py
    │       ├── lvgl-gui.cmake      #   -> GUI_APP_SOURCES, GUI_APP_INCLUDE_DIRS
    │       └── simulator/          # ... and build entry 2: host simulator
    └── Output/                     # simulator mock file system (gitignored)
```

| | Firmware | Simulator |
|---|---|---|
| Entry | `Software/Apps/VO2Max-CMake/CMakeLists.txt` | `Software/Apps/LVGL-GUI/simulator/CMakeLists.txt` |
| Target / compiler | ARM watch, `arm-none-eabi-gcc` | x86-64 Linux, host GCC + SDL2 |
| Service + GUI | two ELFs packed into `Output/VO2Max_<ver>.uapp` | one executable, `simulator/build/bin/VO2MaxSimulator` |
| Kernel | real UNA kernel | SDK mock kernel |

Both builds compile the same sources: `Software/Libs` and `LVGL-GUI/{gui,assets}`.

## Rules

- **Service–GUI boundary.** GUI code may only include headers from
  `Libs/Header/common`. The firmware GUI build has only that directory on its
  include path, so including a service header there fails to compile. The
  simulator builds both processes as one target and does not enforce it.
- **New code goes where the SDK expects it.** Service code in `Libs/Header/service`
  and `Libs/Sources`; shared message and data types in `Libs/Header/common`; GUI
  code in `LVGL-GUI/gui`.

## Why keep the SDK layout

A cleaner layout (separate `firmware/`, `simulator/`, `src/{common,service,gui}`)
was considered and rejected for now:

- Every SDK example, tutorial and doc uses this layout, so copying from them
  stays a direct copy.
- The simulator's file system path is hard-coded relative to the working
  directory (`Libs/Source/Simulator/Kernel/Kernel.cpp`: `../../../../../Output/`),
  which a different layout would have to work around.
- SDK updates to the CMake helpers merge without translation.

Reconsider if the app grows into several modules, shares code with another app,
or if the SDK publishes an official blank app template with a different layout.
