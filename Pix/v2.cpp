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
#include <functional>
#include <unordered_map>
#include <memory>
#include <sstream>
#include <iomanip>
#include "stb_image_write.h"

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>

using namespace std::chrono;

// ----------------- Config -----------------
// const int WINDOW_W = 1200;
// const int WINDOW_H = 800;

int CANVAS_W = 64;
int CANVAS_H = 64;

int DEFAULT_PIXEL_SIZE = 10;
int MIN_PIXEL_SIZE = 2;
int MAX_PIXEL_SIZE = 40;

const int TOOLBAR_W = 160;
const int TIMELINE_H = 120;
const int STATUS_H = 28;
const int PALETTE_SWATCH = 32;
const int MAX_UNDO_HISTORY = 50;

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
    for (int i = 0; i <= wpx; i++)
    {
        int x = x0 + i * px_size;
        SDL_RenderDrawLine(ren, x, y0, x, y0 + hpx * px_size);
    }
    for (int j = 0; j <= hpx; j++)
    {
        int y = y0 + j * px_size;
        SDL_RenderDrawLine(ren, x0, y, x0 + wpx * px_size, y);
    }
}

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

void draw_tool_icon(SDL_Renderer *ren, SDL_Rect r, Tool t, SDL_Color foreground, SDL_Color bg)
{
    draw_rect(ren, r.x, r.y, r.w, r.h, bg, true);
    SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(ren, foreground.r, foreground.g, foreground.b, foreground.a);

    switch (t)
    {
    case TOOL_PENCIL:
        SDL_RenderDrawLine(ren, r.x + 8, r.y + r.h - 8, r.x + r.w / 2, r.y + 8);
        SDL_RenderDrawLine(ren, r.x + r.w / 2, r.y + 8, r.x + r.w - 8, r.y + r.h - 8);
        break;
    case TOOL_ERASER:
        SDL_Rect er = {r.x + r.w / 4, r.y + r.h / 4, r.w / 2, r.h / 2};
        SDL_RenderDrawRect(ren, &er);
        break;
    case TOOL_EYEDROPPER:
        for (int i = 0; i < 4; i++)
        {
            SDL_RenderDrawLine(ren, r.x + 10 + i, r.y + 10 + i, r.x + 10 + i, r.y + 14 + i);
        }
        break;
    case TOOL_FILL:
        SDL_RenderDrawLine(ren, r.x + 8, r.y + 8, r.x + 12, r.y + 12);
        SDL_RenderDrawLine(ren, r.x + 12, r.y + 12, r.x + r.w - 8, r.y + 12);
        SDL_RenderDrawLine(ren, r.x + r.w - 8, r.y + 12, r.x + r.w - 12, r.y + 16);
        SDL_RenderDrawLine(ren, r.x + r.w - 12, r.y + 16, r.x + r.w - 12, r.y + r.h - 8);
        break;
    default:
        break;
    }
}

// Add export function
bool export_spritesheet(const Sprite &s, const char *path)
{
    if (s.frames.empty())
        return false;
    int W = s.w * s.frames.size();
    int H = s.h;
    std::vector<uint8_t> out(W * H * 4);

    for (int fi = 0; fi < s.frames.size(); ++fi)
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

    return stbi_write_png(path, W, H, 4, out.data(), W * 4) != 0;
}

// ----------------- UI System -----------------
struct UIPanel
{
    std::string name;
    SDL_Rect rect;
    bool visible;
    bool movable;
    bool collapsed;
    SDL_Color bg_color;

    UIPanel(const std::string &n, int x, int y, int w, int h, SDL_Color color = {50, 50, 55, 255})
        : name(n), rect{x, y, w, h}, visible(true), movable(true), collapsed(false), bg_color(color) {}

    virtual void render(SDL_Renderer *ren, TTF_Font *font) = 0;
    virtual void handle_event(SDL_Event *e) = 0;

    bool contains_point(int x, int y) const
    {
        return x >= rect.x && x <= rect.x + rect.w && y >= rect.y && y <= rect.y + rect.h;
    }

    virtual ~UIPanel() = default;
};

struct UIButton
{
    SDL_Rect rect;
    std::string text;
    std::function<void()> callback;
    bool hovered;
    bool pressed;

    UIButton(int x, int y, int w, int h, const std::string &t, std::function<void()> cb)
        : rect{x, y, w, h}, text(t), callback(cb), hovered(false), pressed(false) {}

    bool contains_point(int x, int y) const
    {
        return x >= rect.x && x <= rect.x + rect.w && y >= rect.y && y <= rect.y + rect.h;
    }
};

struct UIContext
{
    std::vector<std::unique_ptr<UIPanel>> panels;
    UIPanel *active_panel = nullptr;
    int drag_offset_x = 0;
    int drag_offset_y = 0;
    bool is_dragging = false;

    void add_panel(UIPanel *panel)
    {
        panels.emplace_back(panel);
    }

    void render(SDL_Renderer *ren, TTF_Font *font)
    {
        for (auto &panel : panels)
        {
            if (panel->visible && !panel->collapsed)
            {
                panel->render(ren, font);
            }
        }
    }

    void handle_event(SDL_Event *e)
    {

        if (e->type == SDL_MOUSEBUTTONDOWN)
        {
            if (e->button.button == SDL_BUTTON_LEFT)
            {
                int mx = e->button.x, my = e->button.y;

                for (auto &panel : panels)
                {
                    if (panel->visible && panel->contains_point(mx, my) && panel->movable)
                    {
                        active_panel = panel.get();
                        drag_offset_x = mx - panel->rect.x;
                        drag_offset_y = my - panel->rect.y;
                        is_dragging = true;
                        break;
                    }
                }
            }
        }
        else if (e->type == SDL_MOUSEBUTTONUP)
        {
            if (e->button.button == SDL_BUTTON_LEFT)
            {
                is_dragging = false;
                active_panel = nullptr;
            }
        }
        else if (e->type == SDL_MOUSEMOTION)
        {
            if (is_dragging && active_panel)
            {
                active_panel->rect.x = e->motion.x - drag_offset_x;
                active_panel->rect.y = e->motion.y - drag_offset_y;
            }
        }

        for (auto &panel : panels)
        {
            if (panel->visible)
            {
                panel->handle_event(e);
            }
        }
    }
};

