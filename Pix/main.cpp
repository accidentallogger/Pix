// main.cpp
// Full-featured minimal pixel editor (SDL2 + SDL2_ttf + stb_image_write)
// - Tools: pencil, eraser, eyedropper, fill
// - Palette, toolbar, grid, checkerboard
// - Zoom & pan, frames, onion-skin, undo/redo
// - Export spritesheet PNG
//
// Requires: SDL2, SDL2_ttf
//
// Compile (Linux):
// g++ main.cpp -O2 -std=c++17 -lSDL2 -lSDL2_ttf -o pixel_editor

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <vector>
#include <string>
#include <cstring>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <algorithm>
#include <chrono>
#include <deque>
#include <fstream>
#include <iostream>
#include "stb_image_write.h"

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>

using namespace std::chrono;

// ----------------- Config -----------------
const int WINDOW_W = 1100;
const int WINDOW_H = 720;

int CANVAS_W = 64; // pixels (editable at top of file)
int CANVAS_H = 64;

int DEFAULT_PIXEL_SIZE = 10; // default zoom scale (can be changed by wheel)
int MIN_PIXEL_SIZE = 2;
int MAX_PIXEL_SIZE = 40;

const int TOOLBAR_W = 140;
const int TIMELINE_H = 96;
const int STATUS_H = 26;
const int PALETTE_SWATCH = 28;

// ----------------- Utility -----------------
static inline uint32_t rgba_u(uint8_t r, uint8_t g, uint8_t b, uint8_t a = 255)
{
    return (uint32_t)r | ((uint32_t)g << 8) | ((uint32_t)b << 16) | ((uint32_t)a << 24);
}
static inline uint8_t r_of(uint32_t c) { return c & 0xFF; }
static inline uint8_t g_of(uint32_t c) { return (c >> 8) & 0xFF; }
static inline uint8_t b_of(uint32_t c) { return (c >> 16) & 0xFF; }
static inline uint8_t a_of(uint32_t c) { return (c >> 24) & 0xFF; }

static inline std::string hex_of(uint32_t c)
{
    char buf[16];
    std::snprintf(buf, sizeof(buf), "#%02X%02X%02X", r_of(c), g_of(c), b_of(c));
    return std::string(buf);
}

// ----------------- Tools -----------------
enum Tool
{
    TOOL_PENCIL = 0,
    TOOL_ERASER,
    TOOL_EYEDROPPER,
    TOOL_FILL,
    TOOL_COUNT
};

// ----------------- Frame & Sprite -----------------
struct Frame
{
    int w, h;
    std::vector<uint32_t> px; // RGBA
    Frame() : w(0), h(0) {}
    Frame(int W, int H, uint32_t fill = rgba_u(0, 0, 0, 0)) : w(W), h(H), px(W * H, fill) {}
    inline uint32_t &at(int x, int y) { return px[y * w + x]; }
    inline const uint32_t &at(int x, int y) const { return px[y * w + x]; }
};

struct Sprite
{
    int w, h;
    std::vector<Frame> frames;
    int cur_frame = 0;
    Sprite() : w(0), h(0) {}
    Sprite(int W, int H) : w(W), h(H)
    {
        frames.emplace_back(W, H, rgba_u(0, 0, 0, 0));
        cur_frame = 0;
    }
};

// ----------------- Undo / Redo -----------------
struct UndoStack
{
    std::deque<Frame> stack;
    std::deque<Frame> redo_stack;
    size_t max_size = 16;

    void push(const Frame &f)
    {
        stack.push_front(f);
        if (stack.size() > max_size)
            stack.pop_back();
        redo_stack.clear();
    }
    bool can_undo() { return !stack.empty(); }
    bool can_redo() { return !redo_stack.empty(); }
    Frame undo(const Frame &current)
    {
        if (!can_undo())
            return current;
        Frame prev = stack.front();
        stack.pop_front();
        redo_stack.push_front(current);
        return prev;
    }
    Frame redo(const Frame &current)
    {
        if (!can_redo())
            return current;
        Frame nxt = redo_stack.front();
        redo_stack.pop_front();
        stack.push_front(current);
        return nxt;
    }
    void clear()
    {
        stack.clear();
        redo_stack.clear();
    }
};

// ----------------- Global State -----------------
Sprite sprite;
int pixel_size = DEFAULT_PIXEL_SIZE;
double camera_x = 0, camera_y = 0; // pan in screen-space px
bool panning = false;
int last_mouse_x = 0, last_mouse_y = 0;

Tool cur_tool = TOOL_PENCIL;
uint32_t cur_color = rgba_u(0, 0, 0, 255);
std::vector<uint32_t> palette;

bool mouse_left_down = false;
bool mouse_right_down = false;
bool playing = false;
double play_fps = 8.0;
double play_accum = 0.0;
int play_frame_index = 0;
bool onion_skin = true;
int onion_prev = 1; // how many frames previous to show
int onion_next = 1;

UndoStack undo_stack;

