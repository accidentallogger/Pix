## PixelForge - Minimal Pixel Editor

A lightweight, SDL2-based pixel art editor designed for creating sprites and small animations. Perfect for game developers and digital artists.

### Features
- 🎨 Basic drawing tools (pencil, eraser, eyedropper, fill)
- 🖌️ Customizable color palette
- 📺 Canvas with zoom & pan
- 🎞️ Multi-frame animation support
- ⏪ Undo/redo system
- 📤 PNG export (single frames & spritesheets)
- 💾 Project save/load

### Quick Start
```bash
# Install dependencies (Ubuntu/Debian)
sudo apt install libsdl2-dev libsdl2-ttf-dev

# Compile
g++ main.cpp -O2 -std=c++17 -lSDL2 -lSDL2_ttf -o pixelforge

# Run
./pixelforge
```

### Controls
- **P** - Pencil tool
- **E** - Eraser tool  
- **I** - Eyedropper tool
- **F** - Fill tool
- **Z** - Undo (Ctrl+Z)
- **Y** - Redo (Ctrl+Y)
- **Mouse Wheel** - Zoom in/out
- **Middle Mouse** - Pan canvas
- **Left/Right Arrows** - Navigate frames
- **Ctrl+S** - Save project
- **Ctrl+O** - Load project

### File Formats
- `.pxl` - Native project format
- `.png` - Export single frames or spritesheets

Perfect for creating game assets, icons, and pixel art animations!