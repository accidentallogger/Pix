// main.cpp

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
#include <memory>
#include <thread>
#include <atomic>
#include <sys/stat.h> // For directory creation

using u8 = sf::Uint8;
using u32 = uint32_t;

// Define Color struct FIRST
struct Color
{
    u8 r = 0, g = 0, b = 0, a = 255;
    Color() {}
    Color(u8 rr, u8 gg, u8 bb, u8 aa = 255) : r(rr), g(gg), b(bb), a(aa) {}

    bool operator==(const Color &other) const
    {
        return r == other.r && g == other.g && b == other.b && a == other.a;
    }

    bool operator!=(const Color &other) const
    {
        return !(*this == other);
    }
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

// Simple GIF encoder (minimal implementation)
class SimpleGIFEncoder
{
private:
    std::ofstream file;
    int width, height;
    int delay; // in hundredths of a second

public:
    SimpleGIFEncoder(const std::string &filename, int w, int h, int frameDelay = 5)
        : width(w), height(h), delay(frameDelay)
    {
        file.open(filename, std::ios::binary);
        if (!file)
            return;

        // Write GIF header
        file << "GIF89a";

        // Write Logical Screen Descriptor
        file.put(width & 0xFF);
        file.put((width >> 8) & 0xFF);
        file.put(height & 0xFF);
        file.put((height >> 8) & 0xFF);
        file.put(0xF7); // Packed fields: global color table, 256 colors, 8 bits per pixel
        file.put(0x00); // Background color index
        file.put(0x00); // Pixel aspect ratio

        // Write Global Color Table (simplified - using 8-bit palette)
        for (int i = 0; i < 16; ++i)
        {
            file.put(EightBitColors::Palette[i].r);
            file.put(EightBitColors::Palette[i].g);
            file.put(EightBitColors::Palette[i].b);
        }
        // Fill remaining with black
        for (int i = 16; i < 256; ++i)
        {
            file.put(0);
            file.put(0);
            file.put(0);
        }

        // Write Application Extension (for animation)
        file.put(0x21); // Extension introducer
        file.put(0xFF); // Application extension label
        file.put(0x0B); // Block size
        file.write("NETSCAPE2.0", 11);
        file.put(0x03); // Sub-block size
        file.put(0x01); // Loop sub-block ID
        file.put(0x00); // Loop count (0 = infinite)
        file.put(0x00); // Loop count
        file.put(0x00); // Block terminator
    }

    ~SimpleGIFEncoder()
    {
        if (file)
        {
            file.put(0x3B); // GIF trailer
            file.close();
        }
    }

    bool addFrame(const sf::Image &image)
    {
        if (!file)
            return false;

        // Write Graphics Control Extension
        file.put(0x21);                // Extension introducer
        file.put(0xF9);                // Graphic control label
        file.put(0x04);                // Block size
        file.put(0x04);                // Disposal method, no user input
        file.put(delay & 0xFF);        // Delay time low byte
        file.put((delay >> 8) & 0xFF); // Delay time high byte
        file.put(0x00);                // Transparent color index (none)
        file.put(0x00);                // Block terminator

        // Write Image Descriptor
        file.put(0x2C); // Image separator
        file.put(0x00); // Image left low byte
        file.put(0x00); // Image left high byte
        file.put(0x00); // Image top low byte
        file.put(0x00); // Image top high byte
        file.put(width & 0xFF);
        file.put((width >> 8) & 0xFF);
        file.put(height & 0xFF);
        file.put((height >> 8) & 0xFF);
        file.put(0x00); // No local color table, not interlaced

        // Write image data (simplified - using LZW compression would be better)
        // For simplicity, we'll use uncompressed data
        file.put(0x08); // LZW minimum code size
        file.put(0xFF); // First data sub-block size

        for (int y = 0; y < height; ++y)
        {
            for (int x = 0; x < width; ++x)
            {
                sf::Color pixel = image.getPixel(x, y);
                if (pixel.a == 0)
                {
                    file.put(0); // Transparent -> black
                }
                else
                {
                    // Find closest 8-bit color
                    int bestIndex = 0;
                    int bestDist = 255 * 255 * 3;
                    for (int i = 0; i < 16; ++i)
                    {
                        const Color &paletteColor = EightBitColors::Palette[i];
                        int dist = (pixel.r - paletteColor.r) * (pixel.r - paletteColor.r) +
                                   (pixel.g - paletteColor.g) * (pixel.g - paletteColor.g) +
                                   (pixel.b - paletteColor.b) * (pixel.b - paletteColor.b);
                        if (dist < bestDist)
                        {
                            bestDist = dist;
                            bestIndex = i;
                        }
                    }
                    file.put(bestIndex);
                }
            }
        }

        file.put(0x00); // Block terminator
        return true;
    }
};

struct Frame
{
    std::string name = "Frame";
    sf::Image image;
    sf::Texture thumbnail;

    Frame() {}
    Frame(unsigned w, unsigned h, const std::string &n = "Frame") : name(n)
    {
        // SFML's create returns void, not bool - just call it directly
        image.create(w, h, sf::Color(0, 0, 0, 0));
        updateThumbnail();
    }

    // Rule of Five: Add proper copy/move semantics to prevent memory issues
    Frame(const Frame &other) : name(other.name)
    {
        if (other.image.getSize().x > 0 && other.image.getSize().y > 0)
        {
            image.create(other.image.getSize().x, other.image.getSize().y);
            image.copy(other.image, 0, 0);
        }
        updateThumbnail();
    }

    Frame &operator=(const Frame &other)
    {
        if (this != &other)
        {
            name = other.name;
            if (other.image.getSize().x > 0 && other.image.getSize().y > 0)
            {
                image.create(other.image.getSize().x, other.image.getSize().y);
                image.copy(other.image, 0, 0);
            }
            updateThumbnail();
        }
        return *this;
    }

    void clear()
    {
        auto s = image.getSize();
        image.create(s.x, s.y, sf::Color(0, 0, 0, 0));
        updateThumbnail();
    }