// ----------------- Utility -----------------
static inline uint32_t rgba_u(uint8_t r, uint8_t g, uint8_t b, uint8_t a = 255)
{
    return (uint32_t)r | ((uint32_t)g << 8) | ((uint32_t)b << 16) | ((uint32_t)a << 24);
}

static inline uint8_t r_of(uint32_t c) { return c & 0xFF; }
static inline uint8_t g_of(uint32_t c) { return (c >> 8) & 0xFF; }
static inline uint8_t b_of(uint32_t c) { return (c >> 16) & 0xFF; }
static inline uint8_t a_of(uint32_t c) { return (c >> 24) & 0xFF; }

static inline uint32_t blend_colors(uint32_t bg, uint32_t fg)
{
    uint8_t alpha = a_of(fg);
    if (alpha == 0)
        return bg;
    if (alpha == 255)
        return fg;

    float a = alpha / 255.0f;
    float inv_a = 1.0f - a;

    return rgba_u(
        static_cast<uint8_t>(r_of(fg) * a + r_of(bg) * inv_a),
        static_cast<uint8_t>(g_of(fg) * a + g_of(bg) * inv_a),
        static_cast<uint8_t>(b_of(fg) * a + b_of(bg) * inv_a),
        static_cast<uint8_t>(alpha + a_of(bg) * inv_a));
}

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
    TOOL_RECT,
    TOOL_CIRCLE,
    TOOL_LINE,
    TOOL_COUNT
};

const char *tool_names[] = {
    "Pencil", "Eraser", "Eyedropper", "Fill", "Rectangle", "Circle", "Line"};

// ----------------- Frame & Sprite -----------------
struct Frame
{
    int w, h;
    std::vector<uint32_t> px; // RGBA

    Frame() : w(0), h(0) {}
    Frame(int W, int H, uint32_t fill = rgba_u(0, 0, 0, 0)) : w(W), h(H), px(W * H, fill) {}

    inline uint32_t &at(int x, int y) { return px[y * w + x]; }
    inline const uint32_t &at(int x, int y) const { return px[y * w + x]; }

    Frame clone() const
    {
        Frame new_frame(w, h);
        new_frame.px = px;
        return new_frame;
    }
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

    void add_frame(bool duplicate_current = false)
    {
        if (duplicate_current && !frames.empty())
        {
            frames.insert(frames.begin() + cur_frame + 1, frames[cur_frame].clone());
            cur_frame++;
        }
        else
        {
            frames.emplace_back(w, h, rgba_u(0, 0, 0, 0));
            cur_frame = static_cast<int>(frames.size()) - 1;
        }
    }

    void remove_current_frame()
    {
        if (frames.size() <= 1)
        {
            frames[0] = Frame(w, h, rgba_u(0, 0, 0, 0));
        }
        else
        {
            frames.erase(frames.begin() + cur_frame);
            if (cur_frame >= static_cast<int>(frames.size()))
            {
                cur_frame = static_cast<int>(frames.size()) - 1;
            }
        }
    }

    void move_frame(int from, int to)
    {
        if (from < 0 || from >= static_cast<int>(frames.size()) ||
            to < 0 || to >= static_cast<int>(frames.size()) || from == to)
        {
            return;
        }

        Frame temp = frames[from];
        if (from < to)
        {
            for (int i = from; i < to; i++)
            {
                frames[i] = frames[i + 1];
            }
        }
        else
        {
            for (int i = from; i > to; i--)
            {
                frames[i] = frames[i - 1];
            }
        }
        frames[to] = temp;
        cur_frame = to;
    }
};

// ----------------- Undo / Redo -----------------
struct UndoState
{
    Frame frame;
    int frame_index;
};

struct UndoStack
{
    std::deque<UndoState> stack;
    std::deque<UndoState> redo_stack;
    size_t max_size = MAX_UNDO_HISTORY;

    void push(const Frame &f, int frame_index)
    {
        stack.push_front({f, frame_index});
        if (stack.size() > max_size)
            stack.pop_back();
        redo_stack.clear();
    }

    bool can_undo() const { return !stack.empty(); }
    bool can_redo() const { return !redo_stack.empty(); }

    UndoState undo(const Frame &current, int current_frame_index)
    {
        if (!can_undo())
            return {current, current_frame_index};

        UndoState prev = stack.front();
        stack.pop_front();
        redo_stack.push_front({current, current_frame_index});
        return prev;
    }

    UndoState redo(const Frame &current, int current_frame_index)
    {
        if (!can_redo())
            return {current, current_frame_index};

        UndoState nxt = redo_stack.front();
        redo_stack.pop_front();
        stack.push_front({current, current_frame_index});
        return nxt;
    }

    void clear()
    {
        stack.clear();
        redo_stack.clear();
    }
};

// ----------------- UI State -----------------
struct UIState
{
    bool mouse_left_down = false;
    bool mouse_right_down = false;
    bool mouse_middle_down = false;
    int last_mouse_x = 0, last_mouse_y = 0;
    int drag_start_x = 0, drag_start_y = 0;
    bool is_dragging = false;
    int hover_frame = -1;
    int drag_frame = -1;
};

// ----------------- Global State -----------------
Sprite sprite;
int pixel_size = DEFAULT_PIXEL_SIZE;
double camera_x = 0, camera_y = 0;
bool panning = false;

Tool cur_tool = TOOL_PENCIL;
uint32_t cur_color = rgba_u(0, 0, 0, 255);
uint32_t secondary_color = rgba_u(255, 255, 255, 255);
bool use_secondary_color = false;

std::vector<uint32_t> palette;
std::vector<uint32_t> recent_colors;