// ----------------- Functions -----------------
void init_default_palette()
{
    palette.clear();
    palette.push_back(rgba_u(0, 0, 0, 255));
    palette.push_back(rgba_u(255, 255, 255, 255));
    palette.push_back(rgba_u(255, 0, 0, 255));
    palette.push_back(rgba_u(0, 255, 0, 255));
    palette.push_back(rgba_u(0, 0, 255, 255));
    palette.push_back(rgba_u(255, 255, 0, 255));
    palette.push_back(rgba_u(255, 128, 0, 255));
    palette.push_back(rgba_u(128, 0, 255, 255));
    palette.push_back(rgba_u(128, 128, 128, 255));
    palette.push_back(rgba_u(200, 100, 100, 255));
    palette.push_back(rgba_u(100, 200, 200, 255));
    palette.push_back(rgba_u(50, 50, 200, 255));
}

void ensure_sprite_initialized()
{
    if (sprite.frames.empty())
    {
        sprite = Sprite(CANVAS_W, CANVAS_H);
    }
}

// convert screen coords to canvas coords (pixel indices)
bool screen_to_canvas(int sx, int sy, int &cx, int &cy, int canvas_origin_x, int canvas_origin_y)
{
    // sx,sy are window coords
    float localx = (sx - canvas_origin_x) / (float)pixel_size;
    float localy = (sy - canvas_origin_y) / (float)pixel_size;
    cx = int(std::floor(localx));
    cy = int(std::floor(localy));
    if (cx < 0 || cy < 0 || cx >= sprite.w || cy >= sprite.h)
        return false;
    return true;
}

void flood_fill(Frame &f, int sx, int sy, uint32_t target, uint32_t repl)
{
    if (target == repl)
        return;
    if (sx < 0 || sy < 0 || sx >= f.w || sy >= f.h)
        return;
    if (f.at(sx, sy) != target)
        return;
    std::vector<std::pair<int, int>> stack;
    stack.emplace_back(sx, sy);
    while (!stack.empty())
    {
        auto [x, y] = stack.back();
        stack.pop_back();
        if (x < 0 || y < 0 || x >= f.w || y >= f.h)
            continue;
        if (f.at(x, y) != target)
            continue;
        f.at(x, y) = repl;
        stack.emplace_back(x + 1, y);
        stack.emplace_back(x - 1, y);
        stack.emplace_back(x, y + 1);
        stack.emplace_back(x, y - 1);
    }
}

// export sprite frames horizontally into PNG using stb_image_write
bool export_spritesheet(const Sprite &s, const char *path)
{
    if (s.frames.empty())
        return false;
    int W = s.w * (int)s.frames.size();
    int H = s.h;
    std::vector<uint8_t> out(W * H * 4);
    for (int fi = 0; fi < (int)s.frames.size(); ++fi)
    {
        const Frame &fr = s.frames[fi];
        for (int y = 0; y < s.h; ++y)
        {
            for (int x = 0; x < s.w; ++x)
            {
                uint32_t c = fr.at(x, y);
                int ox = fi * s.w + x;
                int idx = 4 * (y * W + ox);
                out[idx + 0] = r_of(c);
                out[idx + 1] = g_of(c);
                out[idx + 2] = b_of(c);
                out[idx + 3] = a_of(c);
            }
        }
    }
    int ok = stbi_write_png(path, W, H, 4, out.data(), W * 4);
    return ok != 0;
}

// save project (simple binary: w,h,framecount, then raw uint32 pixels per frame)
bool save_project(const Sprite &s, const char *path)
{
    std::ofstream ofs(path, std::ios::binary);
    if (!ofs)
        return false;
    ofs.write("PXL1", 4);
    int32_t w = s.w, h = s.h;
    int32_t fc = (int32_t)s.frames.size();
    ofs.write((char *)&w, sizeof(w));
    ofs.write((char *)&h, sizeof(h));
    ofs.write((char *)&fc, sizeof(fc));
    for (auto &fr : s.frames)
    {
        ofs.write((char *)fr.px.data(), sizeof(uint32_t) * fr.w * fr.h);
    }
    return true;
}
bool load_project(Sprite &s, const char *path)
{
    std::ifstream ifs(path, std::ios::binary);
    if (!ifs)
        return false;
    char hdr[4];
    ifs.read(hdr, 4);
    if (std::strncmp(hdr, "PXL1", 4) != 0)
        return false;
    int32_t w, h, fc;
    ifs.read((char *)&w, sizeof(w));
    ifs.read((char *)&h, sizeof(h));
    ifs.read((char *)&fc, sizeof(fc));
    if (w <= 0 || h <= 0 || fc <= 0)
        return false;
    s.w = w;
    s.h = h;
    s.frames.clear();
    for (int i = 0; i < fc; i++)
    {
        Frame fr(w, h);
        ifs.read((char *)fr.px.data(), sizeof(uint32_t) * w * h);
        s.frames.push_back(std::move(fr));
    }
    s.cur_frame = 0;
    return true;
}

// save PNG of current visible composited frame with checkerboard compositing
bool save_png_current(const Sprite &s, const char *path)
{
    if (s.frames.empty())
        return false;
    const Frame &fr = s.frames[s.cur_frame];
    int W = fr.w, H = fr.h;
    std::vector<uint8_t> out(W * H * 4);
    for (int y = 0; y < H; ++y)
    {
        for (int x = 0; x < W; ++x)
        {
            uint32_t c = fr.at(x, y);
            int idx = 4 * (y * W + x);
            out[idx + 0] = r_of(c);
            out[idx + 1] = g_of(c);
            out[idx + 2] = b_of(c);
            out[idx + 3] = a_of(c);
        }
    }
    int ok = stbi_write_png(path, W, H, 4, out.data(), W * 4);
    return ok != 0;
}

