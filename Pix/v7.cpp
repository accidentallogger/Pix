// v2.cpp

#include <SFML/Graphics.hpp>
#include <vector>
#include <string>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <iostream>
#include <chrono>
#include <cstdint>
#include <cmath>
#include <cctype>

using u8 = sf::Uint8;
using u32 = uint32_t;

// Define Color struct FIRST
struct Color
{
    u8 r = 0, g = 0, b = 0, a = 255;
    Color() {}
    Color(u8 rr, u8 gg, u8 bb, u8 aa = 255) : r(rr), g(gg), b(bb), a(aa) {}
};

// Now define the 8-bit color palette AFTER Color struct
namespace EightBitColors
{
    const Color Black(0, 0, 0);
    const Color DarkBlue(0, 0, 168);
    const Color DarkPurple(87, 0, 127);
    const Color DarkGreen(0, 147, 0);
    const Color Brown(170, 85, 0);
    const Color DarkGray(85, 85, 85);
    const Color LightGray(170, 170, 170);
    const Color White(255, 255, 255);
    const Color Red(255, 0, 0);
    const Color Orange(255, 85, 0);
    const Color Yellow(255, 255, 0);
    const Color Green(0, 255, 0);
    const Color Blue(0, 0, 255);
    const Color Indigo(85, 0, 255);
    const Color Pink(255, 85, 255);
    const Color Peach(255, 187, 153);

    const std::vector<Color> Palette = {
        Black, DarkBlue, DarkPurple, DarkGreen, Brown, DarkGray,
        LightGray, White, Red, Orange, Yellow, Green,
        Blue, Indigo, Pink, Peach};
}

struct Frame
{
    std::string name = "Frame";
    sf::Image image;
    sf::Texture thumbnail;

    Frame() {}
    Frame(unsigned w, unsigned h, const std::string &n = "Frame") : name(n)
    {
        image.create(w, h, sf::Color(0, 0, 0, 0));
        updateThumbnail();
    }

    void clear()
    {
        auto s = image.getSize();
        image.create(s.x, s.y, sf::Color(0, 0, 0, 0));
        updateThumbnail();
    }

    sf::Color getPixel(unsigned x, unsigned y) const { return image.getPixel(x, y); }

    void setPixel(unsigned x, unsigned y, const sf::Color &c)
    {
        image.setPixel(x, y, c);
        updateThumbnail();
    }

    void updateThumbnail()
    {
        // Create a thumbnail (scaled down version)
        unsigned thumbSize = 48;
        sf::Image thumbImg;
        thumbImg.create(thumbSize, thumbSize, sf::Color(0, 0, 0, 0));

        auto size = image.getSize();
        if (size.x > 0 && size.y > 0)
        {
            for (unsigned y = 0; y < thumbSize; ++y)
            {
                for (unsigned x = 0; x < thumbSize; ++x)
                {
                    unsigned srcX = (x * size.x) / thumbSize;
                    unsigned srcY = (y * size.y) / thumbSize;
                    thumbImg.setPixel(x, y, image.getPixel(srcX, srcY));
                }
            }
        }

        thumbnail.loadFromImage(thumbImg);
    }
};

enum class Tool
{
    Pencil,
    Eraser,
    Fill
};

class Canvas
{
public:
    unsigned width, height;
    float zoom = 8.0f; // pixels -> screen
    sf::Vector2f pan{0, 0};
    std::vector<Frame> frames;
    int currentFrame = 0;
    Color drawColor{255, 0, 0, 255}; // Start with red for 8-bit vibe
    Tool currentTool = Tool::Pencil;
    bool showGrid = true;
    bool onionSkin = false;

    Canvas(unsigned w = 64, unsigned h = 64) : width(w), height(h)
    {
        frames.emplace_back(w, h, "Frame 0");
    }

    void resizeCanvas(unsigned newWidth, unsigned newHeight)
    {
        width = newWidth;
        height = newHeight;

        for (auto &frame : frames)
        {
            sf::Image newImage;
            newImage.create(newWidth, newHeight, sf::Color(0, 0, 0, 0));

            // Copy existing pixels from old image to new image
            auto oldSize = frame.image.getSize();
            for (unsigned y = 0; y < std::min(oldSize.y, newHeight); ++y)
            {
                for (unsigned x = 0; x < std::min(oldSize.x, newWidth); ++x)
                {
                    newImage.setPixel(x, y, frame.image.getPixel(x, y));
                }
            }

            frame.image = newImage;
            frame.updateThumbnail();
        }
    }

    void newProject(unsigned w, unsigned h)
    {
        width = w;
        height = h;
        frames.clear();
        frames.emplace_back(w, h, "Frame 0");
        currentFrame = 0;
        zoom = 8.0f;
        pan = {0, 0};
    }

    void addFrame()
    {
        frames.emplace_back(width, height, "Frame " + std::to_string(frames.size()));
        currentFrame = (int)frames.size() - 1;
    }

    void duplicateFrame()
    {
        if (frames.empty())
            return;

        Frame newFrame = frames[currentFrame];
        newFrame.name = frames[currentFrame].name + " copy";
        frames.insert(frames.begin() + currentFrame + 1, newFrame);
        currentFrame++;
    }

    void deleteFrame(int index)
    {
        if (frames.size() <= 1 || index < 0 || index >= (int)frames.size())
            return;

        // Delete the specified frame
        frames.erase(frames.begin() + index);

        // Adjust current frame index
        if (currentFrame >= (int)frames.size())
            currentFrame = (int)frames.size() - 1;
        else if (currentFrame >= index)
            currentFrame--;
    }

    void moveFrameUp()
    {
        if (currentFrame > 0)
        {
            std::swap(frames[currentFrame], frames[currentFrame - 1]);
            currentFrame--;
        }
    }

    void moveFrameDown()
    {
        if (currentFrame < (int)frames.size() - 1)
        {
            std::swap(frames[currentFrame], frames[currentFrame + 1]);
            currentFrame++;
        }
    }

    void nextFrame()
    {
        if (!frames.empty())
            currentFrame = (currentFrame + 1) % frames.size();
    }
    void prevFrame()
    {
        if (!frames.empty())
            currentFrame = (currentFrame - 1 + frames.size()) % frames.size();
    }

    sf::Image getCurrentFrameImage() const
    {
        return frames[currentFrame].image;
    }

    void setPixelAtCurrentFrame(int x, int y, const sf::Color &c)
    {
        if (x < 0 || y < 0 || x >= (int)width || y >= (int)height)
            return;
        Frame &f = frames[currentFrame];
        f.setPixel(x, y, c);
    }