bool playing = false;
double play_fps = 12.0;
double play_accum = 0.0;
int play_frame_index = 0;
bool onion_skin = true;
int onion_prev = 1;
int onion_next = 1;

UndoStack undo_stack;
UIState ui_state;
UIContext ui_context;

// Shortcut mappings
std::unordered_map<SDL_Keycode, std::function<void()>> shortcuts;

// ----------------- UI Panels -----------------
struct ToolbarPanel : public UIPanel
{
    std::vector<UIButton> tool_buttons;
    std::vector<UIButton> palette_buttons;
    SDL_Rect color_preview_rect;

    ToolbarPanel(int x, int y) : UIPanel("Toolbar", x, y, TOOLBAR_W, 400)
    {
        // Tool buttons
        for (int i = 0; i < TOOL_COUNT; i++)
        {
            tool_buttons.emplace_back(
                rect.x + 10,
                rect.y + 40 + i * 40,
                36, 36,
                "",
                [i]()
                { cur_tool = static_cast<Tool>(i); });
        }

        // Palette buttons
        for (int i = 0; i < 16; i++)
        {
            palette_buttons.emplace_back(
                rect.x + 60 + (i % 2) * 40,
                rect.y + 40 + (i / 2) * 40,
                36, 36,
                "",
                [i]()
                { cur_color = palette[i]; });
        }

        color_preview_rect = {rect.x + 60, rect.y + 320, 76, 36};
    }

    void render(SDL_Renderer *ren, TTF_Font *font) override
    {
        // Draw panel background
        draw_rect(ren, rect.x, rect.y, rect.w, rect.h, bg_color, true);

        // Draw title
        if (font)
        {
            draw_text(ren, font, name, rect.x + 10, rect.y + 10, {220, 220, 220, 255});
        }

        // Draw tool buttons
        for (size_t i = 0; i < tool_buttons.size(); i++)
        {
            SDL_Color btn_bg = (cur_tool == i) ? SDL_Color{90, 120, 200, 255} : SDL_Color{70, 70, 75, 255};
            draw_rect(ren, tool_buttons[i].rect.x, tool_buttons[i].rect.y,
                      tool_buttons[i].rect.w, tool_buttons[i].rect.h, btn_bg, true);

            // Draw tool icon
            draw_tool_icon(ren, tool_buttons[i].rect, static_cast<Tool>(i),
                           {220, 220, 220, 255}, btn_bg);
        }

        // Draw palette buttons
        for (size_t i = 0; i < palette_buttons.size() && i < palette.size(); i++)
        {
            uint32_t col = palette[i];
            SDL_Color sdl_col = {r_of(col), g_of(col), b_of(col), 255};
            draw_rect(ren, palette_buttons[i].rect.x, palette_buttons[i].rect.y,
                      palette_buttons[i].rect.w, palette_buttons[i].rect.h, sdl_col, true);

            // Border for selected color
            if (col == cur_color)
            {
                draw_rect(ren, palette_buttons[i].rect.x, palette_buttons[i].rect.y,
                          palette_buttons[i].rect.w, palette_buttons[i].rect.h, {255, 255, 255, 255}, false);
            }
        }

        // Draw current color preview
        SDL_Color cur_col = {r_of(cur_color), g_of(cur_color), b_of(cur_color), 255};
        SDL_Color sec_col = {r_of(secondary_color), g_of(secondary_color), b_of(secondary_color), 255};

        draw_rect(ren, color_preview_rect.x, color_preview_rect.y,
                  color_preview_rect.w / 2, color_preview_rect.h, cur_col, true);
        draw_rect(ren, color_preview_rect.x + color_preview_rect.w / 2, color_preview_rect.y,
                  color_preview_rect.w / 2, color_preview_rect.h, sec_col, true);
        draw_rect(ren, color_preview_rect.x, color_preview_rect.y,
                  color_preview_rect.w, color_preview_rect.h, {255, 255, 255, 255}, false);
    }

    void handle_event(SDL_Event *e) override
    {
        if (e->type == SDL_MOUSEBUTTONDOWN && e->button.button == SDL_BUTTON_LEFT)
        {
            int mx = e->button.x, my = e->button.y;

            // Check tool buttons
            for (size_t i = 0; i < tool_buttons.size(); i++)
            {
                if (tool_buttons[i].contains_point(mx, my))
                {
                    tool_buttons[i].callback();
                    break;
                }
            }

            // Check palette buttons
            for (size_t i = 0; i < palette_buttons.size() && i < palette.size(); i++)
            {
                if (palette_buttons[i].contains_point(mx, my))
                {
                    palette_buttons[i].callback();
                    break;
                }
            }
        }
    }
};

struct TimelinePanel : public UIPanel
{
    std::vector<UIButton> control_buttons;
    int frame_thumb_size = 48;
    int frame_spacing = 4;

    TimelinePanel(int x, int y) : UIPanel("Timeline", x, y, WINDOW_W - 20, TIMELINE_H)
    {
        // Control buttons
        control_buttons.emplace_back(rect.x + 10, rect.y + 10, 32, 28, "+", []()
                                     { sprite.add_frame(false); });
        control_buttons.emplace_back(rect.x + 50, rect.y + 10, 32, 28, "D", []()
                                     { sprite.add_frame(true); });
        control_buttons.emplace_back(rect.x + 90, rect.y + 10, 32, 28, "-", []()
                                     { sprite.remove_current_frame(); });
        control_buttons.emplace_back(rect.x + 130, rect.y + 10, 32, 28, "◀", []()
                                     { sprite.cur_frame = (sprite.cur_frame - 1 + sprite.frames.size()) % sprite.frames.size(); });
        control_buttons.emplace_back(rect.x + 170, rect.y + 10, 32, 28, "▶", []()
                                     { sprite.cur_frame = (sprite.cur_frame + 1) % sprite.frames.size(); });
        control_buttons.emplace_back(rect.x + 210, rect.y + 10, 32, 28, "⏯", []()
                                     { playing = !playing; });
        control_buttons.emplace_back(rect.x + 250, rect.y + 10, 32, 28, "E", []()
                                     { export_spritesheet(sprite, "spritesheet.png"); });
        control_buttons.emplace_back(rect.x + 290, rect.y + 10, 32, 28, "O", []()
                                     { onion_skin = !onion_skin; });
    }