// ----------------- Drawing helpers -----------------
void draw_rect(SDL_Renderer *ren, int x, int y, int w, int h, SDL_Color col, bool fill = true)
{
    SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(ren, col.r, col.g, col.b, col.a);
    SDL_Rect r = {x, y, w, h};
    if (fill)
        SDL_RenderFillRect(ren, &r);
    else
        SDL_RenderDrawRect(ren, &r);
}

void draw_checkerboard(SDL_Renderer *ren, int x0, int y0, int wpx, int hpx, int px_size)
{
    // small 2x2 checker for transparent background
    for (int y = 0; y < hpx; ++y)
    {
        for (int x = 0; x < wpx; ++x)
        {
            bool alt = ((x / 2 + y / 2) & 1);
            uint8_t v = alt ? 240 : 220;
            SDL_SetRenderDrawColor(ren, v, v, v, 255);
            SDL_Rect r = {x0 + x * px_size, y0 + y * px_size, px_size, px_size};
            SDL_RenderFillRect(ren, &r);
        }
    }
}

void draw_pixel_grid(SDL_Renderer *ren, int x0, int y0, int wpx, int hpx, int px_size, SDL_Color grid_col)
{
    SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(ren, grid_col.r, grid_col.g, grid_col.b, grid_col.a);
    // vertical
    for (int i = 0; i <= wpx; i++)
    {
        int x = x0 + i * px_size;
        SDL_RenderDrawLine(ren, x, y0, x, y0 + hpx * px_size);
    }
    // horizontal
    for (int j = 0; j <= hpx; j++)
    {
        int y = y0 + j * px_size;
        SDL_RenderDrawLine(ren, x0, y, x0 + wpx * px_size, y);
    }
}

// draw small text using SDL_ttf
void draw_text(SDL_Renderer *ren, TTF_Font *font, const std::string &text, int x, int y, SDL_Color col)
{
    if (!font)
        return;
    SDL_Surface *surf = TTF_RenderUTF8_Blended(font, text.c_str(), col);
    if (!surf)
        return;
    SDL_Texture *tex = SDL_CreateTextureFromSurface(ren, surf);
    SDL_Rect dst = {x, y, surf->w, surf->h};
    SDL_RenderCopy(ren, tex, nullptr, &dst);
    SDL_DestroyTexture(tex);
    SDL_FreeSurface(surf);
}

// draw a simple icon for tool
void draw_tool_icon(SDL_Renderer *ren, SDL_Rect r, Tool t, SDL_Color foreground, SDL_Color bg)
{
    // background rounded-ish
    draw_rect(ren, r.x, r.y, r.w, r.h, bg, true);
    SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(ren, foreground.r, foreground.g, foreground.b, foreground.a);

    // draw simple shapes for each tool
    if (t == TOOL_PENCIL)
    {
        // diagonal pencil (triangle)
        int x1 = r.x + r.w / 4, y1 = r.y + r.h * 3 / 4;
        int x2 = r.x + r.w / 2, y2 = r.y + r.h / 4;
        int x3 = r.x + r.w * 3 / 4, y3 = r.y + r.h / 2;
        SDL_RenderDrawLine(ren, x1, y1, x2, y2);
        SDL_RenderDrawLine(ren, x2, y2, x3, y3);
        SDL_RenderDrawLine(ren, x1, y1, x3, y3);
    }
    else if (t == TOOL_ERASER)
    {
        SDL_Rect er = {r.x + r.w / 6, r.y + r.h / 4, r.w * 2 / 3, r.h / 2};
        draw_rect(ren, er.x, er.y, er.w, er.h, foreground, false);
    }
    else if (t == TOOL_EYEDROPPER)
    {
        // small circle + handle
        int cx = r.x + r.w / 3, cy = r.y + r.h / 3;
        for (int i = 0; i < 6; i++)
        {
            SDL_RenderDrawPoint(ren, cx + i, cy + i);
        }
        SDL_RenderDrawLine(ren, cx + 4, cy + 4, r.x + r.w - 6, r.y + r.h - 6);
    }
    else if (t == TOOL_FILL)
    {
        // bucket: rectangle with spout
        SDL_Rect inner = {r.x + r.w / 4, r.y + r.h / 4, r.w / 2, r.h / 2};
        SDL_RenderDrawRect(ren, &inner);
        SDL_RenderDrawLine(ren, r.x + r.w / 4, r.y + r.h / 4, r.x + r.w / 6, r.y + r.h / 6);
    }
}

