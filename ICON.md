# Icon Assets

FlipBitChat requires an icon for the Flipper Zero app launcher.

## Icon Specifications

- **Format**: Monochrome bitmap (1-bit color depth)
- **Size**: 10x10 pixels (for `A_BitChat_14` reference in app manifest)
- **File format**: `.png` (will be converted to Flipper's internal format)

## Creating Icons

1. Create a 10x10 pixel monochrome PNG image
2. Name it `icon.png`
3. Place in this directory
4. The build system will convert it to Flipper format

## Suggested Icon Design

Given BitChat's mesh networking focus, consider:
- Mesh network node pattern (connected dots)
- Chat bubble with mesh lines
- Bluetooth symbol with chat elements
- Simple "BC" monogram

## Tool Recommendations

- [Piskel](https://www.piskelapp.com/) - Online pixel art editor
- GIMP - Export as 1-bit PNG
- ImageMagick - Convert to required format:
  ```bash
  convert icon.png -monochrome -depth 1 icon_mono.png
  ```

## TODO

- [ ] Create icon.png (10x10 monochrome)
- [ ] Update application.fam with correct icon reference
- [ ] Test icon in Flipper launcher menu

## Current Status

Icon is currently using placeholder reference `A_BitChat_14` which needs to be created.