    void render(SDL_Renderer *ren, TTF_Font *font) override
    {
        // Draw panel background
        draw_rect(ren, rect.x, rect.y, rect.w, rect.h, bg_color, true);

        // Draw title
        if (font)
        {
            draw_text(ren, font, name, rect.x + 10, rect.y + 10, {220, 220, 220, 255});
        }

        // Draw control buttons
        for (auto &btn : control_buttons)
        {
            SDL_Color btn_bg = btn.hovered ? SDL_Color{90, 90, 100, 255} : SDL_Color{70, 70, 75, 255};
            if (btn.text == "⏯" && playing)
                btn_bg = {90, 160, 90, 255};
            if (btn.text == "O" && onion_skin)
                btn_bg = {90, 160, 90, 255};

            draw_rect(ren, btn.rect.x, btn.rect.y, btn.rect.w, btn.rect.h, btn_bg, true);
            if (font)
            {
                draw_text(ren, font, btn.text, btn.rect.x + btn.rect.w / 2 - 5, btn.rect.y + btn.rect.h / 2 - 8, {220, 220, 220, 255});
            }
        }

        // Draw frame thumbnails
        int thumb_x = rect.x + 330;
        for (int i = 0; i < static_cast<int>(sprite.frames.size()); i++)
        {
            int thumb_y = rect.y + 10;
            SDL_Rect thumb_rect = {thumb_x, thumb_y, frame_thumb_size, frame_thumb_size};

            // Draw thumbnail background
            draw_rect(ren, thumb_x, thumb_y, frame_thumb_size, frame_thumb_size, {40, 40, 45, 255}, true);

            // Highlight current frame
            if (i == sprite.cur_frame)
            {
                draw_rect(ren, thumb_x, thumb_y, frame_thumb_size, frame_thumb_size, {90, 120, 200, 255}, false);
            }

            // Draw frame content
            const Frame &frame = sprite.frames[i];
            int scale = std::max(1, std::min(frame_thumb_size / sprite.w, frame_thumb_size / sprite.h));
            int px_size = scale;
            int offset_x = thumb_x + (frame_thumb_size - sprite.w * px_size) / 2;
            int offset_y = thumb_y + (frame_thumb_size - sprite.h * px_size) / 2;

            for (int y = 0; y < sprite.h; y++)
            {
                for (int x = 0; x < sprite.w; x++)
                {
                    uint32_t col = frame.at(x, y);
                    if (a_of(col) > 0)
                    {
                        SDL_SetRenderDrawColor(ren, r_of(col), g_of(col), b_of(col), a_of(col));
                        SDL_Rect r = {offset_x + x * px_size, offset_y + y * px_size, px_size, px_size};
                        SDL_RenderFillRect(ren, &r);
                    }
                }
            }

            // Draw frame number
            if (font)
            {
                draw_text(ren, font, std::to_string(i + 1), thumb_x + 2, thumb_y + frame_thumb_size + 2, {200, 200, 200, 255});
            }

            thumb_x += frame_thumb_size + frame_spacing;
        }
    }

    void handle_event(SDL_Event *e) override
    {
        if (e->type == SDL_MOUSEBUTTONDOWN && e->button.button == SDL_BUTTON_LEFT)
        {
            int mx = e->button.x, my = e->button.y;

            // Check control buttons
            for (auto &btn : control_buttons)
            {
                if (btn.contains_point(mx, my))
                {
                    btn.callback();
                    return;
                }
            }

            // Check frame thumbnails
            int thumb_x = rect.x + 330;
            for (int i = 0; i < static_cast<int>(sprite.frames.size()); i++)
            {
                SDL_Rect thumb_rect = {thumb_x, rect.y + 10, frame_thumb_size, frame_thumb_size};
                if (mx >= thumb_rect.x && mx <= thumb_rect.x + thumb_rect.w &&
                    my >= thumb_rect.y && my <= thumb_rect.y + thumb_rect.h)
                {
                    sprite.cur_frame = i;
                    return;
                }
                thumb_x += frame_thumb_size + frame_spacing;
            }
        }

        // Update button hover states
        if (e->type == SDL_MOUSEMOTION)
        {
            int mx = e->motion.x, my = e->motion.y;
            for (auto &btn : control_buttons)
            {
                btn.hovered = btn.contains_point(mx, my);
            }
        }
    }
};

struct StatusPanel : public UIPanel
{
    StatusPanel(int x, int y) : UIPanel("Status", x, y, WINDOW_W, STATUS_H) {}

    void render(SDL_Renderer *ren, TTF_Font *font) override
    {
        // Draw panel background
        draw_rect(ren, rect.x, rect.y, rect.w, rect.h, {40, 40, 45, 255}, true);

        if (font)
        {
            std::string status = "Tool: " + std::string(tool_names[cur_tool]) +
                                 " | Frame: " + std::to_string(sprite.cur_frame + 1) + "/" + std::to_string(sprite.frames.size()) +
                                 " | Size: " + std::to_string(sprite.w) + "x" + std::to_string(sprite.h) +
                                 " | Zoom: " + std::to_string(pixel_size) + "x" +
                                 " | Color: " + hex_of(cur_color);

            draw_text(ren, font, status, rect.x + 10, rect.y + 6, {220, 220, 220, 255});
        }
    }

    void handle_event(SDL_Event *e) override
    {
        // Status bar doesn't handle events
    }
};

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
    palette.push_back(rgba_u(200, 50, 200, 255));
    palette.push_back(rgba_u(50, 200, 50, 255));
    palette.push_back(rgba_u(200, 200, 50, 255));
    palette.push_back(rgba_u(50, 50, 50, 255));

    recent_colors = {cur_color, secondary_color};
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
    float localx = (sx - canvas_origin_x) / static_cast<float>(pixel_size);
    float localy = (sy - canvas_origin_y) / static_cast<float>(pixel_size);
    cx = static_cast<int>(std::floor(localx));
    cy = static_cast<int>(std::floor(localy));
    return (cx >= 0 && cy >= 0 && cx < sprite.w && cy < sprite.h);
}