// Draw icons for timeline buttons
void draw_timeline_icon(SDL_Renderer *ren, SDL_Rect r, int type, SDL_Color foreground, SDL_Color bg)
{
    // background
    draw_rect(ren, r.x, r.y, r.w, r.h, bg, true);
    SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(ren, foreground.r, foreground.g, foreground.b, foreground.a);

    // draw icon based on type
    if (type == 0) // Add frame (plus)
    {
        int cx = r.x + r.w / 2;
        int cy = r.y + r.h / 2;
        SDL_RenderDrawLine(ren, cx, r.y + 6, cx, r.y + r.h - 6);
        SDL_RenderDrawLine(ren, r.x + 6, cy, r.x + r.w - 6, cy);
    }
    else if (type == 1) // Duplicate frame (two squares)
    {
        SDL_Rect r1 = {r.x + 6, r.y + 6, r.w / 3, r.h - 12};
        SDL_Rect r2 = {r.x + r.w - r.w / 3 - 6, r.y + 6, r.w / 3, r.h - 12};
        SDL_RenderDrawRect(ren, &r1);
        SDL_RenderDrawRect(ren, &r2);
    }
    else if (type == 2) // Delete frame (trash can / X)
    {
        SDL_RenderDrawLine(ren, r.x + 6, r.y + 6, r.x + r.w - 6, r.y + r.h - 6);
        SDL_RenderDrawLine(ren, r.x + r.w - 6, r.y + 6, r.x + 6, r.y + r.h - 6);
    }
    else if (type == 3) // Play (triangle)
    {
        SDL_RenderDrawLine(ren, r.x + 6, r.y + 6, r.x + 6, r.y + r.h - 6);
        SDL_RenderDrawLine(ren, r.x + 6, r.y + 6, r.x + r.w - 6, r.y + r.h / 2);
        SDL_RenderDrawLine(ren, r.x + 6, r.y + r.h - 6, r.x + r.w - 6, r.y + r.h / 2);
    }
    else if (type == 4) // Pause (two vertical lines)
    {
        SDL_RenderDrawLine(ren, r.x + r.w / 3, r.y + 6, r.x + r.w / 3, r.y + r.h - 6);
        SDL_RenderDrawLine(ren, r.x + 2 * r.w / 3, r.y + 6, r.x + 2 * r.w / 3, r.y + r.h - 6);
    }
    else if (type == 5) // Export (arrow pointing out of box)
    {
        SDL_Rect box = {r.x + 6, r.y + 6, r.w - 12, r.h - 12};
        SDL_RenderDrawRect(ren, &box);
        SDL_RenderDrawLine(ren, r.x + r.w / 2, r.y + 6, r.x + r.w / 2, r.y);
        SDL_RenderDrawLine(ren, r.x + r.w / 2, r.y, r.x + r.w, r.y - r.h / 4);
    }
}

