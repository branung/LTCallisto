# Lua 5.4.6, vendored

The library sources of Lua 5.4.6 (`src/`: everything in the release's `src/`
except `lua.c`, `luac.c` and the `Makefile`), **unmodified**. The mod layer
embeds it to run a mod's `Mod\Script.lua`.

- Source: <https://www.lua.org/ftp/lua-5.4.6.tar.gz>
- SHA-256 of that archive:
  `7d5ea1b9cb6aa0b59ca3dde1c6adcb57ef83a1ba8e5432c0ecd06bf439b3ad88`, the value
  lua.org publishes for it.
- License: MIT, Copyright (C) 1994-2023 Lua.org, PUC-Rio. The full notice is
  at the end of `src/lua.h`.

Do not edit these files. To update, replace `src/` with a later release's,
leave out the same three files, and record the new archive and hash here.