    void floodFill(int sx, int sy, const sf::Color &newColor)
    {
        if (sx < 0 || sy < 0 || sx >= (int)width || sy >= (int)height)
            return;
        Frame &f = frames[currentFrame];
        sf::Color target = f.getPixel(sx, sy);
        if (target == newColor)
            return;
        std::vector<sf::Vector2i> stack;
        stack.emplace_back(sx, sy);
        while (!stack.empty())
        {
            auto p = stack.back();
            stack.pop_back();
            int x = p.x, y = p.y;
            if (x < 0 || y < 0 || x >= (int)width || y >= (int)height)
                continue;
            if (f.getPixel(x, y) != target)
                continue;
            f.setPixel(x, y, newColor);
            stack.emplace_back(x + 1, y);
            stack.emplace_back(x - 1, y);
            stack.emplace_back(x, y + 1);
            stack.emplace_back(x, y - 1);
        }
        f.updateThumbnail();
    }

    // Save and load .pix custom format
    bool saveToPix(const std::string &filename) const
    {
        std::ofstream ofs(filename, std::ios::binary);
        if (!ofs)
            return false;
        ofs.write("PIX1", 4); // magic
        auto write32 = [&](u32 v)
        { ofs.write((char *)&v, 4); };
        write32(width);
        write32(height);
        write32((u32)frames.size());
        for (const Frame &f : frames)
        {
            u32 nameLen = (u32)f.name.size();
            write32(nameLen);
            ofs.write(f.name.c_str(), nameLen);
            // raw pixels RGBA
            const sf::Image &img = f.image;
            const sf::Uint8 *ptr = img.getPixelsPtr();
            u32 total = width * height * 4;
            ofs.write((char *)ptr, total);
        }
        return true;
    }

    bool loadFromPix(const std::string &filename)
    {
        std::ifstream ifs(filename, std::ios::binary);
        if (!ifs)
            return false;
        char magic[5] = {0};
        ifs.read(magic, 4);
        if (std::string(magic) != "PIX1")
            return false;
        auto read32 = [&]() -> u32
        { u32 v; ifs.read((char*)&v,4); return v; };
        u32 w = read32(), h = read32();
        u32 fcount = read32();
        frames.clear();
        width = w;
        height = h;
        for (u32 fi = 0; fi < fcount; ++fi)
        {
            u32 nameLen = read32();
            std::string name(nameLen, '\0');
            ifs.read(&name[0], nameLen);
            Frame f(width, height, name);
            u32 total = width * height * 4;
            std::vector<sf::Uint8> buf(total);
            ifs.read((char *)buf.data(), total);
            f.image.create(width, height, buf.data());
            f.updateThumbnail();
            frames.push_back(std::move(f));
        }
        currentFrame = 0;
        return true;
    }

    // Export current frame or all frames as PNGs
    bool exportCurrentFramePNG(const std::string &filename) const
    {
        sf::Image img = frames[currentFrame].image;
        return img.saveToFile(filename);
    }
    bool exportAllFramesPNG(const std::string &basename) const
    {
        for (size_t i = 0; i < frames.size(); ++i)
        {
            std::ostringstream oss;
            oss << basename << "_" << i << ".png";
            if (!frames[i].image.saveToFile(oss.str()))
                return false;
        }
        return true;
    }
};

class ColorPicker
{
public:
    bool isOpen = false;
    sf::Vector2f position{100, 100};
    sf::Vector2f size{200, 200};
    Color currentColor{255, 0, 0};

    void draw(sf::RenderWindow &window, sf::Font &font)
    {
        if (!isOpen)
            return;

        // Background with 8-bit style
        sf::RectangleShape bg(size);
        bg.setPosition(position);
        bg.setFillColor(sf::Color(EightBitColors::DarkBlue.r, EightBitColors::DarkBlue.g, EightBitColors::DarkBlue.b));
        bg.setOutlineColor(sf::Color(EightBitColors::Yellow.r, EightBitColors::Yellow.g, EightBitColors::Yellow.b));
        bg.setOutlineThickness(2);
        window.draw(bg);

        // Title with 8-bit font style
        sf::Text title("8-BIT COLOR PICKER", font, 14);
        title.setPosition(position.x + 10, position.y + 5);
        title.setFillColor(sf::Color(EightBitColors::Yellow.r, EightBitColors::Yellow.g, EightBitColors::Yellow.b));
        title.setStyle(sf::Text::Bold);
        window.draw(title);

        // Draw 8-bit color palette squares (4x4 grid)
        float cellSize = 30;
        float startX = position.x + 10;
        float startY = position.y + 30;

        for (int i = 0; i < 16; ++i)
        {
            int row = i / 4;
            int col = i % 4;

            sf::RectangleShape colorRect({cellSize, cellSize});
            colorRect.setPosition(startX + col * (cellSize + 5), startY + row * (cellSize + 5));
            colorRect.setFillColor(sf::Color(
                EightBitColors::Palette[i].r,
                EightBitColors::Palette[i].g,
                EightBitColors::Palette[i].b));
            colorRect.setOutlineColor(sf::Color(EightBitColors::LightGray.r, EightBitColors::LightGray.g, EightBitColors::LightGray.b));
            colorRect.setOutlineThickness(1);
            window.draw(colorRect);

            // Highlight selected color
            if (currentColor.r == EightBitColors::Palette[i].r &&
                currentColor.g == EightBitColors::Palette[i].g &&
                currentColor.b == EightBitColors::Palette[i].b)
            {
                sf::RectangleShape highlight({cellSize + 4, cellSize + 4});
                highlight.setPosition(startX + col * (cellSize + 5) - 2, startY + row * (cellSize + 5) - 2);
                highlight.setFillColor(sf::Color::Transparent);
                highlight.setOutlineColor(sf::Color(EightBitColors::White.r, EightBitColors::White.g, EightBitColors::White.b));
                highlight.setOutlineThickness(2);
                window.draw(highlight);
            }
        }

        // Current color preview with 8-bit style
        sf::RectangleShape preview({60, 40});
        preview.setPosition(position.x + size.x - 70, position.y + size.y - 50);
        preview.setFillColor(sf::Color(currentColor.r, currentColor.g, currentColor.b));
        preview.setOutlineColor(sf::Color(EightBitColors::White.r, EightBitColors::White.g, EightBitColors::White.b));
        preview.setOutlineThickness(2);
        window.draw(preview);

        // Close button with 8-bit style
        sf::RectangleShape closeBtn({70, 25});
        closeBtn.setPosition(position.x + size.x - 80, position.y + size.y - 25);
        closeBtn.setFillColor(sf::Color(EightBitColors::Red.r, EightBitColors::Red.g, EightBitColors::Red.b));
        closeBtn.setOutlineColor(sf::Color(EightBitColors::White.r, EightBitColors::White.g, EightBitColors::White.b));
        closeBtn.setOutlineThickness(1);
        window.draw(closeBtn);

        sf::Text closeText("CLOSE", font, 12);
        closeText.setPosition(closeBtn.getPosition().x + 10, closeBtn.getPosition().y + 5);
        closeText.setFillColor(sf::Color(EightBitColors::White.r, EightBitColors::White.g, EightBitColors::White.b));
        closeText.setStyle(sf::Text::Bold);
        window.draw(closeText);
    }