// ----------------- Main -----------------
int main(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0)
    {
        std::fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }
    if (TTF_Init() != 0)
    {
        std::fprintf(stderr, "TTF_Init failed: %s\n", TTF_GetError());
        return 1;
    }

    ensure_sprite_initialized();
    CANVAS_W = sprite.w;
    CANVAS_H = sprite.h;
    init_default_palette();

    SDL_Window *win = SDL_CreateWindow("Pixel Editor - Mini Aseprite Clone",
                                       SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, WINDOW_W, WINDOW_H, SDL_WINDOW_RESIZABLE);
    if (!win)
    {
        std::fprintf(stderr, "Window create failed: %s\n", SDL_GetError());
        return 1;
    }

    SDL_Renderer *ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!ren)
    {
        std::fprintf(stderr, "Renderer create failed: %s\n", SDL_GetError());
        return 1;
    }

    TTF_Font *font = TTF_OpenFont("/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf", 14);
    if (!font)
    {
        // fallback: try smaller or bundled
        font = nullptr;
        std::fprintf(stderr, "Warning: TTF font not found. Status bar text will be absent.\n");
    }

    // simple initial canvas origin
    int canvas_x = 24;
    int canvas_y = 40;

    bool quit = false;
    SDL_Event e;

    high_resolution_clock::time_point prev_time = high_resolution_clock::now();

    // For continuous drawing we will store last painted pixel to avoid rapid repeats
    int last_paint_x = -1, last_paint_y = -1;

    // some UI layout convenience
    auto toolbar_x = [&](int layer)
    { return 8; };
    auto toolbar_y = [&](int layer)
    { return 40 + layer * 52; };

    // Begin main loop
    while (!quit)
    {
        // frame timing
        auto now = high_resolution_clock::now();
        double dt = duration<double>(now - prev_time).count();
        prev_time = now;

        // playback
        if (playing && sprite.frames.size() > 0)
        {
            play_accum += dt;
            double frame_time = 1.0 / std::max(1.0, play_fps);
            while (play_accum >= frame_time)
            {
                play_accum -= frame_time;
                sprite.cur_frame = (sprite.cur_frame + 1) % (int)sprite.frames.size();
            }
        }

        while (SDL_PollEvent(&e))
        {
            if (e.type == SDL_QUIT)
            {
                quit = true;
                break;
            }
            if (e.type == SDL_WINDOWEVENT)
            {
                if (e.window.event == SDL_WINDOWEVENT_SIZE_CHANGED)
                {
                    // could adapt UI if needed
                }
            }

            // mouse down / up
            if (e.type == SDL_MOUSEBUTTONDOWN)
            {
                int mx = e.button.x, my = e.button.y;
                last_mouse_x = mx;
                last_mouse_y = my;

                // Right button -> eraser quickly while held
                if (e.button.button == SDL_BUTTON_RIGHT)
                {
                    mouse_right_down = true;
                }
                else if (e.button.button == SDL_BUTTON_MIDDLE)
                {
                    // start panning
                    panning = true;
                }
                else if (e.button.button == SDL_BUTTON_LEFT)
                {
                    mouse_left_down = true;
                }

                // detect toolbar clicks (left)
                if (e.button.button == SDL_BUTTON_LEFT)
                {
                    // compute toolbar rects
                    int tbx = toolbar_x(0);
                    int tby = toolbar_y(0);
                    SDL_Rect r_pencil = {tbx, tby, 44, 36};
                    SDL_Rect r_eraser = {tbx, tby + 44, 44, 36};
                    SDL_Rect r_eyed = {tbx, tby + 88, 44, 36};
                    SDL_Rect r_fill = {tbx, tby + 132, 44, 36};

                    if (mx >= r_pencil.x && mx <= r_pencil.x + r_pencil.w && my >= r_pencil.y && my <= r_pencil.y + r_pencil.h)
                    {
                        cur_tool = TOOL_PENCIL;
                    }
                    else if (mx >= r_eraser.x && mx <= r_eraser.x + r_eraser.w && my >= r_eraser.y && my <= r_eraser.y + r_eraser.h)
                    {
                        cur_tool = TOOL_ERASER;
                    }
                    else if (mx >= r_eyed.x && mx <= r_eyed.x + r_eyed.w && my >= r_eyed.y && my <= r_eyed.y + r_eyed.h)
                    {
                        cur_tool = TOOL_EYEDROPPER;
                    }
                    else if (mx >= r_fill.x && mx <= r_fill.x + r_fill.w && my >= r_fill.y && my <= r_fill.y + r_fill.h)
                    {
                        cur_tool = TOOL_FILL;
                    }

                    // palette click area
                    int pal_x = toolbar_x(0) + 64;
                    int pal_y = 40;
                    for (int i = 0; i < (int)palette.size(); ++i)
                    {
                        int sx = pal_x;
                        int sy = pal_y + i * (PALETTE_SWATCH + 8);
                        SDL_Rect sw = {sx, sy, PALETTE_SWATCH, PALETTE_SWATCH};
                        if (mx >= sw.x && mx <= sw.x + sw.w && my >= sw.y && my <= sw.y + sw.h)
                        {
                            cur_color = palette[i];
                        }
                    }

                    // timeline buttons
                    int tlx = canvas_x;
                    int tly = canvas_y + sprite.h * pixel_size + 8;

                    // New button arrangement with icons
                    SDL_Rect add_btn = {tlx + 8, tly + 4, 32, 28};
                    SDL_Rect dup_btn = {tlx + 48, tly + 4, 32, 28};
                    SDL_Rect del_btn = {tlx + 88, tly + 4, 32, 28};
                    SDL_Rect play_btn = {tlx + 128, tly + 4, 32, 28};
                    SDL_Rect export_btn = {tlx + 168, tly + 4, 32, 28};

                    if (mx >= add_btn.x && mx <= add_btn.x + add_btn.w && my >= add_btn.y && my <= add_btn.y + add_btn.h)
                    {
                        // add frame
                        sprite.frames.emplace_back(sprite.w, sprite.h, rgba_u(0, 0, 0, 0));
                        sprite.cur_frame = (int)sprite.frames.size() - 1;
                        undo_stack.clear();
                    }
                    else if (mx >= dup_btn.x && mx <= dup_btn.x + dup_btn.w && my >= dup_btn.y && my <= dup_btn.y + dup_btn.h)
                    {
                        // duplicate
                        sprite.frames.push_back(sprite.frames[sprite.cur_frame]);
                        sprite.cur_frame = (int)sprite.frames.size() - 1;
                        undo_stack.clear();
                    }
                    else if (mx >= del_btn.x && mx <= del_btn.x + del_btn.w && my >= del_btn.y && my <= del_btn.y + del_btn.h)
                    {
                        if (sprite.frames.size() > 1)
                        {
                            sprite.frames.erase(sprite.frames.begin() + sprite.cur_frame);
                            sprite.cur_frame = std::max(0, sprite.cur_frame - 1);
                            undo_stack.clear();
                        }
                        else
                        {
                            // clear single frame
                            sprite.frames[0] = Frame(sprite.w, sprite.h, rgba_u(0, 0, 0, 0));
                            undo_stack.clear();
                        }
                    }
                    else if (mx >= play_btn.x && mx <= play_btn.x + play_btn.w && my >= play_btn.y && my <= play_btn.y + play_btn.h)
                    {
                        // toggle playback
                        playing = !playing;
                    }
                    else if (mx >= export_btn.x && mx <= export_btn.x + export_btn.w && my >= export_btn.y && my <= export_btn.y + export_btn.h)
                    {
                        // export spritesheet
                        export_spritesheet(sprite, "spritesheet.png");
                        std::puts("Exported spritesheet.png");
                    }
                }

                // canvas clicks: painting
                int cx, cy;
                if (screen_to_canvas(mx, my, cx, cy, canvas_x, canvas_y))
                {
                    // save for undo
                    undo_stack.push(sprite.frames[sprite.cur_frame]);

                    Frame &fr = sprite.frames[sprite.cur_frame];
                    if (cur_tool == TOOL_PENCIL)
                    {
                        fr.at(cx, cy) = cur_color;
                        last_paint_x = cx;
                        last_paint_y = cy;
                    }
                    else if (cur_tool == TOOL_ERASER)
                    {
                        fr.at(cx, cy) = rgba_u(0, 0, 0, 0);
                        last_paint_x = cx;
                        last_paint_y = cy;
                    }
                    else if (cur_tool == TOOL_EYEDROPPER)
                    {
                        uint32_t c = fr.at(cx, cy);
                        cur_color = c;
                    }
                    else if (cur_tool == TOOL_FILL)
                    {
                        uint32_t target = fr.at(cx, cy);
                        flood_fill(fr, cx, cy, target, cur_color);
                    }
                }
            }

            if (e.type == SDL_MOUSEBUTTONUP)
            {
                if (e.button.button == SDL_BUTTON_LEFT)
                    mouse_left_down = false;
                if (e.button.button == SDL_BUTTON_RIGHT)
                    mouse_right_down = false;
                if (e.button.button == SDL_BUTTON_MIDDLE)
                    panning = false;
                last_paint_x = last_paint_y = -1;
            }

            if (e.type == SDL_MOUSEWHEEL)
            {
                // zoom with wheel (while ctrl pressed ?)
                if (e.wheel.y > 0)
                    pixel_size = std::min(MAX_PIXEL_SIZE, pixel_size + 2);
                else if (e.wheel.y < 0)
                    pixel_size = std::max(MIN_PIXEL_SIZE, pixel_size - 2);
            }

            if (e.type == SDL_MOUSEMOTION)
            {
                int mx = e.motion.x, my = e.motion.y;
                // panning
                if (panning || (e.key.keysym.sym == SDLK_SPACE))
                {
                    if (mouse_left_down || panning)
                    {
                        camera_x += (mx - last_mouse_x);
                        camera_y += (my - last_mouse_y);
                        canvas_x = int(camera_x);
                        canvas_y = int(camera_y);
                    }
                }

                // continuous painting while left down
                if (mouse_left_down)
                {
                    int cx, cy;
                    if (screen_to_canvas(mx, my, cx, cy, canvas_x, canvas_y))
                    {
                        Frame &fr = sprite.frames[sprite.cur_frame];
                        if (cur_tool == TOOL_PENCIL || (mouse_right_down && cur_tool != TOOL_EYEDROPPER))
                        {
                            // pencil or right-click quick eraser
                            if (last_paint_x != cx || last_paint_y != cy)
                            {
                                fr.at(cx, cy) = (mouse_right_down ? rgba_u(0, 0, 0, 0) : cur_color);
                                last_paint_x = cx;
                                last_paint_y = cy;
                            }
                        }
                        else if (cur_tool == TOOL_ERASER)
                        {
                            if (last_paint_x != cx || last_paint_y != cy)
                            {
                                fr.at(cx, cy) = rgba_u(0, 0, 0, 0);
                                last_paint_x = cx;
                                last_paint_y = cy;
                            }
                        }
                        else if (cur_tool == TOOL_EYEDROPPER)
                        {
                            cur_color = fr.at(cx, cy);
                        }
                    }
                }

                last_mouse_x = mx;
                last_mouse_y = my;
            }

            if (e.type == SDL_KEYDOWN)
            {
                SDL_Keycode k = e.key.keysym.sym;
                if (k == SDLK_p)
                    cur_tool = TOOL_PENCIL;
                else if (k == SDLK_e)
                    cur_tool = TOOL_ERASER;
                else if (k == SDLK_i)
                    cur_tool = TOOL_EYEDROPPER;
                else if (k == SDLK_f)
                    cur_tool = TOOL_FILL;
                else if (k == SDLK_z && SDL_GetModState() & KMOD_CTRL)
                {
                    // undo
                    if (undo_stack.can_undo())
                    {
                        Frame current = sprite.frames[sprite.cur_frame];
                        sprite.frames[sprite.cur_frame] = undo_stack.undo(current);
                    }
                }
                else if (k == SDLK_y && SDL_GetModState() & KMOD_CTRL)
                {
                    // redo
                    if (undo_stack.can_redo())
                    {
                        Frame current = sprite.frames[sprite.cur_frame];
                        sprite.frames[sprite.cur_frame] = undo_stack.redo(current);
                    }
                }
                else if (k == SDLK_s && SDL_GetModState() & KMOD_CTRL)
                {
                    // save project
                    save_project(sprite, "project.pxl");
                    std::puts("Saved project.pxl");
                }
                else if (k == SDLK_o && SDL_GetModState() & KMOD_CTRL)
                {
                    // load project
                    if (load_project(sprite, "project.pxl"))
                    {
                        std::puts("Loaded project.pxl");
                        undo_stack.clear();
                    }
                    else
                    {
                        std::puts("Failed to load project.pxl");
                    }
                }
                else if (k == SDLK_SPACE)
                {
                    // temporary pan tool while space is held
                    panning = true;
                }
                else if (k == SDLK_RIGHT)
                {
                    // next frame
                    sprite.cur_frame = (sprite.cur_frame + 1) % (int)sprite.frames.size();
                }
                else if (k == SDLK_LEFT)
                {
                    // previous frame
                    sprite.cur_frame = (sprite.cur_frame - 1 + (int)sprite.frames.size()) % (int)sprite.frames.size();
                }
                else if (k == SDLK_ESCAPE)
                {
                    quit = true;
                }
            }

            if (e.type == SDL_KEYUP)
            {
                if (e.key.keysym.sym == SDLK_SPACE)
                {
                    panning = false;
                }
            }
        }

        // Clear screen
        SDL_SetRenderDrawColor(ren, 60, 60, 70, 255);
        SDL_RenderClear(ren);

        // Draw UI background panels
        draw_rect(ren, 0, 0, WINDOW_W, STATUS_H, {40, 40, 45, 255});                                 // status bar
        draw_rect(ren, 0, STATUS_H, TOOLBAR_W, WINDOW_H - STATUS_H - TIMELINE_H, {50, 50, 55, 255}); // toolbar
        draw_rect(ren, 0, WINDOW_H - TIMELINE_H, WINDOW_W, TIMELINE_H, {45, 45, 50, 255});           // timeline

        // Draw canvas area background
        draw_rect(ren, canvas_x, canvas_y, sprite.w * pixel_size, sprite.h * pixel_size, {30, 30, 35, 255});

        // Draw checkerboard pattern for transparency
        draw_checkerboard(ren, canvas_x, canvas_y, sprite.w, sprite.h, pixel_size);

        // Draw onion skinning (previous/next frames)
        if (onion_skin && sprite.frames.size() > 1)
        {
            SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);

            // Previous frames
            for (int i = 1; i <= onion_prev; i++)
            {
                int frame_idx = (sprite.cur_frame - i + (int)sprite.frames.size()) % (int)sprite.frames.size();
                if (frame_idx != sprite.cur_frame)
                {
                    const Frame &fr = sprite.frames[frame_idx];
                    for (int y = 0; y < sprite.h; y++)
                    {
                        for (int x = 0; x < sprite.w; x++)
                        {
                            uint32_t c = fr.at(x, y);
                            if (a_of(c) > 0)
                            {
                                SDL_SetRenderDrawColor(ren, r_of(c), g_of(c), b_of(c), 80); // semi-transparent
                                SDL_Rect r = {canvas_x + x * pixel_size, canvas_y + y * pixel_size, pixel_size, pixel_size};
                                SDL_RenderFillRect(ren, &r);
                            }
                        }
                    }
                }
            }

            // Next frames
            for (int i = 1; i <= onion_next; i++)
            {
                int frame_idx = (sprite.cur_frame + i) % (int)sprite.frames.size();
                if (frame_idx != sprite.cur_frame)
                {
                    const Frame &fr = sprite.frames[frame_idx];
                    for (int y = 0; y < sprite.h; y++)
                    {
                        for (int x = 0; x < sprite.w; x++)
                        {
                            uint32_t c = fr.at(x, y);
                            if (a_of(c) > 0)
                            {
                                SDL_SetRenderDrawColor(ren, r_of(c), g_of(c), b_of(c), 60); // more transparent
                                SDL_Rect r = {canvas_x + x * pixel_size, canvas_y + y * pixel_size, pixel_size, pixel_size};
                                SDL_RenderFillRect(ren, &r);
                            }
                        }
                    }
                }
            }
        }

        // Draw current frame
        if (!sprite.frames.empty())
        {
            const Frame &fr = sprite.frames[sprite.cur_frame];
            for (int y = 0; y < sprite.h; y++)
            {
                for (int x = 0; x < sprite.w; x++)
                {
                    uint32_t c = fr.at(x, y);
                    if (a_of(c) > 0)
                    {
                        SDL_SetRenderDrawColor(ren, r_of(c), g_of(c), b_of(c), a_of(c));
                        SDL_Rect r = {canvas_x + x * pixel_size, canvas_y + y * pixel_size, pixel_size, pixel_size};
                        SDL_RenderFillRect(ren, &r);
                    }
                }
            }
        }

        // Draw pixel grid
        if (pixel_size >= 4)
        {
            draw_pixel_grid(ren, canvas_x, canvas_y, sprite.w, sprite.h, pixel_size, {80, 80, 90, 100});
        }

        // Draw toolbar with tools
        int tbx = toolbar_x(0);
        int tby = toolbar_y(0);

        // Tool buttons
        SDL_Color tool_bg = {70, 70, 75, 255};
        SDL_Color tool_selected = {90, 120, 200, 255};
        SDL_Color tool_fg = {220, 220, 220, 255};

        SDL_Rect r_pencil = {tbx, tby, 44, 36};
        draw_tool_icon(ren, r_pencil, TOOL_PENCIL, tool_fg, cur_tool == TOOL_PENCIL ? tool_selected : tool_bg);

        SDL_Rect r_eraser = {tbx, tby + 44, 44, 36};
        draw_tool_icon(ren, r_eraser, TOOL_ERASER, tool_fg, cur_tool == TOOL_ERASER ? tool_selected : tool_bg);

        SDL_Rect r_eyed = {tbx, tby + 88, 44, 36};
        draw_tool_icon(ren, r_eyed, TOOL_EYEDROPPER, tool_fg, cur_tool == TOOL_EYEDROPPER ? tool_selected : tool_bg);

        SDL_Rect r_fill = {tbx, tby + 132, 44, 36};
        draw_tool_icon(ren, r_fill, TOOL_FILL, tool_fg, cur_tool == TOOL_FILL ? tool_selected : tool_bg);

        // Draw palette
        int pal_x = toolbar_x(0) + 64;
        int pal_y = 40;
        for (int i = 0; i < (int)palette.size(); ++i)
        {
            uint32_t c = palette[i];
            SDL_SetRenderDrawColor(ren, r_of(c), g_of(c), b_of(c), a_of(c));
            SDL_Rect sw = {pal_x, pal_y + i * (PALETTE_SWATCH + 8), PALETTE_SWATCH, PALETTE_SWATCH};
            SDL_RenderFillRect(ren, &sw);

            // Border for current color
            if (c == cur_color)
            {
                SDL_SetRenderDrawColor(ren, 255, 255, 255, 255);
                SDL_RenderDrawRect(ren, &sw);
            }
        }

        // Draw current color preview
        SDL_Rect cur_col_rect = {pal_x, pal_y + (int)palette.size() * (PALETTE_SWATCH + 8) + 16, PALETTE_SWATCH * 2, PALETTE_SWATCH * 2};
        SDL_SetRenderDrawColor(ren, r_of(cur_color), g_of(cur_color), b_of(cur_color), a_of(cur_color));
        SDL_RenderFillRect(ren, &cur_col_rect);
        SDL_SetRenderDrawColor(ren, 255, 255, 255, 255);
        SDL_RenderDrawRect(ren, &cur_col_rect);

        // Draw timeline with frame thumbnails
        int tlx = canvas_x;
        int tly = canvas_y + sprite.h * pixel_size + 8;
        int frame_thumb_w = 48;
        int frame_thumb_h = 48;

        for (int i = 0; i < (int)sprite.frames.size(); i++)
        {
            int thumb_x = tlx + i * (frame_thumb_w + 4);
            SDL_Rect thumb_bg = {thumb_x, tly, frame_thumb_w, frame_thumb_h};

            // Background for thumbnail
            draw_rect(ren, thumb_x, tly, frame_thumb_w, frame_thumb_h, {40, 40, 45, 255}, true);

            // Highlight current frame
            if (i == sprite.cur_frame)
            {
                SDL_SetRenderDrawColor(ren, 90, 120, 200, 255);
                SDL_RenderDrawRect(ren, &thumb_bg);
            }

            // Draw frame thumbnail (scaled down)
            const Frame &fr = sprite.frames[i];
            int thumb_scale = std::max(1, std::min(frame_thumb_w / sprite.w, frame_thumb_h / sprite.h));
            int thumb_px = thumb_scale;
            int thumb_offset_x = thumb_x + (frame_thumb_w - sprite.w * thumb_px) / 2;
            int thumb_offset_y = tly + (frame_thumb_h - sprite.h * thumb_px) / 2;

            for (int y = 0; y < sprite.h; y++)
            {
                for (int x = 0; x < sprite.w; x++)
                {
                    uint32_t c = fr.at(x, y);
                    if (a_of(c) > 0)
                    {
                        SDL_SetRenderDrawColor(ren, r_of(c), g_of(c), b_of(c), a_of(c));
                        SDL_Rect r = {thumb_offset_x + x * thumb_px, thumb_offset_y + y * thumb_px, thumb_px, thumb_px};
                        SDL_RenderFillRect(ren, &r);
                    }
                }
            }

            // Frame number
            if (font)
            {
                draw_text(ren, font, std::to_string(i + 1), thumb_x + 2, tly + frame_thumb_h + 2, {200, 200, 200, 255});
            }
        }

        // Draw timeline buttons
        SDL_Color btn_bg = {70, 70, 75, 255};
        SDL_Color btn_fg = {220, 220, 220, 255};

        SDL_Rect add_btn = {tlx + 8, tly + 4, 32, 28};
        draw_timeline_icon(ren, add_btn, 0, btn_fg, btn_bg);

        SDL_Rect dup_btn = {tlx + 48, tly + 4, 32, 28};
        draw_timeline_icon(ren, dup_btn, 1, btn_fg, btn_bg);

        SDL_Rect del_btn = {tlx + 88, tly + 4, 32, 28};
        draw_timeline_icon(ren, del_btn, 2, btn_fg, btn_bg);

        SDL_Rect play_btn = {tlx + 128, tly + 4, 32, 28};
        draw_timeline_icon(ren, play_btn, playing ? 4 : 3, btn_fg, playing ? SDL_Color{90, 160, 90, 255} : btn_bg);

        SDL_Rect export_btn = {tlx + 168, tly + 4, 32, 28};
        draw_timeline_icon(ren, export_btn, 5, btn_fg, btn_bg);

        // Draw status bar info
        if (font)
        {
            std::string status = "Tool: ";
            switch (cur_tool)
            {
            case TOOL_PENCIL:
                status += "Pencil";
                break;
            case TOOL_ERASER:
                status += "Eraser";
                break;
            case TOOL_EYEDROPPER:
                status += "Eyedropper";
                break;
            case TOOL_FILL:
                status += "Fill";
                break;
            }
            status += " | Frame: " + std::to_string(sprite.cur_frame + 1) + "/" + std::to_string(sprite.frames.size());
            status += " | Size: " + std::to_string(sprite.w) + "x" + std::to_string(sprite.h);
            status += " | Zoom: " + std::to_string(pixel_size) + "x";
            status += " | Color: " + hex_of(cur_color);

            draw_text(ren, font, status, 8, 4, {220, 220, 220, 255});
        }

        // Draw cursor preview
        int cx, cy;
        if (screen_to_canvas(last_mouse_x, last_mouse_y, cx, cy, canvas_x, canvas_y))
        {
            SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);
            SDL_SetRenderDrawColor(ren, 255, 255, 255, 150);
            SDL_Rect cursor = {canvas_x + cx * pixel_size, canvas_y + cy * pixel_size, pixel_size, pixel_size};
            SDL_RenderDrawRect(ren, &cursor);
        }

        SDL_RenderPresent(ren);
    }

    // Cleanup
    if (font)
        TTF_CloseFont(font);
    SDL_DestroyRenderer(ren);
    SDL_DestroyWindow(win);
    TTF_Quit();
    SDL_Quit();

    return 0;
}