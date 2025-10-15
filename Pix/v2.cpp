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

using u8 = sf::Uint8;
using u32 = uint32_t;

struct Color
{
    u8 r = 0, g = 0, b = 0, a = 255;
    Color() {}
    Color(u8 rr, u8 gg, u8 bb, u8 aa = 255) : r(rr), g(gg), b(bb), a(aa) {}
};

struct Frame
{
    std::string name = "Frame";
    sf::Image image;
    Frame() {}
    Frame(unsigned w, unsigned h, const std::string &n = "Frame") : name(n)
    {
        image.create(w, h, sf::Color(0, 0, 0, 0));
    }
    void clear()
    {
        auto s = image.getSize();
        image.create(s.x, s.y, sf::Color(0, 0, 0, 0));
    }
    sf::Color getPixel(unsigned x, unsigned y) const { return image.getPixel(x, y); }
    void setPixel(unsigned x, unsigned y, const sf::Color &c) { image.setPixel(x, y, c); }
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
    Color drawColor{0, 0, 0, 255};
    Tool currentTool = Tool::Pencil;
    bool showGrid = true;
    bool onionSkin = false;

    Canvas(unsigned w = 64, unsigned h = 64) : width(w), height(h)
    {
        frames.emplace_back(w, h);
    }

    void resizeCanvas(unsigned newWidth, unsigned newHeight)
    {
        for (auto &frame : frames)
        {
            sf::Image newImage;
            newImage.create(newWidth, newHeight, sf::Color(0, 0, 0, 0));

            // Copy existing pixels
            auto oldSize = frame.image.getSize();
            for (unsigned y = 0; y < std::min(oldSize.y, newHeight); ++y)
            {
                for (unsigned x = 0; x < std::min(oldSize.x, newWidth); ++x)
                {
                    newImage.setPixel(x, y, frame.image.getPixel(x, y));
                }
            }

            frame.image = newImage;
        }
        width = newWidth;
        height = newHeight;
    }

    void newProject(unsigned w, unsigned h)
    {
        width = w;
        height = h;
        frames.clear();
        frames.emplace_back(w, h);
        currentFrame = 0;
        zoom = 8.0f;
        pan = {0, 0};
    }

    void addFrame()
    {
        frames.emplace_back(width, height);
        currentFrame = (int)frames.size() - 1;
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

        // Background
        sf::RectangleShape bg(size);
        bg.setPosition(position);
        bg.setFillColor(sf::Color(60, 60, 60));
        bg.setOutlineColor(sf::Color::White);
        bg.setOutlineThickness(2);
        window.draw(bg);

        // Color spectrum (simplified - hue gradient)
        sf::RectangleShape spectrum({size.x - 20, size.y - 80});
        spectrum.setPosition(position.x + 10, position.y + 10);

        // Create a gradient texture
        sf::Texture texture;
        texture.create(spectrum.getSize().x, spectrum.getSize().y);
        sf::Image gradientImage;
        gradientImage.create(spectrum.getSize().x, spectrum.getSize().y);

        for (unsigned y = 0; y < gradientImage.getSize().y; ++y)
        {
            for (unsigned x = 0; x < gradientImage.getSize().x; ++x)
            {
                float hue = (float)x / gradientImage.getSize().x;
                float sat = 1.0f - (float)y / gradientImage.getSize().y;

                // Simple HSV to RGB conversion
                float r, g, b;
                int i = int(hue * 6);
                float f = hue * 6 - i;
                float p = 0;
                float q = 1 - sat;
                float t = sat * f;

                switch (i % 6)
                {
                case 0:
                    r = 1;
                    g = t;
                    b = p;
                    break;
                case 1:
                    r = q;
                    g = 1;
                    b = p;
                    break;
                case 2:
                    r = p;
                    g = 1;
                    b = t;
                    break;
                case 3:
                    r = p;
                    g = q;
                    b = 1;
                    break;
                case 4:
                    r = t;
                    g = p;
                    b = 1;
                    break;
                case 5:
                    r = 1;
                    g = p;
                    b = q;
                    break;
                }

                gradientImage.setPixel(x, y, sf::Color(r * 255, g * 255, b * 255));
            }
        }

        texture.update(gradientImage);
        spectrum.setTexture(&texture);
        window.draw(spectrum);

        // Current color preview
        sf::RectangleShape preview({40, 40});
        preview.setPosition(position.x + size.x - 50, position.y + size.y - 50);
        preview.setFillColor(sf::Color(currentColor.r, currentColor.g, currentColor.b));
        preview.setOutlineColor(sf::Color::White);
        preview.setOutlineThickness(1);
        window.draw(preview);

        // Close button
        sf::RectangleShape closeBtn({60, 25});
        closeBtn.setPosition(position.x + size.x - 70, position.y + size.y - 25);
        closeBtn.setFillColor(sf::Color(100, 100, 100));
        window.draw(closeBtn);

        sf::Text closeText("Close", font, 12);
        closeText.setPosition(closeBtn.getPosition().x + 10, closeBtn.getPosition().y + 5);
        closeText.setFillColor(sf::Color::White);
        window.draw(closeText);
    }

