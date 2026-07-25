# Wallpapers

Put images in **`assets/wallpapers/`** (`.jpg`, `.jpeg`, or `.png`). Each one is
converted at build time to a raw 1024×768 framebuffer image, shipped as a GRUB
Multiboot2 module (`wp0`, `wp1`, …), and made selectable in the **Settings**
app's wallpaper picker at runtime. `wp0` (alphabetically first) is the default.

- Any resolution / aspect ratio works: images are scaled to 1024×768 with a
  center-crop (`-resize 1024x768^ -extent 1024x768`), the desktop's native mode.
- If the folder is empty, the build generates a single default gradient.
- Conversion needs ImageMagick (`convert`), installed in CI.

So: generate wallpapers (e.g. with Nano Banana), drop them in
`assets/wallpapers/`, commit, and the next build bakes them all in — switchable
from Settings.
