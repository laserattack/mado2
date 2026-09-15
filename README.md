# mado2 — markdown organizer

In progress...

A working Linux only version (the predecessor) is available at
[laserattack/mado](https://github.com/laserattack/mado)

# Build

Requirements:

- C++17 compiler
- Meson + Ninja

Build:

```
meson setup build
meson compile -C build
```

# License

This project is distributed under the WTFPL, except for the vendored
utf8proc library, which is distributed under its own licenses (MIT and
Unicode Data License). See
[code/libmado/3rdparty/utf8proc/LICENSE.md](code/libmado/3rdparty/utf8proc/LICENSE.md)