void flood_fill(Frame &f, int sx, int sy, uint32_t target, uint32_t repl)
{
    if (target == repl || sx < 0 || sy < 0 || sx >= f.w || sy >= f.h)
        return;

    if (f.at(sx, sy) != target)
        return;

    std::vector<std::pair<int, int>> stack;
    stack.emplace_back(sx, sy);

    while (!stack.empty())
    {
        auto [x, y] = stack.back();
        stack.pop_back();

        if (x < 0 || y < 0 || x >= f.w || y >= f.h || f.at(x, y) != target)
            continue;

        f.at(x, y) = repl;
        stack.emplace_back(x + 1, y);
        stack.emplace_back(x - 1, y);
        stack.emplace_back(x, y + 1);
        stack.emplace_back(x, y - 1);
    }
}

void draw_line(Frame &f, int x0, int y0, int x1, int y1, uint32_t color)
{
    int dx = abs(x1 - x0);
    int dy = abs(y1 - y0);
    int sx = (x0 < x1) ? 1 : -1;
    int sy = (y0 < y1) ? 1 : -1;
    int err = dx - dy;

    while (true)
    {
        if (x0 >= 0 && x0 < f.w && y0 >= 0 && y0 < f.h)
            f.at(x0, y0) = color;

        if (x0 == x1 && y0 == y1)
            break;

        int e2 = 2 * err;
        if (e2 > -dy)
        {
            err -= dy;
            x0 += sx;
        }
        if (e2 < dx)
        {
            err += dx;
            y0 += sy;
        }
    }
}

void draw_rect(Frame &f, int x0, int y0, int x1, int y1, uint32_t color, bool filled)
{
    int min_x = std::max(0, std::min(x0, x1));
    int max_x = std::min(f.w - 1, std::max(x0, x1));
    int min_y = std::max(0, std::min(y0, y1));
    int max_y = std::min(f.h - 1, std::max(y0, y1));

    if (filled)
    {
        for (int y = min_y; y <= max_y; y++)
            for (int x = min_x; x <= max_x; x++)
                f.at(x, y) = color;
    }
    else
    {
        for (int x = min_x; x <= max_x; x++)
        {
            f.at(x, min_y) = color;
            f.at(x, max_y) = color;
        }
        for (int y = min_y; y <= max_y; y++)
        {
            f.at(min_x, y) = color;
            f.at(max_x, y) = color;
        }
    }
}

void draw_circle(Frame &f, int cx, int cy, int radius, uint32_t color, bool filled)
{
    if (radius <= 0)
        return;

    int x = radius;
    int y = 0;
    int err = 0;

    while (x >= y)
    {
        if (filled)
        {
            for (int i = cx - x; i <= cx + x; i++)
            {
                if (i >= 0 && i < f.w)
                {
                    if (cy + y >= 0 && cy + y < f.h)
                        f.at(i, cy + y) = color;
                    if (cy - y >= 0 && cy - y < f.h)
                        f.at(i, cy - y) = color;
                }
            }
            for (int i = cx - y; i <= cx + y; i++)
            {
                if (i >= 0 && i < f.w)
                {
                    if (cy + x >= 0 && cy + x < f.h)
                        f.at(i, cy + x) = color;
                    if (cy - x >= 0 && cy - x < f.h)
                        f.at(i, cy - x) = color;
                }
            }
        }
        else
        {
            if (cx + x < f.w && cy + y < f.h)
                f.at(cx + x, cy + y) = color;
            if (cx + y < f.w && cy + x < f.h)
                f.at(cx + y, cy + x) = color;
            if (cx - y >= 0 && cy + x < f.h)
                f.at(cx - y, cy + x) = color;
            if (cx - x >= 0 && cy + y < f.h)
                f.at(cx - x, cy + y) = color;
            if (cx - x >= 0 && cy - y >= 0)
                f.at(cx - x, cy - y) = color;
            if (cx - y >= 0 && cy - x >= 0)
                f.at(cx - y, cy - x) = color;
            if (cx + y < f.w && cy - x >= 0)
                f.at(cx + y, cy - x) = color;
            if (cx + x < f.w && cy - y >= 0)
                f.at(cx + x, cy - y) = color;
        }

        if (err <= 0)
        {
            y += 1;
            err += 2 * y + 1;
        }
        if (err > 0)
        {
            x -= 1;
            err -= 2 * x + 1;
        }
    }
}

// export sprite frames horizontally into PNG using stb_image_write
bool export_spritesheet(const Sprite &s, const char *path)
{
    if (s.frames.empty())
        return false;

    int W = s.w * static_cast<int>(s.frames.size());
    int H = s.h;
    std::vector<uint8_t> out(W * H * 4);

    for (int fi = 0; fi < static_cast<int>(s.frames.size()); ++fi)
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

    ofs.write("PXL2", 4);
    int32_t w = s.w, h = s.h;
    int32_t fc = static_cast<int32_t>(s.frames.size());
    ofs.write(reinterpret_cast<const char *>(&w), sizeof(w));
    ofs.write(reinterpret_cast<const char *>(&h), sizeof(h));
    ofs.write(reinterpret_cast<const char *>(&fc), sizeof(fc));

    for (auto &fr : s.frames)
    {
        ofs.write(reinterpret_cast<const char *>(fr.px.data()), sizeof(uint32_t) * fr.w * fr.h);
    }

    // Save palette
    int32_t palette_size = static_cast<int32_t>(palette.size());
    ofs.write(reinterpret_cast<const char *>(&palette_size), sizeof(palette_size));
    ofs.write(reinterpret_cast<const char *>(palette.data()), sizeof(uint32_t) * palette.size());

    return true;
}

