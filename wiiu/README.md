# Taisei Project for Wii U (experimental)

Taisei's OpenGL 3.3 renderer running on [gx2gl](https://github.com/ExpansionPak/gx2gl),
an OpenGL implementation on top of GX2. Needs Aroma.

## Installing

Copy the contents of the `sd` folder to the root of the SD card:

    sd:/wiiu/apps/taisei.wuhb
    sd:/wiiu/apps/taisei/data/00-taisei.zip      (the game data)
    sd:/wiiu/apps/taisei/data/gamecontrollerdb.txt

Saves, replays and settings go to `sd:/wiiu/apps/taisei/storage`.

## Problems

Every run writes `sd:/wiiu/apps/taisei/taisei.log`, with a backtrace if
the game crashes. Attach it to bug reports.
