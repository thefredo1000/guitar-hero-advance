# Guitar Hero Advance

A Game Boy Advance rhythm game project built with [Butano](https://github.com/GValiente/butano).

## Project Structure

- `src/`: Gameplay and scene logic.
- `include/`: Headers and data definitions.
- `audio/`: Music modules used by Butano.
- `graphics/`: Sprite and background assets.
- `scripts/`: Utility scripts.

## Build

Run from this folder:

```sh
make
```

This generates the ROM build artifacts locally.

## Notes

- See `CLONE_HERO_TO_GBA_GUIDE.md` for conversion workflow and content pipeline details.
- Keep backup and experimental audio outside `audio/` if they are not intended for Butano processing.