bool load_project(Sprite &s, const char *path)
{
    std::ifstream ifs(path, std::ios::binary);
    if (!ifs)
        return false;

    char hdr[4];
    ifs.read(hdr, 4);
    if (std::strncmp(hdr, "PXL2", 4) != 0)
        return false;

    int32_t w, h, fc;
    ifs.read(reinterpret_cast<char *>(&w), sizeof(w));
    ifs.read(reinterpret_cast<char *>(&h), sizeof(h));
    ifs.read(reinterpret_cast<char *>(&fc), sizeof(fc));

    if (w <= 0 || h <= 0 || fc <= 0)
        return false;

    s.w = w;
    s.h = h;
    s.frames.clear();

    for (int i = 0; i < fc; i++)
    {
        Frame fr(w, h);
        ifs.read(reinterpret_cast<char *>(fr.px.data()), sizeof(uint32_t) * w * h);
        s.frames.push_back(std::move(fr));
    }

    s.cur_frame = 0;

    // Load palette if available
    try
    {
        int32_t palette_size;
        ifs.read(reinterpret_cast<char *>(&palette_size), sizeof(palette_size));
        if (palette_size > 0 && palette_size < 256)
        {
            palette.resize(palette_size);
            ifs.read(reinterpret_cast<char *>(palette.data()), sizeof(uint32_t) * palette_size);
        }
    }
    catch (...)
    {
        // If palette loading fails, keep the current palette
    }

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
    switch (t)
    {
    case TOOL_PENCIL:
    {
        // pencil icon
        SDL_RenderDrawLine(ren, r.x + 8, r.y + r.h - 8, r.x + r.w / 2, r.y + 8);
        SDL_RenderDrawLine(ren, r.x + r.w / 2, r.y + 8, r.x + r.w - 8, r.y + r.h - 8);
        SDL_RenderDrawLine(ren, r.x + 8, r.y + r.h - 8, r.x + r.w - 8, r.y + r.h - 8);
        break;
    }
    case TOOL_ERASER:
    {
        SDL_Rect er = {r.x + r.w / 6, r.y + r.h / 4, r.w * 2 / 3, r.h / 2};
        draw_rect(ren, er.x, er.y, er.w, er.h, foreground, false);
        break;
    }
    case TOOL_EYEDROPPER:
    {
        // eyedropper icon
        int cx = r.x + r.w / 3, cy = r.y + r.h / 3;
        for (int i = 0; i < 4; i++)
        {
            SDL_RenderDrawLine(ren, cx + i, cy + i, cx + i, cy + i + 4);
        }
        SDL_RenderDrawLine(ren, cx + 4, cy + 4, r.x + r.w - 6, r.y + r.h - 6);
        break;
    }
    case TOOL_FILL:
    {
        // fill bucket icon
        SDL_RenderDrawLine(ren, r.x + 8, r.y + 8, r.x + 12, r.y + 12);
        SDL_RenderDrawLine(ren, r.x + 12, r.y + 12, r.x + r.w - 8, r.y + 12);
        SDL_RenderDrawLine(ren, r.x + r.w - 8, r.y + 12, r.x + r.w - 12, r.y + 16);
        SDL_RenderDrawLine(ren, r.x + r.w - 12, r.y + 16, r.x + r.w - 12, r.y + r.h - 8);
        SDL_RenderDrawLine(ren, r.x + r.w - 12, r.y + r.h - 8, r.x + 8, r.y + r.h - 8);
        SDL_RenderDrawLine(ren, r.x + 8, r.y + r.h - 8, r.x + 8, r.y + 8);
        break;
    }
    case TOOL_RECT:
    {
        SDL_Rect rect = {r.x + 8, r.y + 8, r.w - 16, r.h - 16};
        SDL_RenderDrawRect(ren, &rect);
        break;
    }
    case TOOL_CIRCLE:
    {
        int cx = r.x + r.w / 2;
        int cy = r.y + r.h / 2;
        int radius = std::min(r.w, r.h) / 2 - 8;

        for (int i = 0; i < 16; i++)
        {
            float angle = i * M_PI / 8;
            float next_angle = (i + 1) * M_PI / 8;
            SDL_RenderDrawLine(ren,
                               cx + radius * cos(angle),
                               cy + radius * sin(angle),
                               cx + radius * cos(next_angle),
                               cy + radius * sin(next_angle));
        }
        break;
    }
    case TOOL_LINE:
    {
        SDL_RenderDrawLine(ren, r.x + 8, r.y + r.h - 8, r.x + r.w - 8, r.y + 8);
        break;
    }
    default:
        break;
    }
}