    bool handleClick(sf::Vector2i mousePos, Color &targetColor)
    {
        if (!isOpen)
            return false;

        // Check color palette clicks
        float cellSize = 30;
        float startX = position.x + 10;
        float startY = position.y + 30;

        for (int i = 0; i < 16; ++i)
        {
            int row = i / 4;
            int col = i % 4;

            sf::FloatRect colorRect(
                startX + col * (cellSize + 5),
                startY + row * (cellSize + 5),
                cellSize,
                cellSize);

            if (colorRect.contains(mousePos.x, mousePos.y))
            {
                currentColor = EightBitColors::Palette[i];
                targetColor = currentColor;
                return true;
            }
        }

        // Check close button
        sf::FloatRect closeBtn(position.x + size.x - 80, position.y + size.y - 25, 70, 25);
        if (closeBtn.contains(mousePos.x, mousePos.y))
        {
            isOpen = false;
            return true;
        }

        return false;
    }
};

// Utility: draw a rectangle button with 8-bit style
void drawButton(sf::RenderWindow &w, const sf::FloatRect &rect, const sf::Font &font,
                const std::string &label, bool isActive = false, bool isHovered = false)
{
    // 8-bit button style
    sf::Color bgColor = isActive ? sf::Color(EightBitColors::Blue.r, EightBitColors::Blue.g, EightBitColors::Blue.b) : sf::Color(EightBitColors::DarkPurple.r, EightBitColors::DarkPurple.g, EightBitColors::DarkPurple.b);

    if (isHovered && !isActive)
    {
        bgColor = sf::Color(EightBitColors::DarkBlue.r, EightBitColors::DarkBlue.g, EightBitColors::DarkBlue.b);
    }

    sf::RectangleShape rs({rect.width, rect.height});
    rs.setPosition(rect.left, rect.top);
    rs.setFillColor(bgColor);
    rs.setOutlineColor(isActive ? sf::Color(EightBitColors::Yellow.r, EightBitColors::Yellow.g, EightBitColors::Yellow.b) : sf::Color(EightBitColors::LightGray.r, EightBitColors::LightGray.g, EightBitColors::LightGray.b));
    rs.setOutlineThickness(2);
    w.draw(rs);

    // 8-bit text style
    sf::Text t(label, font, 12);
    t.setStyle(sf::Text::Bold);
    sf::FloatRect textBounds = t.getLocalBounds();
    t.setPosition(rect.left + (rect.width - textBounds.width) / 2,
                  rect.top + (rect.height - textBounds.height) / 2 - 2);
    t.setFillColor(sf::Color(EightBitColors::White.r, EightBitColors::White.g, EightBitColors::White.b));
    w.draw(t);
}

// Draw 8-bit style panel
void drawPanel(sf::RenderWindow &w, const sf::FloatRect &rect, const std::string &title = "", const sf::Font &font = sf::Font())
{
    // Main panel
    sf::RectangleShape panel({rect.width, rect.height});
    panel.setPosition(rect.left, rect.top);
    panel.setFillColor(sf::Color(EightBitColors::DarkBlue.r, EightBitColors::DarkBlue.g, EightBitColors::DarkBlue.b));
    panel.setOutlineColor(sf::Color(EightBitColors::LightGray.r, EightBitColors::LightGray.g, EightBitColors::LightGray.b));
    panel.setOutlineThickness(2);
    w.draw(panel);

    // Title if provided
    if (!title.empty() && font.getInfo().family != "")
    {
        sf::Text titleText(title, font, 14);
        titleText.setStyle(sf::Text::Bold);
        titleText.setPosition(rect.left + 10, rect.top + 5);
        titleText.setFillColor(sf::Color(EightBitColors::Yellow.r, EightBitColors::Yellow.g, EightBitColors::Yellow.b));
        w.draw(titleText);
    }
}