    bool handleClick(sf::Vector2i mousePos, Color &targetColor)
    {
        if (!isOpen)
            return false;

        sf::FloatRect colorArea(position.x + 10, position.y + 10, size.x - 20, size.y - 80);
        sf::FloatRect closeBtn(position.x + size.x - 70, position.y + size.y - 25, 60, 25);

        if (closeBtn.contains(mousePos.x, mousePos.y))
        {
            isOpen = false;
            return true;
        }

        if (colorArea.contains(mousePos.x, mousePos.y))
        {
            float relX = (mousePos.x - colorArea.left) / colorArea.width;
            float relY = (mousePos.y - colorArea.top) / colorArea.height;

            // Simplified color picking from gradient
            float hue = relX;
            float sat = 1.0f - relY;

            // HSV to RGB
            float r, g, b;
            int i = int(hue * 6);
            float f = hue * 6 - i;
            float p = 0;
            float q = 1 - sat;
            float t = sat * f;

            switch (i % 6)
            {
            case 0:
                r = 1;
                g = t;
                b = p;
                break;
            case 1:
                r = q;
                g = 1;
                b = p;
                break;
            case 2:
                r = p;
                g = 1;
                b = t;
                break;
            case 3:
                r = p;
                g = q;
                b = 1;
                break;
            case 4:
                r = t;
                g = p;
                b = 1;
                break;
            case 5:
                r = 1;
                g = p;
                b = q;
                break;
            }

            currentColor = Color(r * 255, g * 255, b * 255);
            targetColor = currentColor;
            return true;
        }

        return false;
    }
};

// Utility: draw a rectangle button with label
void drawButton(sf::RenderWindow &w, const sf::FloatRect &rect, const sf::Font &font, const std::string &label, const sf::Color &bg = sf::Color(80, 80, 80))
{
    sf::RectangleShape rs({rect.width, rect.height});
    rs.setPosition(rect.left, rect.top);
    rs.setFillColor(bg);
    rs.setOutlineColor(sf::Color::Black);
    rs.setOutlineThickness(1);
    w.draw(rs);

    // Center the text in the button
    sf::Text t(label, font, 12);
    sf::FloatRect textBounds = t.getLocalBounds();
    t.setPosition(rect.left + (rect.width - textBounds.width) / 2, rect.top + (rect.height - textBounds.height) / 2 - 2);
    t.setFillColor(sf::Color::White);
    w.draw(t);
}

