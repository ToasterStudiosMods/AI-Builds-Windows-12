# Wallpapers

Drop a file named **`wallpaper.png`** in this folder and rebuild — the ISO build
converts it to a raw framebuffer image, ships it as a GRUB Multiboot2 module,
and the kernel blits it as the desktop background.

- Any resolution / aspect ratio works: it's scaled to **1024×768** with a
  center-crop (`-resize 1024x768^ -extent 1024x768`), the desktop's native mode.
- If no `wallpaper.png` is present, the build generates a default gradient.
- PNG or JPG both work (rename to `wallpaper.png`, or adjust the Makefile).

So: generate a wallpaper (e.g. with Nano Banana), save it here as
`wallpaper.png`, commit, and the next CI build bakes it into the OS.