    sf::Color getPixel(unsigned x, unsigned y) const
    {
        if (x < image.getSize().x && y < image.getSize().y)
        {
            return image.getPixel(x, y);
        }
        return sf::Color::Transparent;
    }

    void setPixel(unsigned x, unsigned y, const sf::Color &c)
    {
        if (x < image.getSize().x && y < image.getSize().y)
        {
            image.setPixel(x, y, c);
            updateThumbnail();
        }
    }

    void updateThumbnail()
    {
        // Create a thumbnail (scaled down version)
        unsigned thumbSize = 48;

        // Check if image is valid
        auto size = image.getSize();
        if (size.x == 0 || size.y == 0)
        {
            return;
        }

        sf::Image thumbImg;
        thumbImg.create(thumbSize, thumbSize, sf::Color(0, 0, 0, 0));

        for (unsigned y = 0; y < thumbSize; ++y)
        {
            for (unsigned x = 0; x < thumbSize; ++x)
            {
                unsigned srcX = (x * size.x) / thumbSize;
                unsigned srcY = (y * size.y) / thumbSize;
                if (srcX < size.x && srcY < size.y)
                {
                    thumbImg.setPixel(x, y, image.getPixel(srcX, srcY));
                }
            }
        }

        // Just try to load the thumbnail, don't check return value
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
    int eraserSize = 1; // New: Eraser size management

    Canvas(unsigned w = 64, unsigned h = 64) : width(w), height(h)
    {
        // Add bounds checking
        if (w == 0 || h == 0 || w > 2048 || h > 2048)
        {
            std::cerr << "Invalid canvas dimensions: " << w << "x" << h << std::endl;
            width = 64;
            height = 64;
        }
        frames.emplace_back(width, height, "Frame 0");
    }

    void resizeCanvas(unsigned newWidth, unsigned newHeight)
    {
        // Add bounds checking
        if (newWidth == 0 || newHeight == 0 || newWidth > 2048 || newHeight > 2048)
        {
            std::cerr << "Invalid resize dimensions: " << newWidth << "x" << newHeight << std::endl;
            return;
        }

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
        if (w == 0 || h == 0 || w > 2048 || h > 2048)
        {
            std::cerr << "Invalid project dimensions: " << w << "x" << h << std::endl;
            w = 64;
            h = 64;
        }

        width = w;
        height = h;
        frames.clear();
        frames.emplace_back(w, h, "Frame 0");
        currentFrame = 0;
        zoom = 8.0f;
        pan = {0, 0};
        eraserSize = 1; // Reset eraser size
    }

    void addFrame()
    {
        // Limit maximum frames to prevent memory issues
        if (frames.size() >= 1000)
        {
            std::cerr << "Maximum frame limit reached!" << std::endl;
            return;
        }
        frames.emplace_back(width, height, "Frame " + std::to_string(frames.size()));
        currentFrame = (int)frames.size() - 1;
    }

    void duplicateFrame()
    {
        if (frames.empty())
            return;

        // Limit maximum frames
        if (frames.size() >= 1000)
        {
            std::cerr << "Maximum frame limit reached!" << std::endl;
            return;
        }

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
        if (currentFrame > 0 && currentFrame < (int)frames.size())
        {
            // Swap the current frame with the one above it
            std::swap(frames[currentFrame], frames[currentFrame - 1]);
            // Update currentFrame to follow the frame that was moved
            currentFrame--;
        }
    }

    void moveFrameDown()
    {
        if (currentFrame >= 0 && currentFrame < (int)frames.size() - 1)
        {
            // Swap the current frame with the one below it
            std::swap(frames[currentFrame], frames[currentFrame + 1]);
            // Update currentFrame to follow the frame that was moved
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
        if (frames.empty() || currentFrame < 0 || currentFrame >= (int)frames.size())
        {
            return sf::Image();
        }
        return frames[currentFrame].image;
    }

    void setPixelAtCurrentFrame(int x, int y, const sf::Color &c)
    {
        if (x < 0 || y < 0 || x >= (int)width || y >= (int)height || frames.empty())
            return;
        Frame &f = frames[currentFrame];
        f.setPixel(x, y, c);
    }

    // New: Set multiple pixels for eraser size
    void setPixelsAtCurrentFrame(int centerX, int centerY, const sf::Color &c, int size)
    {
        if (frames.empty())
            return;

        Frame &f = frames[currentFrame];
        int halfSize = size / 2;

        for (int y = centerY - halfSize; y <= centerY + halfSize; ++y)
        {
            for (int x = centerX - halfSize; x <= centerX + halfSize; ++x)
            {
                if (x >= 0 && y >= 0 && x < (int)width && y < (int)height)
                {
                    f.setPixel(x, y, c);
                }
            }
        }
    }

    void floodFill(int sx, int sy, const sf::Color &newColor)
    {
        if (sx < 0 || sy < 0 || sx >= (int)width || sy >= (int)height || frames.empty())
            return;
        Frame &f = frames[currentFrame];
        sf::Color target = f.getPixel(sx, sy);
        if (target == newColor)
            return;

        // Use iterative flood fill with bounds checking
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

    // NEW: Export as GIF
    bool exportAsGIF(const std::string &filename, int frameDelay = 5) const
    {
        if (frames.empty())
            return false;

        try
        {
            SimpleGIFEncoder gif(filename, width, height, frameDelay);
            for (const auto &frame : frames)
            {
                if (!gif.addFrame(frame.image))
                {
                    return false;
                }
            }
            return true;
        }
        catch (...)
        {
            return false;
        }
    }

    // NEW: Create directory function (cross-platform)
    bool createDirectory(const std::string &path) const
    {
#ifdef _WIN32
        return _mkdir(path.c_str()) == 0 || errno == EEXIST;
#else
        return mkdir(path.c_str(), 0755) == 0 || errno == EEXIST;
#endif
    }

    // NEW: Export Godot resources
    bool exportForGodot(const std::string &basePath) const
    {
        if (frames.empty())
            return false;

        try
        {
            // Create directory if it doesn't exist
            std::string dir = basePath + "/godot_export/";

            // Use our custom directory creation function
            if (!createDirectory(dir))
            {
                std::cerr << "Failed to create directory: " << dir << std::endl;
                return false;
            }

            if (frames.size() == 1)
            {
                // Single frame - export as sprite
                return exportGodotSpriteSheet(dir + "sprite.png", dir + "sprite.tres");
            }
            else
            {
                // Multiple frames - export as sprite sheet and animation
                return exportGodotAnimation(dir + "spritesheet.png", dir + "spritesheet.tres", dir + "animation.tres");
            }
        }
        catch (const std::exception &e)
        {
            std::cerr << "Error exporting for Godot: " << e.what() << std::endl;
            return false;
        }
        catch (...)
        {
            std::cerr << "Unknown error exporting for Godot" << std::endl;
            return false;
        }
    }

private:
    bool exportGodotSpriteSheet(const std::string &imagePath, const std::string &resourcePath) const
    {
        // Export single frame as PNG
        if (!frames[0].image.saveToFile(imagePath))
            return false;

        // Create Godot SpriteFrames resource
        std::ofstream res(resourcePath);
        if (!res)
            return false;

        res << "[gd_resource type=\"SpriteFrames\" load_steps=2 format=2]\n\n";
        res << "[ext_resource path=\"res://" << imagePath.substr(imagePath.find_last_of("/\\") + 1) << "\" type=\"Texture\" id=1]\n\n";
        res << "[resource]\n";
        res << "animations = [ {\n";
        res << "\"frames\": [ ExtResource( 1 ) ],\n";
        res << "\"loop\": true,\n";
        res << "\"name\": \"default\",\n";
        res << "\"speed\": 5.0\n";
        res << "} ]\n";

        return true;
    }

    bool exportGodotAnimation(const std::string &imagePath, const std::string &spritePath, const std::string &animPath) const
    {
        // Create sprite sheet (all frames horizontally)
        int totalWidth = width * frames.size();
        sf::Image spriteSheet;
        spriteSheet.create(totalWidth, height, sf::Color(0, 0, 0, 0));

        for (size_t i = 0; i < frames.size(); ++i)
        {
            for (unsigned y = 0; y < height; ++y)
            {
                for (unsigned x = 0; x < width; ++x)
                {
                    spriteSheet.setPixel(x + i * width, y, frames[i].image.getPixel(x, y));
                }
            }
        }

        if (!spriteSheet.saveToFile(imagePath))
            return false;

        // Create SpriteFrames resource
        std::ofstream spriteRes(spritePath);
        if (!spriteRes)
            return false;

        spriteRes << "[gd_resource type=\"SpriteFrames\" load_steps=2 format=2]\n\n";
        spriteRes << "[ext_resource path=\"res://" << imagePath.substr(imagePath.find_last_of("/\\") + 1) << "\" type=\"Texture\" id=1]\n\n";
        spriteRes << "[resource]\n";
        spriteRes << "animations = [ {\n";
        spriteRes << "\"frames\": [";
        for (size_t i = 0; i < frames.size(); ++i)
        {
            spriteRes << " ExtResource( 1 )";
            if (i < frames.size() - 1)
                spriteRes << ",";
        }
        spriteRes << " ],\n";
        spriteRes << "\"loop\": true,\n";
        spriteRes << "\"name\": \"default\",\n";
        spriteRes << "\"speed\": 5.0\n";
        spriteRes << "} ]\n";

        // Create Animation resource
        std::ofstream animRes(animPath);
        if (!animRes)
            return false;

        animRes << "[gd_resource type=\"Animation\" load_steps=2 format=2]\n\n";
        animRes << "[ext_resource path=\"res://" << spritePath.substr(spritePath.find_last_of("/\\") + 1) << "\" type=\"SpriteFrames\" id=1]\n\n";
        animRes << "[resource]\n";
        animRes << "loop = true\n";
        animRes << "step = 0.1\n";
        animRes << "length = " << (frames.size() * 0.2) << "\n";
        animRes << "tracks/0 = \"value\"\n";
        animRes << "tracks/0/type = 0\n";
        animRes << "tracks/0/path = NodePath(\"Sprite:frame\")\n";
        animRes << "tracks/0/interp = 1\n";
        animRes << "tracks/0/loop_wrap = true\n";
        animRes << "tracks/0/imported = false\n";
        animRes << "tracks/0/enabled = true\n";
        animRes << "tracks/0/keys = {\n";
        animRes << "\"times\": PoolRealArray(";
        for (size_t i = 0; i < frames.size(); ++i)
        {
            animRes << (i * 0.2);
            if (i < frames.size() - 1)
                animRes << ", ";
        }
        animRes << "),\n";
        animRes << "\"transitions\": PoolRealArray(";
        for (size_t i = 0; i < frames.size(); ++i)
        {
            animRes << "1.0";
            if (i < frames.size() - 1)
                animRes << ", ";
        }
        animRes << "),\n";
        animRes << "\"update\": 0,\n";
        animRes << "\"values\": [ ";
        for (size_t i = 0; i < frames.size(); ++i)
        {
            animRes << i;
            if (i < frames.size() - 1)
                animRes << ", ";
        }
        animRes << " ]\n";
        animRes << "}\n";

        return true;
    }

public:
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

        // Validate dimensions
        if (w == 0 || h == 0 || w > 2048 || h > 2048 || fcount > 1000)
        {
            std::cerr << "Invalid file format or dimensions too large!" << std::endl;
            return false;
        }

        frames.clear();
        width = w;
        height = h;
        for (u32 fi = 0; fi < fcount; ++fi)
        {
            u32 nameLen = read32();
            if (nameLen > 1000)
            { // Sanity check
                std::cerr << "Invalid frame name length!" << std::endl;
                return false;
            }
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
        eraserSize = 1; // Reset eraser size
        return true;
    }

    // Export current frame or all frames as PNGs
    bool exportCurrentFramePNG(const std::string &filename) const
    {
        if (frames.empty() || currentFrame < 0 || currentFrame >= (int)frames.size())
        {
            return false;
        }
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

            if (colorRect.contains((float)mousePos.x, (float)mousePos.y))
            {
                currentColor = EightBitColors::Palette[i];
                targetColor = currentColor;
                return true;
            }
        }

        // Check close button
        sf::FloatRect closeBtn(position.x + size.x - 80, position.y + size.y - 25, 70, 25);
        if (closeBtn.contains((float)mousePos.x, (float)mousePos.y))
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
    // Basic parameters with error handling
    unsigned initW = 64, initH = 64;
    Canvas canvas(initW, initH);

    // Window & view setup
    sf::RenderWindow window(sf::VideoMode(1100, 700), "Pix", sf::Style::Close | sf::Style::Titlebar);
    window.setVerticalSyncEnabled(true); // Enable VSync to prevent tearing
    window.setFramerateLimit(60);        // Limit FPS to reduce CPU usage

    // Load a retro-style font
    sf::Font font;
    if (!font.loadFromFile("fonts/ARIAL.TTF"))
    {
        // Try to load a pixel font instead
        if (!font.loadFromFile("fonts/FFFFORWA.TTF"))
        { // Example pixel font name
            std::cerr << "Failed to load font! Buttons will not have text labels.\n";
            // Continue without font - the program will still work
        }
    }

    bool running = true;
    bool leftMouseDown = false, middleMouseDown = false;
    bool leftMousePressedThisFrame = false; // NEW: Track single press events
    sf::Vector2i lastMouse;

    // Animation preview
    bool playing = false;
    float fps = 6.0f;
    float playTimer = 0.f;

    // Color picker
    ColorPicker colorPicker;

    // Canvas resize dialog
    bool showResizeDialog = false;
    std::string newWidthStr = "64", newHeightStr = "64";
    bool widthInputActive = false;
    bool heightInputActive = false;

    // NEW: Eraser size dialog
    bool showEraserSizeDialog = false;
    std::string eraserSizeStr = "1";

    // NEW: Export dialogs
    bool showGIFExportDialog = false;
    bool showGodotExportDialog = false;
    std::string gifDelayStr = "5";

    // Track if we just clicked a UI element (to prevent drawing on UI clicks)
    bool uiElementClicked = false;

    // Track duplicate/delete clicks to prevent multiple calls
    bool duplicateClicked = false;
    bool deleteClicked = false;

    // Frame renaming
    bool renamingFrame = false;
    int frameToRename = -1;
    std::string frameNameInput;

    // Hover states for buttons
    int hoveredToolButton = -1;

    // NEW: Export status message
    std::string exportStatus = "";
    float exportStatusTimer = 0.0f;

    sf::Clock clock;

    try
    {
        while (running)
        {
            float dt = clock.restart().asSeconds();

            // Reset UI click tracking at start of frame
            uiElementClicked = false;
            hoveredToolButton = -1;
            leftMousePressedThisFrame = false; // Reset single press flag

            // Update export status timer
            if (exportStatusTimer > 0)
            {
                exportStatusTimer -= dt;
                if (exportStatusTimer <= 0)
                {
                    exportStatus = "";
                }
            }

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
                        leftMousePressedThisFrame = true; // NEW: Mark as pressed this frame
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
                        if (canvas.saveToPix("project.pix"))
                        {
                            exportStatus = "Saved project.pix";
                            exportStatusTimer = 3.0f;
                        }
                        else
                        {
                            exportStatus = "Failed to save project.pix";
                            exportStatusTimer = 3.0f;
                        }
                    }
                    else if (ctrl && ev.key.shift && ev.key.code == sf::Keyboard::S)
                    {
                        // export frames as PNG sequence
                        if (canvas.exportAllFramesPNG("export/frame"))
                        {
                            exportStatus = "Exported frames to export/frame_#.png";
                            exportStatusTimer = 3.0f;
                        }
                        else
                        {
                            exportStatus = "Failed to export frames";
                            exportStatusTimer = 3.0f;
                        }
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
                        widthInputActive = true;
                        heightInputActive = false;
                    }
                    else if (ev.key.code == sf::Keyboard::E && ctrl)
                    {
                        // NEW: Open eraser size dialog
                        showEraserSizeDialog = !showEraserSizeDialog;
                        eraserSizeStr = std::to_string(canvas.eraserSize);
                    }
                    else if (ev.key.code == sf::Keyboard::Tab && (showResizeDialog || showEraserSizeDialog || showGIFExportDialog))
                    {
                        // Switch between inputs in dialogs
                        if (showResizeDialog)
                        {
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
                        else if (showEraserSizeDialog)
                        {
                            // Apply eraser size when Enter is pressed
                            try
                            {
                                int newSize = std::stoi(eraserSizeStr);
                                if (newSize > 0 && newSize <= 20) // Reasonable limits
                                {
                                    canvas.eraserSize = newSize;
                                    showEraserSizeDialog = false;
                                }
                            }
                            catch (...)
                            {
                                // Invalid input, ignore
                                std::cout << "Invalid input for eraser size!\n";
                            }
                        }
                        else if (showGIFExportDialog)
                        {
                            // Export GIF when Enter is pressed
                            try
                            {
                                int delay = std::stoi(gifDelayStr);
                                if (delay > 0 && delay <= 100)
                                {
                                    if (canvas.exportAsGIF("export/animation.gif", delay))
                                    {
                                        exportStatus = "Exported GIF: export/animation.gif";
                                        exportStatusTimer = 3.0f;
                                        showGIFExportDialog = false;
                                    }
                                    else
                                    {
                                        exportStatus = "Failed to export GIF";
                                        exportStatusTimer = 3.0f;
                                    }
                                }
                            }
                            catch (...)
                            {
                                exportStatus = "Invalid GIF delay value";
                                exportStatusTimer = 3.0f;
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
                        else if (showEraserSizeDialog)
                        {
                            // Cancel eraser size dialog
                            showEraserSizeDialog = false;
                        }
                        else if (showGIFExportDialog)
                        {
                            // Cancel GIF export dialog
                            showGIFExportDialog = false;
                        }
                        else if (showGodotExportDialog)
                        {
                            // Cancel Godot export dialog
                            showGodotExportDialog = false;
                        }
                        else if (colorPicker.isOpen)
                        {
                            colorPicker.isOpen = false;
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
                    else if (showEraserSizeDialog)
                    {
                        if (ev.text.unicode == '\b') // Backspace
                        {
                            if (!eraserSizeStr.empty())
                                eraserSizeStr.pop_back();
                        }
                        else if (ev.text.unicode >= '0' && ev.text.unicode <= '9')
                        {
                            // Limit eraser size input
                            if (eraserSizeStr.length() < 3)
                                eraserSizeStr += static_cast<char>(ev.text.unicode);
                        }
                    }
                    else if (showGIFExportDialog)
                    {
                        if (ev.text.unicode == '\b') // Backspace
                        {
                            if (!gifDelayStr.empty())
                                gifDelayStr.pop_back();
                        }
                        else if (ev.text.unicode >= '0' && ev.text.unicode <= '9')
                        {
                            // Limit GIF delay input
                            if (gifDelayStr.length() < 3)
                                gifDelayStr += static_cast<char>(ev.text.unicode);
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

            // Handle color picker clicks - ONLY on mouse press, not hold
            if (leftMousePressedThisFrame && !uiElementClicked)
            {
                if (colorPicker.handleClick(mpos, canvas.drawColor))
                {
                    uiElementClicked = true;
                }
            }

            // Drawing: convert mouse to pixel coords
            if (leftMouseDown && mouseInCanvas && !uiElementClicked && !colorPicker.isOpen && !showResizeDialog && !showEraserSizeDialog && !showGIFExportDialog && !showGodotExportDialog && !renamingFrame)
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
                        // NEW: Use eraser size
                        if (canvas.eraserSize == 1)
                        {
                            canvas.setPixelAtCurrentFrame(px, py, sf::Color(0, 0, 0, 0));
                        }
                        else
                        {
                            canvas.setPixelsAtCurrentFrame(px, py, sf::Color(0, 0, 0, 0), canvas.eraserSize);
                        }
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
            window.clear(sf::Color(EightBitColors::DarkPurple.r, EightBitColors::DarkPurple.g, EightBitColors::DarkPurple.b));

            // Draw main panels with 8-bit style
            drawPanel(window, sf::FloatRect(4, 4, (float)winSize.x - sidebarW - 8, toolbarH - 4), "TOOLS", font);
            drawPanel(window, sf::FloatRect(canvasArea.left - 4, canvasArea.top - 4, canvasArea.width + 8, canvasArea.height + 8), "CANVAS", font);
            drawPanel(window, sf::FloatRect(canvasArea.left + canvasArea.width + 4, 4, sidebarW - 8, (float)winSize.y - 8), "ANIMATION", font);

            // Toolbar buttons: Pencil, Eraser, Fill
            float x = 8;
            float y = 8;
            float bw = 64, bh = 32, spacing = 6;

            // Check hover states for tool buttons
            if (!uiElementClicked && !colorPicker.isOpen && !showResizeDialog && !showEraserSizeDialog && !showGIFExportDialog && !showGodotExportDialog && !renamingFrame)
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
            if (leftMousePressedThisFrame && hoveredToolButton == 0 && !uiElementClicked)
            {
                canvas.currentTool = Tool::Pencil;
                uiElementClicked = true;
            }

            x += bw + spacing;

            // Eraser button
            drawButton(window, sf::FloatRect(x, y, bw, bh), font, "ERASER",
                       canvas.currentTool == Tool::Eraser, hoveredToolButton == 1);
            if (leftMousePressedThisFrame && hoveredToolButton == 1 && !uiElementClicked)
            {
                canvas.currentTool = Tool::Eraser;
                uiElementClicked = true;
            }

            x += bw + spacing;

            // Fill button
            drawButton(window, sf::FloatRect(x, y, bw, bh), font, "FILL",
                       canvas.currentTool == Tool::Fill, hoveredToolButton == 2);
            if (leftMousePressedThisFrame && hoveredToolButton == 2 && !uiElementClicked)
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
            if (leftMousePressedThisFrame && colorBtnHovered && !uiElementClicked)
            {
                colorPicker.isOpen = !colorPicker.isOpen;
                uiElementClicked = true;
            }

            // NEW: Eraser size button
            bool eraserSizeBtnHovered = (mpos.x >= (int)(colorX + 110) && mpos.x <= (int)(colorX + 110 + 80) &&
                                         mpos.y >= (int)y && mpos.y <= (int)(y + bh));
            drawButton(window, sf::FloatRect(colorX + 110, y, 80, bh), font,
                       "ERASER: " + std::to_string(canvas.eraserSize), false, eraserSizeBtnHovered);
            if (leftMousePressedThisFrame && eraserSizeBtnHovered && !uiElementClicked)
            {
                showEraserSizeDialog = !showEraserSizeDialog;
                eraserSizeStr = std::to_string(canvas.eraserSize);
                uiElementClicked = true;
            }

            // Resize canvas button
            bool resizeBtnHovered = (mpos.x >= (int)(colorX + 200) && mpos.x <= (int)(colorX + 200 + 80) &&
                                     mpos.y >= (int)y && mpos.y <= (int)(y + bh));
            drawButton(window, sf::FloatRect(colorX + 200, y, 80, bh), font, "RESIZE", false, resizeBtnHovered);
            if (leftMousePressedThisFrame && resizeBtnHovered && !uiElementClicked)
            {
                showResizeDialog = !showResizeDialog;
                newWidthStr = std::to_string(canvas.width);
                newHeightStr = std::to_string(canvas.height);
                widthInputActive = true;
                heightInputActive = false;
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
                if (pt.loadFromImage(canvas.frames[prev].image))
                {
                    sf::Sprite ps(pt);
                    ps.setScale(canvas.zoom, canvas.zoom);
                    ps.setPosition(canvasArea.left + canvas.pan.x, canvasArea.top + canvas.pan.y);
                    ps.setColor(sf::Color(255, 255, 255, 100)); // semi-transparent
                    window.draw(ps);
                }
            }

            sf::Texture tex;
            if (tex.create(canvas.width, canvas.height))
            {
                tex.update(comp);
                sf::Sprite sprite(tex);
                sprite.setScale(canvas.zoom, canvas.zoom);
                sprite.setPosition(canvasArea.left + canvas.pan.x, canvasArea.top + canvas.pan.y);
                window.draw(sprite);
            }

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
            if (leftMousePressedThisFrame && playBtnHovered && !uiElementClicked)
            {
                playing = !playing;
                uiElementClicked = true;
            }

            // Prev frame button
            bool prevBtnHovered = (mpos.x >= (int)(sidebar.left + 76) && mpos.x <= (int)(sidebar.left + 104) &&
                                   mpos.y >= (int)controlY && mpos.y <= (int)(controlY + 28));
            drawButton(window, sf::FloatRect(sidebar.left + 76, controlY, 28, 28), font, "<", false, prevBtnHovered);
            if (leftMousePressedThisFrame && prevBtnHovered && !uiElementClicked)
            {
                canvas.prevFrame();
                uiElementClicked = true;
            }

            // Next frame button
            bool nextBtnHovered = (mpos.x >= (int)(sidebar.left + 112) && mpos.x <= (int)(sidebar.left + 140) &&
                                   mpos.y >= (int)controlY && mpos.y <= (int)(controlY + 28));
            drawButton(window, sf::FloatRect(sidebar.left + 112, controlY, 28, 28), font, ">", false, nextBtnHovered);
            if (leftMousePressedThisFrame && nextBtnHovered && !uiElementClicked)
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

                // Handle frame selection and controls - ONLY on mouse press
                if (leftMousePressedThisFrame && !uiElementClicked && !renamingFrame)
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
                            // Move up - only move one position up
                            if ((int)i == canvas.currentFrame)
                            {
                                canvas.moveFrameUp();
                            }
                            uiElementClicked = true;
                        }
                        else if (mm.x >= (int)(r.left + 80) && mm.x <= (int)(r.left + 100) && mm.y >= (int)buttonY && mm.y <= (int)(buttonY + 20))
                        {
                            // Move down - only move one position down
                            if ((int)i == canvas.currentFrame)
                            {
                                canvas.moveFrameDown();
                            }
                            uiElementClicked = true;
                        }
                        else if (mm.x >= (int)(r.left + 104) && mm.x <= (int)(r.left + 124) && mm.y >= (int)buttonY && mm.y <= (int)(buttonY + 20) && !duplicateClicked)
                        {
                            // Duplicate - only once per click
                            if ((int)i == canvas.currentFrame)
                            {
                                canvas.duplicateFrame();
                            }
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
            if (leftMousePressedThisFrame && addFrameHovered && !renamingFrame && !uiElementClicked)
            {
                canvas.addFrame();
                uiElementClicked = true;
            }

            // NEW: Export buttons
            float exportY = fy + 45;

            // GIF Export button
            bool gifExportHovered = (mpos.x >= (int)(sidebar.left + 8) && mpos.x <= (int)(sidebar.left + 88) &&
                                     mpos.y >= (int)(exportY) && mpos.y <= (int)(exportY + 28));
            drawButton(window, sf::FloatRect(sidebar.left + 8, exportY, 80, 28), font, "EXPORT GIF", false, gifExportHovered);
            if (leftMousePressedThisFrame && gifExportHovered && !uiElementClicked)
            {
                showGIFExportDialog = !showGIFExportDialog;
                gifDelayStr = "5";
                uiElementClicked = true;
            }

            // Godot Export button
            bool godotExportHovered = (mpos.x >= (int)(sidebar.left + 96) && mpos.x <= (int)(sidebar.left + 176) &&
                                       mpos.y >= (int)(exportY) && mpos.y <= (int)(exportY + 28));
            drawButton(window, sf::FloatRect(sidebar.left + 96, exportY, 80, 28), font, "GODOT", false, godotExportHovered);
            if (leftMousePressedThisFrame && godotExportHovered && !uiElementClicked)
            {
                if (canvas.exportForGodot("export"))
                {
                    exportStatus = "Exported Godot resources to export/godot_export/";
                    exportStatusTimer = 5.0f;
                }
                else
                {
                    exportStatus = "Failed to export Godot resources";
                    exportStatusTimer = 3.0f;
                }
                uiElementClicked = true;
            }

            // PNG Export button
            bool pngExportHovered = (mpos.x >= (int)(sidebar.left + 8) && mpos.x <= (int)(sidebar.left + 88) &&
                                     mpos.y >= (int)(exportY + 35) && mpos.y <= (int)(exportY + 63));
            drawButton(window, sf::FloatRect(sidebar.left + 8, exportY + 35, 80, 28), font, "EXPORT PNG", false, pngExportHovered);
            if (leftMousePressedThisFrame && pngExportHovered && !uiElementClicked)
            {
                if (canvas.exportCurrentFramePNG("export/frame.png"))
                {
                    exportStatus = "Exported current frame to export/frame.png";
                    exportStatusTimer = 3.0f;
                }
                else
                {
                    exportStatus = "Failed to export frame";
                    exportStatusTimer = 3.0f;
                }
                uiElementClicked = true;
            }

            // Draw color picker if open
            colorPicker.draw(window, font);

            // Draw resize dialog with 8-bit style
            if (showResizeDialog)
            {
                sf::Vector2f dialogSize(250, 150);
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

                // Handle resize dialog clicks - ONLY on mouse press
                if (leftMousePressedThisFrame && !uiElementClicked)
                {
                    sf::Vector2i mm = sf::Mouse::getPosition(window);

                    // Check width input click
                    sf::FloatRect widthInputRect(dialogPos.x + 80, dialogPos.y + 40, 80, 25);
                    if (widthInputRect.contains((float)mm.x, (float)mm.y))
                    {
                        widthInputActive = true;
                        heightInputActive = false;
                        uiElementClicked = true;
                    }

                    // Check height input click
                    sf::FloatRect heightInputRect(dialogPos.x + 80, dialogPos.y + 75, 80, 25);
                    if (heightInputRect.contains((float)mm.x, (float)mm.y))
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
                            // Invalid input, ignore
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
                    if (!dialogRect.contains((float)mm.x, (float)mm.y))
                    {
                        showResizeDialog = false;
                        widthInputActive = false;
                        heightInputActive = false;
                        uiElementClicked = true;
                    }
                }
            }

            // Draw eraser size dialog with 8-bit style
            if (showEraserSizeDialog)
            {
                sf::Vector2f dialogSize(250, 120);
                sf::Vector2f dialogPos(winSize.x / 2 - dialogSize.x / 2, winSize.y / 2 - dialogSize.y / 2);

                // Background with 8-bit style
                sf::RectangleShape dialogBg(dialogSize);
                dialogBg.setPosition(dialogPos);
                dialogBg.setFillColor(sf::Color(EightBitColors::DarkBlue.r, EightBitColors::DarkBlue.g, EightBitColors::DarkBlue.b));
                dialogBg.setOutlineColor(sf::Color(EightBitColors::Yellow.r, EightBitColors::Yellow.g, EightBitColors::Yellow.b));
                dialogBg.setOutlineThickness(2);
                window.draw(dialogBg);

                // Title
                sf::Text title("ERASER SIZE", font, 16);
                title.setStyle(sf::Text::Bold);
                title.setPosition(dialogPos.x + 10, dialogPos.y + 10);
                title.setFillColor(sf::Color(EightBitColors::Yellow.r, EightBitColors::Yellow.g, EightBitColors::Yellow.b));
                window.draw(title);

                // Size input
                sf::Text sizeLabel("SIZE (1-20):", font, 14);
                sizeLabel.setStyle(sf::Text::Bold);
                sizeLabel.setPosition(dialogPos.x + 20, dialogPos.y + 40);
                sizeLabel.setFillColor(sf::Color(EightBitColors::White.r, EightBitColors::White.g, EightBitColors::White.b));
                window.draw(sizeLabel);

                sf::RectangleShape sizeInput({80, 25});
                sizeInput.setPosition(dialogPos.x + 120, dialogPos.y + 40);
                sizeInput.setFillColor(sf::Color(EightBitColors::Black.r, EightBitColors::Black.g, EightBitColors::Black.b));
                sizeInput.setOutlineColor(sf::Color(EightBitColors::Yellow.r, EightBitColors::Yellow.g, EightBitColors::Yellow.b));
                sizeInput.setOutlineThickness(2);
                window.draw(sizeInput);

                sf::Text sizeText(eraserSizeStr, font, 14);
                sizeText.setStyle(sf::Text::Bold);
                sizeText.setPosition(dialogPos.x + 125, dialogPos.y + 45);
                sizeText.setFillColor(sf::Color(EightBitColors::White.r, EightBitColors::White.g, EightBitColors::White.b));
                window.draw(sizeText);

                // Apply button
                bool applyHovered = (mpos.x >= (int)(dialogPos.x + 50) && mpos.x <= (int)(dialogPos.x + 110) &&
                                     mpos.y >= (int)(dialogPos.y + 75) && mpos.y <= (int)(dialogPos.y + 100));
                drawButton(window, sf::FloatRect(dialogPos.x + 50, dialogPos.y + 75, 60, 25), font, "APPLY", false, applyHovered);

                // Cancel button
                bool cancelHovered = (mpos.x >= (int)(dialogPos.x + 140) && mpos.x <= (int)(dialogPos.x + 200) &&
                                      mpos.y >= (int)(dialogPos.y + 75) && mpos.y <= (int)(dialogPos.y + 100));
                drawButton(window, sf::FloatRect(dialogPos.x + 140, dialogPos.y + 75, 60, 25), font, "CANCEL", false, cancelHovered);

                // Handle eraser size dialog clicks - ONLY on mouse press
                if (leftMousePressedThisFrame && !uiElementClicked)
                {
                    sf::Vector2i mm = sf::Mouse::getPosition(window);

                    // Check size input click
                    sf::FloatRect sizeInputRect(dialogPos.x + 120, dialogPos.y + 40, 80, 25);
                    if (sizeInputRect.contains((float)mm.x, (float)mm.y))
                    {
                        uiElementClicked = true;
                    }

                    // Apply button
                    if (applyHovered)
                    {
                        try
                        {
                            int newSize = std::stoi(eraserSizeStr);
                            if (newSize > 0 && newSize <= 20)
                            {
                                canvas.eraserSize = newSize;
                                showEraserSizeDialog = false;
                            }
                        }
                        catch (...)
                        {
                            // Invalid input, ignore
                            std::cout << "Invalid input for eraser size!\n";
                        }
                        uiElementClicked = true;
                    }

                    // Cancel button
                    if (cancelHovered)
                    {
                        showEraserSizeDialog = false;
                        uiElementClicked = true;
                    }

                    // Click outside dialog - close it
                    sf::FloatRect dialogRect(dialogPos.x, dialogPos.y, dialogSize.x, dialogSize.y);
                    if (!dialogRect.contains((float)mm.x, (float)mm.y))
                    {
                        showEraserSizeDialog = false;
                        uiElementClicked = true;
                    }
                }
            }

            // NEW: Draw GIF export dialog
            if (showGIFExportDialog)
            {
                sf::Vector2f dialogSize(250, 150);
                sf::Vector2f dialogPos(winSize.x / 2 - dialogSize.x / 2, winSize.y / 2 - dialogSize.y / 2);

                // Background with 8-bit style
                sf::RectangleShape dialogBg(dialogSize);
                dialogBg.setPosition(dialogPos);
                dialogBg.setFillColor(sf::Color(EightBitColors::DarkBlue.r, EightBitColors::DarkBlue.g, EightBitColors::DarkBlue.b));
                dialogBg.setOutlineColor(sf::Color(EightBitColors::Yellow.r, EightBitColors::Yellow.g, EightBitColors::Yellow.b));
                dialogBg.setOutlineThickness(2);
                window.draw(dialogBg);

                // Title
                sf::Text title("EXPORT GIF", font, 16);
                title.setStyle(sf::Text::Bold);
                title.setPosition(dialogPos.x + 10, dialogPos.y + 10);
                title.setFillColor(sf::Color(EightBitColors::Yellow.r, EightBitColors::Yellow.g, EightBitColors::Yellow.b));
                window.draw(title);

                // Delay input
                sf::Text delayLabel("DELAY (1-100):", font, 14);
                delayLabel.setStyle(sf::Text::Bold);
                delayLabel.setPosition(dialogPos.x + 20, dialogPos.y + 40);
                delayLabel.setFillColor(sf::Color(EightBitColors::White.r, EightBitColors::White.g, EightBitColors::White.b));
                window.draw(delayLabel);

                sf::RectangleShape delayInput({80, 25});
                delayInput.setPosition(dialogPos.x + 140, dialogPos.y + 40);
                delayInput.setFillColor(sf::Color(EightBitColors::Black.r, EightBitColors::Black.g, EightBitColors::Black.b));
                delayInput.setOutlineColor(sf::Color(EightBitColors::Yellow.r, EightBitColors::Yellow.g, EightBitColors::Yellow.b));
                delayInput.setOutlineThickness(2);
                window.draw(delayInput);

                sf::Text delayText(gifDelayStr, font, 14);
                delayText.setStyle(sf::Text::Bold);
                delayText.setPosition(dialogPos.x + 145, dialogPos.y + 45);
                delayText.setFillColor(sf::Color(EightBitColors::White.r, EightBitColors::White.g, EightBitColors::White.b));
                window.draw(delayText);

                // Info text
                sf::Text info("Higher delay = slower animation", font, 12);
                info.setStyle(sf::Text::Bold);
                info.setPosition(dialogPos.x + 20, dialogPos.y + 70);
                info.setFillColor(sf::Color(EightBitColors::LightGray.r, EightBitColors::LightGray.g, EightBitColors::LightGray.b));
                window.draw(info);

                // Apply button
                bool applyHovered = (mpos.x >= (int)(dialogPos.x + 50) && mpos.x <= (int)(dialogPos.x + 110) &&
                                     mpos.y >= (int)(dialogPos.y + 95) && mpos.y <= (int)(dialogPos.y + 120));
                drawButton(window, sf::FloatRect(dialogPos.x + 50, dialogPos.y + 95, 60, 25), font, "EXPORT", false, applyHovered);

                // Cancel button
                bool cancelHovered = (mpos.x >= (int)(dialogPos.x + 140) && mpos.x <= (int)(dialogPos.x + 200) &&
                                      mpos.y >= (int)(dialogPos.y + 95) && mpos.y <= (int)(dialogPos.y + 120));
                drawButton(window, sf::FloatRect(dialogPos.x + 140, dialogPos.y + 95, 60, 25), font, "CANCEL", false, cancelHovered);

                // Handle GIF export dialog clicks - ONLY on mouse press
                if (leftMousePressedThisFrame && !uiElementClicked)
                {
                    sf::Vector2i mm = sf::Mouse::getPosition(window);

                    // Check delay input click
                    sf::FloatRect delayInputRect(dialogPos.x + 140, dialogPos.y + 40, 80, 25);
                    if (delayInputRect.contains((float)mm.x, (float)mm.y))
                    {
                        uiElementClicked = true;
                    }

                    // Apply button
                    if (applyHovered)
                    {
                        try
                        {
                            int delay = std::stoi(gifDelayStr);
                            if (delay > 0 && delay <= 100)
                            {
                                if (canvas.exportAsGIF("export/animation.gif", delay))
                                {
                                    exportStatus = "Exported GIF: export/animation.gif";
                                    exportStatusTimer = 3.0f;
                                    showGIFExportDialog = false;
                                }
                                else
                                {
                                    exportStatus = "Failed to export GIF";
                                    exportStatusTimer = 3.0f;
                                }
                            }
                        }
                        catch (...)
                        {
                            exportStatus = "Invalid GIF delay value";
                            exportStatusTimer = 3.0f;
                        }
                        uiElementClicked = true;
                    }

                    // Cancel button
                    if (cancelHovered)
                    {
                        showGIFExportDialog = false;
                        uiElementClicked = true;
                    }

                    // Click outside dialog - close it
                    sf::FloatRect dialogRect(dialogPos.x, dialogPos.y, dialogSize.x, dialogSize.y);
                    if (!dialogRect.contains((float)mm.x, (float)mm.y))
                    {
                        showGIFExportDialog = false;
                        uiElementClicked = true;
                    }
                }
            }

            // NEW: Draw export status message
            if (!exportStatus.empty())
            {
                sf::Text statusText(exportStatus, font, 14);
                statusText.setStyle(sf::Text::Bold);
                statusText.setPosition(winSize.x / 2 - statusText.getLocalBounds().width / 2, winSize.y - 40);
                statusText.setFillColor(sf::Color(EightBitColors::Green.r, EightBitColors::Green.g, EightBitColors::Green.b));
                window.draw(statusText);
            }

            // Small status text with 8-bit style
            std::string toolName = canvas.currentTool == Tool::Pencil ? "PENCIL" : canvas.currentTool == Tool::Eraser ? "ERASER"
                                                                                                                      : "FILL";
            sf::Text status("TOOL: " + toolName + "  FRAME: " + std::to_string(canvas.currentFrame) +
                                "  ZOOM: " + std::to_string((int)canvas.zoom) + "x" +
                                (canvas.currentTool == Tool::Eraser ? "  ERASER SIZE: " + std::to_string(canvas.eraserSize) : ""),
                            font, 12);
            status.setStyle(sf::Text::Bold);
            status.setPosition(8, winSize.y - 22);
            status.setFillColor(sf::Color(EightBitColors::Yellow.r, EightBitColors::Yellow.g, EightBitColors::Yellow.b));
            window.draw(status);

            window.display();
        }
    }
    catch (const std::exception &e)
    {
        std::cerr << "Exception caught: " << e.what() << std::endl;
    }
    catch (...)
    {
        std::cerr << "Unknown exception caught!" << std::endl;
    }

    return 0;
}