int main()
{
    // Basic parameters
    unsigned initW = 64, initH = 64;
    Canvas canvas(initW, initH);

    // Window & view setup
    sf::RenderWindow window(sf::VideoMode(1100, 700), "Pix - simple pixel editor (single-file)", sf::Style::Close | sf::Style::Titlebar);
    window.setVerticalSyncEnabled(true);

    // Load a font - try multiple possible locations
    sf::Font font;
    if (!font.loadFromFile("fonts/ARIAL.TTF"))
    {
        std::cerr << "Failed to load font! Buttons will not have text labels.\n";
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

    // Track button clicks to avoid multiple triggers
    bool buttonClicked = false;

    sf::Clock clock;
    while (running)
    {
        float dt = clock.restart().asSeconds();
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
                    buttonClicked = true; // New click detected

                    // Handle color picker click
                    sf::Vector2i mousePos = sf::Mouse::getPosition(window);
                    colorPicker.handleClick(mousePos, canvas.drawColor);
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
                    buttonClicked = false; // Reset click tracking
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
                }
            }
            else if (ev.type == sf::Event::TextEntered && showResizeDialog)
            {
                if (ev.text.unicode == '\b') // Backspace
                {
                    if (!newWidthStr.empty())
                        newWidthStr.pop_back();
                }
                else if (ev.text.unicode >= '0' && ev.text.unicode <= '9')
                {
                    newWidthStr += static_cast<char>(ev.text.unicode);
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
        if (middleMouseDown || sf::Keyboard::isKeyPressed(sf::Keyboard::Space))
        {
            if (sf::Mouse::isButtonPressed(sf::Mouse::Left) || middleMouseDown)
            {
                sf::Vector2i cur = mpos;
                sf::Vector2f diff((float)(cur.x - lastMouse.x), (float)(cur.y - lastMouse.y));
                canvas.pan += diff;
                lastMouse = cur;
            }
        }

        // Drawing: convert mouse to pixel coords
        if (leftMouseDown && mouseInCanvas && !buttonClicked && !colorPicker.isOpen)
        {
            // map mouse to canvas pixel position
            float localX = (mpos.x - canvasArea.left - canvas.pan.x) / canvas.zoom;
            float localY = (mpos.y - canvasArea.top - canvas.pan.y) / canvas.zoom;
            int px = (int)std::floor(localX), py = (int)std::floor(localY);
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

        // --- Rendering ---
        window.clear(sf::Color(50, 50, 50));

        // Draw toolbar background
        sf::RectangleShape tbBg({(float)winSize.x - sidebarW - 8, toolbarH - 4});
        tbBg.setPosition(4, 4);
        tbBg.setFillColor(sf::Color(60, 60, 60));
        window.draw(tbBg);

        // Toolbar buttons: Pencil, Eraser, Fill
        float x = 8;
        float y = 8;
        float bw = 64, bh = 32, spacing = 6;

        // Pencil button
        sf::Color pencilColor = (canvas.currentTool == Tool::Pencil) ? sf::Color(100, 100, 100) : sf::Color(80, 80, 80);
        drawButton(window, sf::FloatRect(x, y, bw, bh), font, "Pencil", pencilColor);
        if (buttonClicked && sf::Mouse::getPosition(window).x >= (int)x && sf::Mouse::getPosition(window).x <= (int)(x + bw) &&
            sf::Mouse::getPosition(window).y >= (int)y && sf::Mouse::getPosition(window).y <= (int)(y + bh))
        {
            canvas.currentTool = Tool::Pencil;
        }

        x += bw + spacing;

        // Eraser button
        sf::Color eraserColor = (canvas.currentTool == Tool::Eraser) ? sf::Color(100, 100, 100) : sf::Color(80, 80, 80);
        drawButton(window, sf::FloatRect(x, y, bw, bh), font, "Eraser", eraserColor);
        if (buttonClicked && sf::Mouse::getPosition(window).x >= (int)x && sf::Mouse::getPosition(window).x <= (int)(x + bw) &&
            sf::Mouse::getPosition(window).y >= (int)y && sf::Mouse::getPosition(window).y <= (int)(y + bh))
        {
            canvas.currentTool = Tool::Eraser;
        }

        x += bw + spacing;

        // Fill button
        sf::Color fillColor = (canvas.currentTool == Tool::Fill) ? sf::Color(100, 100, 100) : sf::Color(80, 80, 80);
        drawButton(window, sf::FloatRect(x, y, bw, bh), font, "Fill", fillColor);
        if (buttonClicked && sf::Mouse::getPosition(window).x >= (int)x && sf::Mouse::getPosition(window).x <= (int)(x + bw) &&
            sf::Mouse::getPosition(window).y >= (int)y && sf::Mouse::getPosition(window).y <= (int)(y + bh))
        {
            canvas.currentTool = Tool::Fill;
        }

        // Color preview / click to open picker
        float colorX = x + bw + spacing;
        sf::RectangleShape colPreview({36, 36});
        colPreview.setPosition(colorX, y - 2);
        colPreview.setFillColor(sf::Color(canvas.drawColor.r, canvas.drawColor.g, canvas.drawColor.b, 255));
        colPreview.setOutlineColor(sf::Color::Black);
        colPreview.setOutlineThickness(1);
        window.draw(colPreview);

        // Color picker button
        drawButton(window, sf::FloatRect(colorX + 40, y, 60, bh), font, "Color");
        if (buttonClicked && sf::Mouse::getPosition(window).x >= (int)(colorX + 40) && sf::Mouse::getPosition(window).x <= (int)(colorX + 40 + 60) &&
            sf::Mouse::getPosition(window).y >= (int)y && sf::Mouse::getPosition(window).y <= (int)(y + bh))
        {
            colorPicker.isOpen = !colorPicker.isOpen;
        }

        // Resize canvas button
        drawButton(window, sf::FloatRect(colorX + 110, y, 80, bh), font, "Resize");
        if (buttonClicked && sf::Mouse::getPosition(window).x >= (int)(colorX + 110) && sf::Mouse::getPosition(window).x <= (int)(colorX + 110 + 80) &&
            sf::Mouse::getPosition(window).y >= (int)y && sf::Mouse::getPosition(window).y <= (int)(y + bh))
        {
            showResizeDialog = !showResizeDialog;
            newWidthStr = std::to_string(canvas.width);
            newHeightStr = std::to_string(canvas.height);
        }

        // Draw canvas background (checkerboard)
        sf::RectangleShape canvasBg({canvasArea.width, canvasArea.height});
        canvasBg.setPosition(canvasArea.left, canvasArea.top);
        canvasBg.setFillColor(sf::Color(100, 100, 100));
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

        // Optionally draw grid lines
        if (canvas.showGrid)
        {
            sf::VertexArray lines(sf::Lines);
            for (unsigned xg = 0; xg <= canvas.width; ++xg)
            {
                float sxp = canvasArea.left + canvas.pan.x + xg * canvas.zoom;
                lines.append(sf::Vertex({sxp, canvasArea.top + canvas.pan.y}, sf::Color(0, 0, 0, 80)));
                lines.append(sf::Vertex({sxp, canvasArea.top + canvas.pan.y + canvas.height * canvas.zoom}, sf::Color(0, 0, 0, 80)));
            }
            for (unsigned yg = 0; yg <= canvas.height; ++yg)
            {
                float syp = canvasArea.top + canvas.pan.y + yg * canvas.zoom;
                lines.append(sf::Vertex({canvasArea.left + canvas.pan.x, syp}, sf::Color(0, 0, 0, 80)));
                lines.append(sf::Vertex({canvasArea.left + canvas.pan.x + canvas.width * canvas.zoom, syp}, sf::Color(0, 0, 0, 80)));
            }
            window.draw(lines);
        }

        // Sidebar area (frames)
        sf::FloatRect sidebar(canvasArea.left + canvasArea.width + 8, 4, sidebarW - 8, (float)winSize.y - 8);
        sf::RectangleShape sbBg({sidebar.width, sidebar.height});
        sbBg.setPosition(sidebar.left, sidebar.top);
        sbBg.setFillColor(sf::Color(55, 55, 55));
        window.draw(sbBg);

        // Frame list
        float fy = sidebar.top + 8;
        sf::Text ft("Frames", font, 14);
        ft.setPosition(sidebar.left + 8, fy);
        ft.setFillColor(sf::Color::White);
        window.draw(ft);
        fy += 20;

        float itemH = 28;
        for (size_t i = 0; i < canvas.frames.size(); ++i)
        {
            sf::FloatRect r(sidebar.left + 8, fy, sidebar.width - 32, itemH - 4);
            sf::RectangleShape item({r.width, r.height});
            item.setPosition(r.left, r.top);
            item.setFillColor(i == canvas.currentFrame ? sf::Color(90, 90, 90) : sf::Color(80, 80, 80));
            window.draw(item);
            sf::Text t("Frame " + std::to_string(i), font, 13);
            t.setPosition(r.left + 6, r.top + 4);
            t.setFillColor(sf::Color::White);
            window.draw(t);
            if (buttonClicked)
            {
                sf::Vector2i mm = sf::Mouse::getPosition(window);
                if (mm.x >= (int)r.left && mm.x <= (int)(r.left + r.width) && mm.y >= (int)r.top && mm.y <= (int)(r.top + r.height))
                {
                    canvas.currentFrame = (int)i;
                }
            }
            fy += itemH;
        }
        // Frame buttons
        drawButton(window, sf::FloatRect(sidebar.left + 8, fy + 8, 80, 28), font, "+ Frame");
        if (buttonClicked)
        {
            sf::Vector2i mm = sf::Mouse::getPosition(window);
            if (mm.x >= (int)(sidebar.left + 8) && mm.x <= (int)(sidebar.left + 8 + 80) && mm.y >= (int)(fy + 8) && mm.y <= (int)(fy + 8 + 28))
            {
                canvas.addFrame();
            }
        }
        drawButton(window, sf::FloatRect(sidebar.left + 96, fy + 8, 80, 28), font, "Export");
        if (buttonClicked)
        {
            sf::Vector2i mm = sf::Mouse::getPosition(window);
            if (mm.x >= (int)(sidebar.left + 96) && mm.x <= (int)(sidebar.left + 176) && mm.y >= (int)(fy + 8) && mm.y <= (int)(fy + 8 + 28))
            {
                canvas.exportCurrentFramePNG("export/frame.png");
                std::cout << "Exported current frame to export/frame.png\n";
            }
        }

        // Draw color picker if open
        colorPicker.draw(window, font);

        // Draw resize dialog if open
        if (showResizeDialog)
        {
            sf::Vector2f dialogSize(250, 120);
            sf::Vector2f dialogPos(winSize.x / 2 - dialogSize.x / 2, winSize.y / 2 - dialogSize.y / 2);

            // Background
            sf::RectangleShape dialogBg(dialogSize);
            dialogBg.setPosition(dialogPos);
            dialogBg.setFillColor(sf::Color(70, 70, 70));
            dialogBg.setOutlineColor(sf::Color::White);
            dialogBg.setOutlineThickness(2);
            window.draw(dialogBg);

            // Title
            sf::Text title("Resize Canvas", font, 16);
            title.setPosition(dialogPos.x + 10, dialogPos.y + 10);
            title.setFillColor(sf::Color::White);
            window.draw(title);

            // Width input
            sf::Text widthLabel("Width:", font, 14);
            widthLabel.setPosition(dialogPos.x + 20, dialogPos.y + 40);
            widthLabel.setFillColor(sf::Color::White);
            window.draw(widthLabel);

            sf::RectangleShape widthInput({80, 20});
            widthInput.setPosition(dialogPos.x + 80, dialogPos.y + 40);
            widthInput.setFillColor(sf::Color::Black);
            widthInput.setOutlineColor(sf::Color::White);
            widthInput.setOutlineThickness(1);
            window.draw(widthInput);

            sf::Text widthText(newWidthStr, font, 14);
            widthText.setPosition(dialogPos.x + 85, dialogPos.y + 42);
            widthText.setFillColor(sf::Color::White);
            window.draw(widthText);

            // Height input
            sf::Text heightLabel("Height:", font, 14);
            heightLabel.setPosition(dialogPos.x + 20, dialogPos.y + 70);
            heightLabel.setFillColor(sf::Color::White);
            window.draw(heightLabel);

            sf::RectangleShape heightInput({80, 20});
            heightInput.setPosition(dialogPos.x + 80, dialogPos.y + 70);
            heightInput.setFillColor(sf::Color::Black);
            heightInput.setOutlineColor(sf::Color::White);
            heightInput.setOutlineThickness(1);
            window.draw(heightInput);

            sf::Text heightText(newHeightStr, font, 14);
            heightText.setPosition(dialogPos.x + 85, dialogPos.y + 72);
            heightText.setFillColor(sf::Color::White);
            window.draw(heightText);

            // Apply button
            drawButton(window, sf::FloatRect(dialogPos.x + 170, dialogPos.y + 40, 60, 25), font, "Apply");
            if (buttonClicked)
            {
                sf::Vector2i mm = sf::Mouse::getPosition(window);
                if (mm.x >= (int)(dialogPos.x + 170) && mm.x <= (int)(dialogPos.x + 230) &&
                    mm.y >= (int)(dialogPos.y + 40) && mm.y <= (int)(dialogPos.y + 65))
                {
                    try
                    {
                        unsigned newWidth = std::stoi(newWidthStr);
                        unsigned newHeight = std::stoi(newHeightStr);
                        if (newWidth > 0 && newWidth < 1024 && newHeight > 0 && newHeight < 1024)
                        {
                            canvas.resizeCanvas(newWidth, newHeight);
                            showResizeDialog = false;
                        }
                    }
                    catch (...)
                    {
                        // Invalid input, ignore
                    }
                }
            }

            // Cancel button
            drawButton(window, sf::FloatRect(dialogPos.x + 170, dialogPos.y + 70, 60, 25), font, "Cancel");
            if (buttonClicked)
            {
                sf::Vector2i mm = sf::Mouse::getPosition(window);
                if (mm.x >= (int)(dialogPos.x + 170) && mm.x <= (int)(dialogPos.x + 230) &&
                    mm.y >= (int)(dialogPos.y + 70) && mm.y <= (int)(dialogPos.y + 95))
                {
                    showResizeDialog = false;
                }
            }
        }

        // Small status text
        sf::Text status("Tool: " + std::string(canvas.currentTool == Tool::Pencil ? "Pencil" : canvas.currentTool == Tool::Eraser ? "Eraser"
                                                                                                                                  : "Fill") +
                            "  Frame: " + std::to_string(canvas.currentFrame) + "  Zoom: " + std::to_string((int)canvas.zoom) + "x",
                        font, 12);
        status.setPosition(8, winSize.y - 22);
        status.setFillColor(sf::Color::White);
        window.draw(status);

        window.display();
    } // main loop

    return 0;
}