// Initialize keyboard shortcuts
void init_shortcuts()
{
    shortcuts[SDLK_p] = []()
    { cur_tool = TOOL_PENCIL; };
    shortcuts[SDLK_e] = []()
    { cur_tool = TOOL_ERASER; };
    shortcuts[SDLK_i] = []()
    { cur_tool = TOOL_EYEDROPPER; };
    shortcuts[SDLK_f] = []()
    { cur_tool = TOOL_FILL; };
    shortcuts[SDLK_r] = []()
    { cur_tool = TOOL_RECT; };
    shortcuts[SDLK_c] = []()
    { cur_tool = TOOL_CIRCLE; };
    shortcuts[SDLK_l] = []()
    { cur_tool = TOOL_LINE; };
    shortcuts[SDLK_x] = []()
    { std::swap(cur_color, secondary_color); use_secondary_color = !use_secondary_color; };
    shortcuts[SDLK_SPACE] = []()
    { panning = true; };

    shortcuts[SDLK_LEFTBRACKET] = [&]()
    {
        pixel_size = std::max(MIN_PIXEL_SIZE, pixel_size - 1);
    };

    shortcuts[SDLK_RIGHTBRACKET] = [&]()
    {
        pixel_size = std::min(MAX_PIXEL_SIZE, pixel_size + 1);
    };

    shortcuts[SDLK_0] = [&]()
    {
        pixel_size = DEFAULT_PIXEL_SIZE;
    };

    shortcuts[SDLK_LEFT] = [&]()
    {
        sprite.cur_frame = (sprite.cur_frame - 1 + sprite.frames.size()) % sprite.frames.size();
    };

    shortcuts[SDLK_RIGHT] = [&]()
    {
        sprite.cur_frame = (sprite.cur_frame + 1) % sprite.frames.size();
    };

    shortcuts[SDLK_z] = [&]()
    {
        if (SDL_GetModState() & KMOD_CTRL)
        {
            if (SDL_GetModState() & KMOD_SHIFT)
            {
                // Redo
                if (undo_stack.can_redo())
                {
                    UndoState state = undo_stack.redo(sprite.frames[sprite.cur_frame], sprite.cur_frame);
                    sprite.frames[state.frame_index] = state.frame;
                    if (state.frame_index != sprite.cur_frame)
                    {
                        sprite.cur_frame = state.frame_index;
                    }
                }
            }
            else
            {
                // Undo
                if (undo_stack.can_undo())
                {
                    UndoState state = undo_stack.undo(sprite.frames[sprite.cur_frame], sprite.cur_frame);
                    sprite.frames[state.frame_index] = state.frame;
                    if (state.frame_index != sprite.cur_frame)
                    {
                        sprite.cur_frame = state.frame_index;
                    }
                }
            }
        }
    };

    shortcuts[SDLK_s] = [&]()
    {
        if (SDL_GetModState() & KMOD_CTRL)
        {
            save_project(sprite, "project.pxl");
            std::puts("Saved project.pxl");
        }
    };

    shortcuts[SDLK_o] = [&]()
    {
        if (SDL_GetModState() & KMOD_CTRL)
        {
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
    };

    shortcuts[SDLK_n] = [&]()
    {
        if (SDL_GetModState() & KMOD_CTRL)
        {
            sprite = Sprite(CANVAS_W, CANVAS_H);
            undo_stack.clear();
        }
    };
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
    init_shortcuts();

    SDL_Window *win = SDL_CreateWindow("Pixel Editor - Mini Aseprite Clone",
                                       SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, WINDOW_W, WINDOW_H,
                                       SDL_WINDOW_RESIZABLE);
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
        // Try other common font paths
        font = TTF_OpenFont("/usr/share/fonts/TTF/DejaVuSans.ttf", 14);
        if (!font)
        {
            font = TTF_OpenFont("arial.ttf", 14);
            if (!font)
            {
                std::fprintf(stderr, "Warning: TTF font not found. Status bar text will be absent.\n");
            }
        }
    }

    // Initialize UI panels
    ui_context.add_panel(new ToolbarPanel(10, 40));
    ui_context.add_panel(new TimelinePanel(10, WINDOW_H - TIMELINE_H - 10));
    ui_context.add_panel(new StatusPanel(0, WINDOW_H - STATUS_H));

    // simple initial canvas origin
    int canvas_x = 180;
    int canvas_y = 40;

    bool quit = false;
    SDL_Event e;

    high_resolution_clock::time_point prev_time = high_resolution_clock::now();

    // For continuous drawing we will store last painted pixel to avoid rapid repeats
    int last_paint_x = -1, last_paint_y = -1;
    int drag_start_canvas_x = -1, drag_start_canvas_y = -1;

    // Begin main loop
    while (!quit)
    {
        // frame timing
        auto now = high_resolution_clock::now();
        double dt = duration<double>(now - prev_time).count();
        prev_time = now;

        // playback
        if (playing && !sprite.frames.empty())
        {
            play_accum += dt;
            double frame_time = 1.0 / std::max(1.0, play_fps);
            while (play_accum >= frame_time)
            {
                play_accum -= frame_time;
                sprite.cur_frame = (sprite.cur_frame + 1) % static_cast<int>(sprite.frames.size());
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
                    // Update window dimensions

                    int temp_w, temp_h;
                    SDL_GetWindowSize(win, &temp_w, &temp_h);
                    WINDOW_W = temp_w;
                    WINDOW_H = temp_h;

                           // Update status panel position
                    for (auto &panel : ui_context.panels)
                    {
                        if (panel->name == "Status")
                        {
                            panel->rect.y = WINDOW_H - STATUS_H;
                            panel->rect.w = WINDOW_W;
                        }
                        else if (panel->name == "Timeline")
                        {
                            panel->rect.w = WINDOW_W - 20;
                        }
                    }
                }
            }

            // Handle UI events first
            ui_context.handle_event(&e);

            // Handle other events
            if (e.type == SDL_MOUSEBUTTONDOWN)
            {
                int mx = e.button.x, my = e.button.y;
                ui_state.last_mouse_x = mx;
                ui_state.last_mouse_y = my;

                // Check if we're clicking on a UI panel
                bool clicked_on_ui = false;
                for (auto &panel : ui_context.panels)
                {
                    if (panel->visible && panel->contains_point(mx, my))
                    {
                        clicked_on_ui = true;
                        break;
                    }
                }

                if (!clicked_on_ui)
                {
                    // Right button -> eraser quickly while held
                    if (e.button.button == SDL_BUTTON_RIGHT)
                    {
                        ui_state.mouse_right_down = true;
                    }
                    else if (e.button.button == SDL_BUTTON_MIDDLE)
                    {
                        // start panning
                        panning = true;
                    }
                    else if (e.button.button == SDL_BUTTON_LEFT)
                    {
                        ui_state.mouse_left_down = true;

                        // Check if we're starting a drawing operation on the canvas
                        int cx, cy;
                        if (screen_to_canvas(mx, my, cx, cy, canvas_x, canvas_y))
                        {
                            drag_start_canvas_x = cx;
                            drag_start_canvas_y = cy;

                            // save for undo
                            undo_stack.push(sprite.frames[sprite.cur_frame], sprite.cur_frame);

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
                }
            }

            if (e.type == SDL_MOUSEBUTTONUP)
            {
                if (e.button.button == SDL_BUTTON_LEFT)
                {
                    ui_state.mouse_left_down = false;

                    // Handle shape tools
                    if (drag_start_canvas_x != -1 && drag_start_canvas_y != -1)
                    {
                        int mx = e.button.x, my = e.button.y;
                        int end_x, end_y;
                        if (screen_to_canvas(mx, my, end_x, end_y, canvas_x, canvas_y))
                        {
                            Frame &fr = sprite.frames[sprite.cur_frame];
                            uint32_t color = ui_state.mouse_right_down ? rgba_u(0, 0, 0, 0) : cur_color;

                            if (cur_tool == TOOL_RECT)
                            {
                                draw_rect(fr, drag_start_canvas_x, drag_start_canvas_y, end_x, end_y, color, false);
                            }
                            else if (cur_tool == TOOL_CIRCLE)
                            {
                                int radius = static_cast<int>(std::sqrt(
                                    std::pow(end_x - drag_start_canvas_x, 2) +
                                    std::pow(end_y - drag_start_canvas_y, 2)));
                                draw_circle(fr, drag_start_canvas_x, drag_start_canvas_y, radius, color, false);
                            }
                            else if (cur_tool == TOOL_LINE)
                            {
                                draw_line(fr, drag_start_canvas_x, drag_start_canvas_y, end_x, end_y, color);
                            }
                        }

                        drag_start_canvas_x = -1;
                        drag_start_canvas_y = -1;
                    }
                }

                if (e.button.button == SDL_BUTTON_RIGHT)
                {
                    ui_state.mouse_right_down = false;
                }

                if (e.button.button == SDL_BUTTON_MIDDLE)
                {
                    panning = false;
                }

                last_paint_x = last_paint_y = -1;
            }

            if (e.type == SDL_MOUSEWHEEL)
            {
                // zoom with wheel
                if (e.wheel.y > 0)
                {
                    pixel_size = std::min(MAX_PIXEL_SIZE, pixel_size + 2);
                }
                else if (e.wheel.y < 0)
                {
                    pixel_size = std::max(MIN_PIXEL_SIZE, pixel_size - 2);
                }
            }

            if (e.type == SDL_MOUSEMOTION)
            {
                int mx = e.motion.x, my = e.motion.y;

                // panning
                if (panning)
                {
                    camera_x += (mx - ui_state.last_mouse_x);
                    camera_y += (my - ui_state.last_mouse_y);
                    canvas_x = static_cast<int>(camera_x);
                    canvas_y = static_cast<int>(camera_y);
                }

                // continuous painting while left down
                if (ui_state.mouse_left_down && drag_start_canvas_x == -1)
                {
                    int cx, cy;
                    if (screen_to_canvas(mx, my, cx, cy, canvas_x, canvas_y))
                    {
                        Frame &fr = sprite.frames[sprite.cur_frame];
                        if (cur_tool == TOOL_PENCIL || (ui_state.mouse_right_down && cur_tool != TOOL_EYEDROPPER))
                        {
                            // pencil or right-click quick eraser
                            if (last_paint_x != cx || last_paint_y != cy)
                            {
                                fr.at(cx, cy) = (ui_state.mouse_right_down ? rgba_u(0, 0, 0, 0) : cur_color);
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

                ui_state.last_mouse_x = mx;
                ui_state.last_mouse_y = my;
            }

            if (e.type == SDL_KEYDOWN)
            {
                SDL_Keycode k = e.key.keysym.sym;

                // Check shortcuts
                if (shortcuts.find(k) != shortcuts.end())
                {
                    shortcuts[k]();
                }

                if (k == SDLK_ESCAPE)
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
                int frame_idx = (sprite.cur_frame - i + static_cast<int>(sprite.frames.size())) % static_cast<int>(sprite.frames.size());
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
                int frame_idx = (sprite.cur_frame + i) % static_cast<int>(sprite.frames.size());
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

        // Draw UI panels
        ui_context.render(ren, font);

        // Draw cursor preview
        int cx, cy;
        if (screen_to_canvas(ui_state.last_mouse_x, ui_state.last_mouse_y, cx, cy, canvas_x, canvas_y))
        {
            SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);
            SDL_SetRenderDrawColor(ren, 255, 255, 255, 150);
            SDL_Rect cursor = {canvas_x + cx * pixel_size, canvas_y + cy * pixel_size, pixel_size, pixel_size};
            SDL_RenderDrawRect(ren, &cursor);
        }

        // Draw preview for shape tools
        if (ui_state.mouse_left_down && drag_start_canvas_x != -1)
        {
            int mx = ui_state.last_mouse_x, my = ui_state.last_mouse_y;
            int end_x, end_y;
            if (screen_to_canvas(mx, my, end_x, end_y, canvas_x, canvas_y))
            {
                SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);
                uint32_t color = ui_state.mouse_right_down ? rgba_u(0, 0, 0, 0) : cur_color;
                SDL_SetRenderDrawColor(ren, r_of(color), g_of(color), b_of(color), 150);

                if (cur_tool == TOOL_RECT)
                {
                    SDL_Rect preview = {
                        canvas_x + std::min(drag_start_canvas_x, end_x) * pixel_size,
                        canvas_y + std::min(drag_start_canvas_y, end_y) * pixel_size,
                        std::abs(end_x - drag_start_canvas_x) * pixel_size + pixel_size,
                        std::abs(end_y - drag_start_canvas_y) * pixel_size + pixel_size};
                    SDL_RenderDrawRect(ren, &preview);
                }
                else if (cur_tool == TOOL_LINE)
                {
                    SDL_RenderDrawLine(ren,
                                       canvas_x + drag_start_canvas_x * pixel_size + pixel_size / 2,
                                       canvas_y + drag_start_canvas_y * pixel_size + pixel_size / 2,
                                       canvas_x + end_x * pixel_size + pixel_size / 2,
                                       canvas_y + end_y * pixel_size + pixel_size / 2);
                }
            }
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