int main()
{
    // Basic parameters
    unsigned initW = 64, initH = 64;
    Canvas canvas(initW, initH);

    // Window & view setup
    sf::RenderWindow window(sf::VideoMode(1100, 700), "PIXEL8 - 8-Bit Pixel Editor", sf::Style::Close | sf::Style::Titlebar);
    window.setVerticalSyncEnabled(true);

    // Load a retro-style font
    sf::Font font;
    if (!font.loadFromFile("fonts/ARIAL.TTF"))
    {
        // Try to load a pixel font instead
        if (!font.loadFromFile("fonts/FFFFORWA.TTF"))
        { // Example pixel font name
            std::cerr << "Failed to load font! Buttons will not have text labels.\n";
        }
    }

    bool running = true;
    bool leftMouseDown = false, middleMouseDown = false;
    sf::Vector2i lastMouse;
    bool isSpacePan = false;

    // Animation preview
    bool playing = false;
    float fps = 6.0f;
    float playTimer = 0.f;

    // Color picker
    ColorPicker colorPicker;

    // Canvas resize dialog
    bool showResizeDialog = false;
    std::string newWidthStr = "64", newHeightStr = "64";
    bool editingWidth = true;

    // Track if we just clicked a UI element (to prevent drawing on UI clicks)
    bool uiElementClicked = false;

    // Frame dragging
    int draggingFrame = -1;
    sf::Vector2f dragOffset;

    // Track duplicate/delete clicks to prevent multiple calls
    bool duplicateClicked = false;
    bool deleteClicked = false;

    // Frame renaming
    bool renamingFrame = false;
    int frameToRename = -1;
    std::string frameNameInput;

    bool widthInputActive = false;
    bool heightInputActive = false;

    // Hover states for buttons
    int hoveredToolButton = -1;
    int hoveredFrameButton = -1;

    sf::Clock clock;
    while (running)
    {
        float dt = clock.restart().asSeconds();

        // Reset UI click tracking (but keep track of specific button clicks)
        uiElementClicked = false;
        hoveredToolButton = -1;
        hoveredFrameButton = -1;

        // event handling
        sf::Event ev;
        while (window.pollEvent(ev))
        {
            if (ev.type == sf::Event::Closed)
                running = false;
            else if (ev.type == sf::Event::MouseWheelScrolled)
            {
                if (ev.mouseWheelScroll.delta > 0)
                    canvas.zoom *= 1.1f;
                else
                    canvas.zoom /= 1.1f;
                if (canvas.zoom < 1.0f)
                    canvas.zoom = 1.0f;
                if (canvas.zoom > 64.0f)
                    canvas.zoom = 64.0f;
            }
            else if (ev.type == sf::Event::MouseButtonPressed)
            {
                if (ev.mouseButton.button == sf::Mouse::Left)
                {
                    leftMouseDown = true;
                    // Reset button click flags on new mouse press
                    duplicateClicked = false;
                    deleteClicked = false;
                }
                if (ev.mouseButton.button == sf::Mouse::Middle)
                    middleMouseDown = true;
                lastMouse = sf::Mouse::getPosition(window);
            }
            else if (ev.type == sf::Event::MouseButtonReleased)
            {
                if (ev.mouseButton.button == sf::Mouse::Left)
                {
                    leftMouseDown = false;
                    draggingFrame = -1;
                }
                if (ev.mouseButton.button == sf::Mouse::Middle)
                    middleMouseDown = false;
            }
            else if (ev.type == sf::Event::KeyPressed)
            {
                bool ctrl = ev.key.control;
                if (ctrl && ev.key.code == sf::Keyboard::N)
                {
                    canvas.newProject(64, 64);
                }
                else if (ctrl && ev.key.code == sf::Keyboard::S)
                {
                    // save
                    canvas.saveToPix("project.pix");
                    std::cout << "Saved project.pix\n";
                }
                else if (ctrl && ev.key.shift && ev.key.code == sf::Keyboard::S)
                {
                    // export frames as PNG sequence
                    canvas.exportAllFramesPNG("export/frame");
                    std::cout << "Exported frames to export/frame_#.png\n";
                }
                else if (ev.key.code == sf::Keyboard::Space)
                {
                    playing = !playing;
                }
                else if (ev.key.code == sf::Keyboard::G)
                {
                    canvas.showGrid = !canvas.showGrid;
                }
                else if (ev.key.code == sf::Keyboard::O)
                {
                    canvas.onionSkin = !canvas.onionSkin;
                }
                else if (ev.key.code == sf::Keyboard::Right)
                {
                    canvas.nextFrame();
                }
                else if (ev.key.code == sf::Keyboard::Left)
                {
                    canvas.prevFrame();
                }
                else if (ev.key.code == sf::Keyboard::R && ctrl)
                {
                    showResizeDialog = !showResizeDialog;
                    newWidthStr = std::to_string(canvas.width);
                    newHeightStr = std::to_string(canvas.height);
                    widthInputActive = true; // Start with width input active
                    heightInputActive = false;
                }
                else if (ev.key.code == sf::Keyboard::Tab && showResizeDialog)
                {
                    // Switch between width and height inputs
                    if (widthInputActive)
                    {
                        widthInputActive = false;
                        heightInputActive = true;
                    }
                    else
                    {
                        widthInputActive = true;
                        heightInputActive = false;
                    }
                }
                else if (ev.key.code == sf::Keyboard::Enter)
                {
                    if (renamingFrame)
                    {
                        // Finish renaming
                        if (!frameNameInput.empty())
                        {
                            canvas.frames[frameToRename].name = frameNameInput;
                        }
                        renamingFrame = false;
                        frameToRename = -1;
                    }
                    else if (showResizeDialog)
                    {
                        // Apply resize when Enter is pressed
                        try
                        {
                            unsigned newWidth = std::stoi(newWidthStr);
                            unsigned newHeight = std::stoi(newHeightStr);
                            if (newWidth > 0 && newWidth < 1024 && newHeight > 0 && newHeight < 1024)
                            {
                                canvas.resizeCanvas(newWidth, newHeight);
                                showResizeDialog = false;
                                widthInputActive = false;
                                heightInputActive = false;
                            }
                        }
                        catch (...)
                        {
                            // Invalid input, ignore
                            std::cout << "Invalid input for resize!\n";
                        }
                    }
                }
                else if (ev.key.code == sf::Keyboard::Escape)
                {
                    if (renamingFrame)
                    {
                        // Cancel renaming
                        renamingFrame = false;
                        frameToRename = -1;
                    }
                    else if (showResizeDialog)
                    {
                        // Cancel resize dialog
                        showResizeDialog = false;
                        widthInputActive = false;
                        heightInputActive = false;
                    }
                }
            }
            else if (ev.type == sf::Event::TextEntered)
            {
                if (showResizeDialog && (widthInputActive || heightInputActive))
                {
                    if (ev.text.unicode == '\b') // Backspace
                    {
                        if (widthInputActive && !newWidthStr.empty())
                            newWidthStr.pop_back();
                        else if (heightInputActive && !newHeightStr.empty())
                            newHeightStr.pop_back();
                    }
                    else if (ev.text.unicode >= '0' && ev.text.unicode <= '9')
                    {
                        if (widthInputActive)
                        {
                            // Limit width input to reasonable length
                            if (newWidthStr.length() < 4)
                                newWidthStr += static_cast<char>(ev.text.unicode);
                        }
                        else if (heightInputActive)
                        {
                            // Limit height input to reasonable length
                            if (newHeightStr.length() < 4)
                                newHeightStr += static_cast<char>(ev.text.unicode);
                        }
                    }
                }
                else if (renamingFrame)
                {
                    if (ev.text.unicode == '\b') // Backspace
                    {
                        if (!frameNameInput.empty())
                            frameNameInput.pop_back();
                    }
                    else if (ev.text.unicode >= 32 && ev.text.unicode < 128) // Printable characters
                    {
                        frameNameInput += static_cast<char>(ev.text.unicode);
                    }
                }
            }
        } // end events

        // UI layout measurements
        const float toolbarH = 48;
        const float sidebarW = 260;
        sf::Vector2u winSize = window.getSize();
        sf::FloatRect canvasArea(8, toolbarH + 8, (float)winSize.x - sidebarW - 24, (float)winSize.y - toolbarH - 16);

        // Mouse pos and mapping to canvas coords
        sf::Vector2i mpos = sf::Mouse::getPosition(window);
        bool mouseInCanvas = (mpos.x >= (int)canvasArea.left && mpos.x < (int)(canvasArea.left + canvasArea.width) && mpos.y >= (int)canvasArea.top && mpos.y < (int)(canvasArea.top + canvasArea.height));

        // Pan with middle drag or spacebar + left drag
        if (middleMouseDown || (sf::Keyboard::isKeyPressed(sf::Keyboard::Space) && leftMouseDown))
        {
            sf::Vector2i cur = mpos;
            sf::Vector2f diff((float)(cur.x - lastMouse.x), (float)(cur.y - lastMouse.y));
            canvas.pan += diff;
            lastMouse = cur;
        }

        // Handle color picker clicks
        if (leftMouseDown)
        {
            if (colorPicker.handleClick(mpos, canvas.drawColor))
            {
                uiElementClicked = true;
            }
        }

        // Drawing: convert mouse to pixel coords
        if (leftMouseDown && mouseInCanvas && !uiElementClicked && !colorPicker.isOpen && !showResizeDialog && !renamingFrame)
        {
            // map mouse to canvas pixel position
            float localX = (mpos.x - canvasArea.left - canvas.pan.x) / canvas.zoom;
            float localY = (mpos.y - canvasArea.top - canvas.pan.y) / canvas.zoom;
            int px = (int)std::floor(localX);
            int py = (int)std::floor(localY);

            if (px >= 0 && py >= 0 && px < (int)canvas.width && py < (int)canvas.height)
            {
                if (canvas.currentTool == Tool::Pencil)
                {
                    canvas.setPixelAtCurrentFrame(px, py, sf::Color(canvas.drawColor.r, canvas.drawColor.g, canvas.drawColor.b, canvas.drawColor.a));
                }
                else if (canvas.currentTool == Tool::Eraser)
                {
                    canvas.setPixelAtCurrentFrame(px, py, sf::Color(0, 0, 0, 0));
                }
                else if (canvas.currentTool == Tool::Fill)
                {
                    canvas.floodFill(px, py, sf::Color(canvas.drawColor.r, canvas.drawColor.g, canvas.drawColor.b, canvas.drawColor.a));
                    // after a fill, stop continuous filling on drag
                    leftMouseDown = false;
                }
            }
        }

        // simple animation playback
        if (playing && canvas.frames.size() > 1)
        {
            playTimer += dt;
            if (playTimer >= 1.0f / fps)
            {
                playTimer = 0;
                canvas.nextFrame();
            }
        }

        // --- Rendering with 8-bit style ---
        window.clear(sf::Color(EightBitColors::DarkPurple.r, EightBitColors::DarkPurple.g, EightBitColors::DarkPurple.b)); // 8-bit background

        // Draw main panels with 8-bit style
        drawPanel(window, sf::FloatRect(4, 4, (float)winSize.x - sidebarW - 8, toolbarH - 4), "TOOLS", font);
        drawPanel(window, sf::FloatRect(canvasArea.left - 4, canvasArea.top - 4, canvasArea.width + 8, canvasArea.height + 8), "CANVAS", font);
        drawPanel(window, sf::FloatRect(canvasArea.left + canvasArea.width + 4, 4, sidebarW - 8, (float)winSize.y - 8), "ANIMATION", font);

        // Toolbar buttons: Pencil, Eraser, Fill
        float x = 8;
        float y = 8;
        float bw = 64, bh = 32, spacing = 6;

        // Check hover states for tool buttons
        if (!uiElementClicked && !colorPicker.isOpen && !showResizeDialog && !renamingFrame)
        {
            // Pencil button hover
            if (mpos.x >= (int)x && mpos.x <= (int)(x + bw) && mpos.y >= (int)y && mpos.y <= (int)(y + bh))
            {
                hoveredToolButton = 0;
            }
            // Eraser button hover
            else if (mpos.x >= (int)(x + bw + spacing) && mpos.x <= (int)(x + bw + spacing + bw) &&
                     mpos.y >= (int)y && mpos.y <= (int)(y + bh))
            {
                hoveredToolButton = 1;
            }
            // Fill button hover
            else if (mpos.x >= (int)(x + 2 * (bw + spacing)) && mpos.x <= (int)(x + 2 * (bw + spacing) + bw) &&
                     mpos.y >= (int)y && mpos.y <= (int)(y + bh))
            {
                hoveredToolButton = 2;
            }
        }

        // Pencil button
        drawButton(window, sf::FloatRect(x, y, bw, bh), font, "PENCIL",
                   canvas.currentTool == Tool::Pencil, hoveredToolButton == 0);
        if (leftMouseDown && hoveredToolButton == 0)
        {
            canvas.currentTool = Tool::Pencil;
            uiElementClicked = true;
        }

        x += bw + spacing;

        // Eraser button
        drawButton(window, sf::FloatRect(x, y, bw, bh), font, "ERASER",
                   canvas.currentTool == Tool::Eraser, hoveredToolButton == 1);
        if (leftMouseDown && hoveredToolButton == 1)
        {
            canvas.currentTool = Tool::Eraser;
            uiElementClicked = true;
        }

        x += bw + spacing;

        // Fill button
        drawButton(window, sf::FloatRect(x, y, bw, bh), font, "FILL",
                   canvas.currentTool == Tool::Fill, hoveredToolButton == 2);
        if (leftMouseDown && hoveredToolButton == 2)
        {
            canvas.currentTool = Tool::Fill;
            uiElementClicked = true;
        }

        // Color preview with 8-bit style
        float colorX = x + bw + spacing;
        sf::RectangleShape colPreview({36, 36});
        colPreview.setPosition(colorX, y - 2);
        colPreview.setFillColor(sf::Color(canvas.drawColor.r, canvas.drawColor.g, canvas.drawColor.b, 255));
        colPreview.setOutlineColor(sf::Color(EightBitColors::White.r, EightBitColors::White.g, EightBitColors::White.b));
        colPreview.setOutlineThickness(2);
        window.draw(colPreview);

        // Color picker button
        bool colorBtnHovered = (mpos.x >= (int)(colorX + 40) && mpos.x <= (int)(colorX + 40 + 60) &&
                                mpos.y >= (int)y && mpos.y <= (int)(y + bh));
        drawButton(window, sf::FloatRect(colorX + 40, y, 60, bh), font, "COLORS", false, colorBtnHovered);
        if (leftMouseDown && colorBtnHovered)
        {
            colorPicker.isOpen = !colorPicker.isOpen;
            uiElementClicked = true;
        }

        // Resize canvas button
        bool resizeBtnHovered = (mpos.x >= (int)(colorX + 110) && mpos.x <= (int)(colorX + 110 + 80) &&
                                 mpos.y >= (int)y && mpos.y <= (int)(y + bh));
        drawButton(window, sf::FloatRect(colorX + 110, y, 80, bh), font, "RESIZE", false, resizeBtnHovered);
        if (leftMouseDown && resizeBtnHovered)
        {
            showResizeDialog = !showResizeDialog;
            newWidthStr = std::to_string(canvas.width);
            newHeightStr = std::to_string(canvas.height);
            editingWidth = true;
            uiElementClicked = true;
        }

        // Draw canvas background (8-bit style checkerboard)
        sf::RectangleShape canvasBg({canvasArea.width, canvasArea.height});
        canvasBg.setPosition(canvasArea.left, canvasArea.top);
        canvasBg.setFillColor(sf::Color(EightBitColors::Black.r, EightBitColors::Black.g, EightBitColors::Black.b));
        window.draw(canvasBg);

        // Get current frame image & draw scaled by zoom and pan
        sf::Image comp = canvas.getCurrentFrameImage();

        // onion skin: draw previous frame with low alpha behind
        if (canvas.onionSkin && canvas.frames.size() > 1)
        {
            int prev = (canvas.currentFrame - 1 + canvas.frames.size()) % canvas.frames.size();
            // draw prev frame under comp
            sf::Texture pt;
            pt.loadFromImage(canvas.frames[prev].image);
            sf::Sprite ps(pt);
            ps.setScale(canvas.zoom, canvas.zoom);
            ps.setPosition(canvasArea.left + canvas.pan.x, canvasArea.top + canvas.pan.y);
            ps.setColor(sf::Color(255, 255, 255, 100)); // semi-transparent
            window.draw(ps);
        }

        sf::Texture tex;
        tex.create(canvas.width, canvas.height);
        tex.update(comp);
        sf::Sprite sprite(tex);
        sprite.setScale(canvas.zoom, canvas.zoom);
        sprite.setPosition(canvasArea.left + canvas.pan.x, canvasArea.top + canvas.pan.y);
        window.draw(sprite);

        // Optionally draw grid lines with 8-bit color
        if (canvas.showGrid)
        {
            sf::VertexArray lines(sf::Lines);
            for (unsigned xg = 0; xg <= canvas.width; ++xg)
            {
                float sxp = canvasArea.left + canvas.pan.x + xg * canvas.zoom;
                lines.append(sf::Vertex({sxp, canvasArea.top + canvas.pan.y},
                                        sf::Color(EightBitColors::DarkGray.r, EightBitColors::DarkGray.g, EightBitColors::DarkGray.b)));
                lines.append(sf::Vertex({sxp, canvasArea.top + canvas.pan.y + canvas.height * canvas.zoom},
                                        sf::Color(EightBitColors::DarkGray.r, EightBitColors::DarkGray.g, EightBitColors::DarkGray.b)));
            }
            for (unsigned yg = 0; yg <= canvas.height; ++yg)
            {
                float syp = canvasArea.top + canvas.pan.y + yg * canvas.zoom;
                lines.append(sf::Vertex({canvasArea.left + canvas.pan.x, syp},
                                        sf::Color(EightBitColors::DarkGray.r, EightBitColors::DarkGray.g, EightBitColors::DarkGray.b)));
                lines.append(sf::Vertex({canvasArea.left + canvas.pan.x + canvas.width * canvas.zoom, syp},
                                        sf::Color(EightBitColors::DarkGray.r, EightBitColors::DarkGray.g, EightBitColors::DarkGray.b)));
            }
            window.draw(lines);
        }

        // Sidebar area (frames)
        sf::FloatRect sidebar(canvasArea.left + canvasArea.width + 8, 4, sidebarW - 8, (float)winSize.y - 8);

        // Animation controls
        float controlY = sidebar.top + 30;

        // Play/Stop button with hover
        bool playBtnHovered = (mpos.x >= (int)(sidebar.left + 8) && mpos.x <= (int)(sidebar.left + 68) &&
                               mpos.y >= (int)controlY && mpos.y <= (int)(controlY + 28));
        drawButton(window, sf::FloatRect(sidebar.left + 8, controlY, 60, 28), font, playing ? "STOP" : "PLAY", playing, playBtnHovered);
        if (leftMouseDown && playBtnHovered)
        {
            playing = !playing;
            uiElementClicked = true;
        }

        // Prev frame button
        bool prevBtnHovered = (mpos.x >= (int)(sidebar.left + 76) && mpos.x <= (int)(sidebar.left + 104) &&
                               mpos.y >= (int)controlY && mpos.y <= (int)(controlY + 28));
        drawButton(window, sf::FloatRect(sidebar.left + 76, controlY, 28, 28), font, "<", false, prevBtnHovered);
        if (leftMouseDown && prevBtnHovered)
        {
            canvas.prevFrame();
            uiElementClicked = true;
        }

        // Next frame button
        bool nextBtnHovered = (mpos.x >= (int)(sidebar.left + 112) && mpos.x <= (int)(sidebar.left + 140) &&
                               mpos.y >= (int)controlY && mpos.y <= (int)(controlY + 28));
        drawButton(window, sf::FloatRect(sidebar.left + 112, controlY, 28, 28), font, ">", false, nextBtnHovered);
        if (leftMouseDown && nextBtnHovered)
        {
            canvas.nextFrame();
            uiElementClicked = true;
        }

        // Frame list
        float fy = controlY + 40;
        sf::Text ft("FRAMES", font, 14);
        ft.setStyle(sf::Text::Bold);
        ft.setPosition(sidebar.left + 8, fy);
        ft.setFillColor(sf::Color(EightBitColors::Yellow.r, EightBitColors::Yellow.g, EightBitColors::Yellow.b));
        window.draw(ft);
        fy += 25;

        float itemH = 60;
        for (size_t i = 0; i < canvas.frames.size(); ++i)
        {
            sf::FloatRect r(sidebar.left + 8, fy, sidebar.width - 32, itemH - 4);

            // Draw frame item background with 8-bit style
            sf::RectangleShape item({r.width, r.height});
            item.setPosition(r.left, r.top);
            item.setFillColor(i == canvas.currentFrame ? sf::Color(EightBitColors::Blue.r, EightBitColors::Blue.g, EightBitColors::Blue.b) : sf::Color(EightBitColors::DarkBlue.r, EightBitColors::DarkBlue.g, EightBitColors::DarkBlue.b));
            item.setOutlineColor(i == canvas.currentFrame ? sf::Color(EightBitColors::Yellow.r, EightBitColors::Yellow.g, EightBitColors::Yellow.b) : sf::Color(EightBitColors::LightGray.r, EightBitColors::LightGray.g, EightBitColors::LightGray.b));
            item.setOutlineThickness(2);
            window.draw(item);

            // Draw thumbnail with 8-bit border
            sf::Sprite thumb(canvas.frames[i].thumbnail);
            thumb.setPosition(r.left + 4, r.top + 4);

            // Add border around thumbnail
            sf::RectangleShape thumbBorder({52, 52});
            thumbBorder.setPosition(r.left + 2, r.top + 2);
            thumbBorder.setFillColor(sf::Color::Transparent);
            thumbBorder.setOutlineColor(sf::Color(EightBitColors::White.r, EightBitColors::White.g, EightBitColors::White.b));
            thumbBorder.setOutlineThickness(1);
            window.draw(thumbBorder);

            window.draw(thumb);

            // Draw frame info
            if (renamingFrame && frameToRename == (int)i)
            {
                // Draw editable frame name with 8-bit style
                sf::RectangleShape nameInput({120, 18});
                nameInput.setPosition(r.left + 56, r.top + 8);
                nameInput.setFillColor(sf::Color(EightBitColors::White.r, EightBitColors::White.g, EightBitColors::White.b));
                nameInput.setOutlineColor(sf::Color(EightBitColors::Yellow.r, EightBitColors::Yellow.g, EightBitColors::Yellow.b));
                nameInput.setOutlineThickness(1);
                window.draw(nameInput);

                sf::Text t(frameNameInput, font, 13);
                t.setStyle(sf::Text::Bold);
                t.setPosition(r.left + 58, r.top + 10);
                t.setFillColor(sf::Color(EightBitColors::Black.r, EightBitColors::Black.g, EightBitColors::Black.b));
                window.draw(t);
            }
            else
            {
                // Draw regular frame name
                sf::Text t(canvas.frames[i].name, font, 13);
                t.setStyle(sf::Text::Bold);
                t.setPosition(r.left + 56, r.top + 8);
                t.setFillColor(sf::Color(EightBitColors::White.r, EightBitColors::White.g, EightBitColors::White.b));
                window.draw(t);
            }

            // Draw frame controls with 8-bit style
            float buttonY = r.top + 30;
            drawButton(window, sf::FloatRect(r.left + 56, buttonY, 20, 20), font, "up", false, false);
            drawButton(window, sf::FloatRect(r.left + 80, buttonY, 20, 20), font, "dn", false, false);
            drawButton(window, sf::FloatRect(r.left + 104, buttonY, 20, 20), font, "D", false, false);
            drawButton(window, sf::FloatRect(r.left + 128, buttonY, 20, 20), font, "X", false, false);

            // Handle frame selection and controls
            if (leftMouseDown && !uiElementClicked && !renamingFrame)
            {
                sf::Vector2i mm = sf::Mouse::getPosition(window);
                if (mm.x >= (int)r.left && mm.x <= (int)(r.left + r.width) && mm.y >= (int)r.top && mm.y <= (int)(r.top + r.height))
                {
                    // Check which part was clicked
                    if (mm.x >= (int)r.left && mm.x <= (int)(r.left + 52) && mm.y >= (int)r.top && mm.y <= (int)(r.top + 52))
                    {
                        // Thumbnail clicked - select frame
                        canvas.currentFrame = (int)i;
                        uiElementClicked = true;
                    }
                    else if (mm.x >= (int)(r.left + 56) && mm.x <= (int)(r.left + 76) && mm.y >= (int)buttonY && mm.y <= (int)(buttonY + 20))
                    {
                        // Move up
                        canvas.moveFrameUp();
                        uiElementClicked = true;
                    }
                    else if (mm.x >= (int)(r.left + 80) && mm.x <= (int)(r.left + 100) && mm.y >= (int)buttonY && mm.y <= (int)(buttonY + 20))
                    {
                        // Move down
                        canvas.moveFrameDown();
                        uiElementClicked = true;
                    }
                    else if (mm.x >= (int)(r.left + 104) && mm.x <= (int)(r.left + 124) && mm.y >= (int)buttonY && mm.y <= (int)(buttonY + 20) && !duplicateClicked)
                    {
                        // Duplicate - only once per click
                        canvas.duplicateFrame();
                        uiElementClicked = true;
                        duplicateClicked = true;
                    }
                    else if (mm.x >= (int)(r.left + 128) && mm.x <= (int)(r.left + 148) && mm.y >= (int)buttonY && mm.y <= (int)(buttonY + 20) && !deleteClicked)
                    {
                        // Delete - only once per click
                        canvas.deleteFrame((int)i);
                        uiElementClicked = true;
                        deleteClicked = true;
                    }
                    else if (mm.x >= (int)(r.left + 56) && mm.x <= (int)(r.left + 156) && mm.y >= (int)(r.top + 8) && mm.y <= (int)(r.top + 26))
                    {
                        // Frame name clicked - start renaming
                        renamingFrame = true;
                        frameToRename = (int)i;
                        frameNameInput = canvas.frames[i].name;
                        uiElementClicked = true;
                    }
                }
            }

            fy += itemH;
        }

        // Add frame button with 8-bit style
        bool addFrameHovered = (mpos.x >= (int)(sidebar.left + 8) && mpos.x <= (int)(sidebar.left + 88) &&
                                mpos.y >= (int)(fy + 8) && mpos.y <= (int)(fy + 36));
        drawButton(window, sf::FloatRect(sidebar.left + 8, fy + 8, 80, 28), font, "+ FRAME", false, addFrameHovered);
        if (leftMouseDown && addFrameHovered && !renamingFrame)
        {
            canvas.addFrame();
            uiElementClicked = true;
        }

        // Export button with 8-bit style
        bool exportHovered = (mpos.x >= (int)(sidebar.left + 96) && mpos.x <= (int)(sidebar.left + 176) &&
                              mpos.y >= (int)(fy + 8) && mpos.y <= (int)(fy + 36));
        drawButton(window, sf::FloatRect(sidebar.left + 96, fy + 8, 80, 28), font, "EXPORT", false, exportHovered);
        if (leftMouseDown && exportHovered && !renamingFrame)
        {
            canvas.exportCurrentFramePNG("export/frame.png");
            std::cout << "Exported current frame to export/frame.png\n";
            uiElementClicked = true;
        }

        // Draw color picker if open
        colorPicker.draw(window, font);

        // Draw resize dialog with 8-bit style
        // Draw resize dialog with 8-bit style
        if (showResizeDialog)
        {
            sf::Vector2f dialogSize(250, 150); // Increased height to accommodate better layout
            sf::Vector2f dialogPos(winSize.x / 2 - dialogSize.x / 2, winSize.y / 2 - dialogSize.y / 2);

            // Background with 8-bit style
            sf::RectangleShape dialogBg(dialogSize);
            dialogBg.setPosition(dialogPos);
            dialogBg.setFillColor(sf::Color(EightBitColors::DarkBlue.r, EightBitColors::DarkBlue.g, EightBitColors::DarkBlue.b));
            dialogBg.setOutlineColor(sf::Color(EightBitColors::Yellow.r, EightBitColors::Yellow.g, EightBitColors::Yellow.b));
            dialogBg.setOutlineThickness(2);
            window.draw(dialogBg);

            // Title
            sf::Text title("RESIZE CANVAS", font, 16);
            title.setStyle(sf::Text::Bold);
            title.setPosition(dialogPos.x + 10, dialogPos.y + 10);
            title.setFillColor(sf::Color(EightBitColors::Yellow.r, EightBitColors::Yellow.g, EightBitColors::Yellow.b));
            window.draw(title);

            // Width input
            sf::Text widthLabel("WIDTH:", font, 14);
            widthLabel.setStyle(sf::Text::Bold);
            widthLabel.setPosition(dialogPos.x + 20, dialogPos.y + 40);
            widthLabel.setFillColor(sf::Color(EightBitColors::White.r, EightBitColors::White.g, EightBitColors::White.b));
            window.draw(widthLabel);

            sf::RectangleShape widthInput({80, 25});
            widthInput.setPosition(dialogPos.x + 80, dialogPos.y + 40);
            widthInput.setFillColor(sf::Color(EightBitColors::Black.r, EightBitColors::Black.g, EightBitColors::Black.b));
            widthInput.setOutlineColor(widthInputActive ? sf::Color(EightBitColors::Yellow.r, EightBitColors::Yellow.g, EightBitColors::Yellow.b) : sf::Color(EightBitColors::White.r, EightBitColors::White.g, EightBitColors::White.b));
            widthInput.setOutlineThickness(2);
            window.draw(widthInput);

            sf::Text widthText(newWidthStr, font, 14);
            widthText.setStyle(sf::Text::Bold);
            widthText.setPosition(dialogPos.x + 85, dialogPos.y + 45);
            widthText.setFillColor(sf::Color(EightBitColors::White.r, EightBitColors::White.g, EightBitColors::White.b));
            window.draw(widthText);

            // Height input
            sf::Text heightLabel("HEIGHT:", font, 14);
            heightLabel.setStyle(sf::Text::Bold);
            heightLabel.setPosition(dialogPos.x + 20, dialogPos.y + 75);
            heightLabel.setFillColor(sf::Color(EightBitColors::White.r, EightBitColors::White.g, EightBitColors::White.b));
            window.draw(heightLabel);

            sf::RectangleShape heightInput({80, 25});
            heightInput.setPosition(dialogPos.x + 80, dialogPos.y + 75);
            heightInput.setFillColor(sf::Color(EightBitColors::Black.r, EightBitColors::Black.g, EightBitColors::Black.b));
            heightInput.setOutlineColor(heightInputActive ? sf::Color(EightBitColors::Yellow.r, EightBitColors::Yellow.g, EightBitColors::Yellow.b) : sf::Color(EightBitColors::White.r, EightBitColors::White.g, EightBitColors::White.b));
            heightInput.setOutlineThickness(2);
            window.draw(heightInput);

            sf::Text heightText(newHeightStr, font, 14);
            heightText.setStyle(sf::Text::Bold);
            heightText.setPosition(dialogPos.x + 85, dialogPos.y + 80);
            heightText.setFillColor(sf::Color(EightBitColors::White.r, EightBitColors::White.g, EightBitColors::White.b));
            window.draw(heightText);

            // Apply button
            bool applyHovered = (mpos.x >= (int)(dialogPos.x + 170) && mpos.x <= (int)(dialogPos.x + 230) &&
                                 mpos.y >= (int)(dialogPos.y + 40) && mpos.y <= (int)(dialogPos.y + 65));
            drawButton(window, sf::FloatRect(dialogPos.x + 170, dialogPos.y + 40, 60, 25), font, "APPLY", false, applyHovered);

            // Cancel button
            bool cancelHovered = (mpos.x >= (int)(dialogPos.x + 170) && mpos.x <= (int)(dialogPos.x + 230) &&
                                  mpos.y >= (int)(dialogPos.y + 75) && mpos.y <= (int)(dialogPos.y + 100));
            drawButton(window, sf::FloatRect(dialogPos.x + 170, dialogPos.y + 75, 60, 25), font, "CANCEL", false, cancelHovered);

            // Handle resize dialog clicks
            if (leftMouseDown && !uiElementClicked)
            {
                sf::Vector2i mm = sf::Mouse::getPosition(window);

                // Check width input click
                sf::FloatRect widthInputRect(dialogPos.x + 80, dialogPos.y + 40, 80, 25);
                if (widthInputRect.contains(mm.x, mm.y))
                {
                    widthInputActive = true;
                    heightInputActive = false;
                    uiElementClicked = true;
                }

                // Check height input click
                sf::FloatRect heightInputRect(dialogPos.x + 80, dialogPos.y + 75, 80, 25);
                if (heightInputRect.contains(mm.x, mm.y))
                {
                    heightInputActive = true;
                    widthInputActive = false;
                    uiElementClicked = true;
                }

                // Apply button
                if (applyHovered)
                {
                    try
                    {
                        unsigned newWidth = std::stoi(newWidthStr);
                        unsigned newHeight = std::stoi(newHeightStr);
                        if (newWidth > 0 && newWidth < 1024 && newHeight > 0 && newHeight < 1024)
                        {
                            canvas.resizeCanvas(newWidth, newHeight);
                            showResizeDialog = false;
                            widthInputActive = false;
                            heightInputActive = false;
                        }
                    }
                    catch (...)
                    {
                        // Invalid input, ignore or show error
                        std::cout << "Invalid input for resize!\n";
                    }
                    uiElementClicked = true;
                }

                // Cancel button
                if (cancelHovered)
                {
                    showResizeDialog = false;
                    widthInputActive = false;
                    heightInputActive = false;
                    uiElementClicked = true;
                }

                // Click outside dialog - close it
                sf::FloatRect dialogRect(dialogPos.x, dialogPos.y, dialogSize.x, dialogSize.y);
                if (!dialogRect.contains(mm.x, mm.y))
                {
                    showResizeDialog = false;
                    widthInputActive = false;
                    heightInputActive = false;
                    uiElementClicked = true;
                }
            }
        }
        // Small status text with 8-bit style
        std::string toolName = canvas.currentTool == Tool::Pencil ? "PENCIL" : canvas.currentTool == Tool::Eraser ? "ERASER"
                                                                                                                  : "FILL";
        sf::Text status("TOOL: " + toolName + "  FRAME: " + std::to_string(canvas.currentFrame) +
                            "  ZOOM: " + std::to_string((int)canvas.zoom) + "x",
                        font, 12);
        status.setStyle(sf::Text::Bold);
        status.setPosition(8, winSize.y - 22);
        status.setFillColor(sf::Color(EightBitColors::Yellow.r, EightBitColors::Yellow.g, EightBitColors::Yellow.b));
        window.draw(status);

        window.display();
    } // main loop

    return 0;
}