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

#ifdef _WIN32
#include <windows.h>
#include <direct.h>
#define getcwd _getcwd
#else
#include <unistd.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#endif

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

namespace UIColorTheme
{
    // Dark background tones
    const Color DarkBackground(30, 35, 45);  // Dark blue-gray
    const Color PanelBackground(45, 50, 60); // Medium blue-gray
    const Color LightBackground(60, 65, 75); // Light blue-gray

    // Accent colors (muted and soothing)
    const Color SoftTeal(80, 130, 140);    // Muted teal
    const Color DustyBlue(100, 150, 180);  // Soft blue
    const Color MutedGreen(100, 160, 130); // Sage green
    const Color WarmGray(120, 110, 110);   // Muted purple-gray

    // UI elements
    const Color SoftWhite(220, 220, 220);  // Off-white, not pure white
    const Color MediumGray(150, 150, 150); // Medium gray
    const Color DarkGray(80, 80, 80);      // Dark gray

    // Highlights (subtle)
    const Color SoftYellow(200, 180, 100); // Muted gold
    const Color Peach(200, 150, 130);      // Soft orange

    // Button states
    const Color ButtonNormal(60, 65, 75);   // Normal button
    const Color ButtonHover(70, 120, 130);  // Button hover
    const Color ButtonActive(80, 130, 140); // Button active

    // Special UI elements
    const Color GridLines(60, 70, 80); // Subtle grid
    const Color CanvasBg(35, 40, 45);  // Canvas background
}

// Simple GIF encoder (minimal implementation)
class SimpleGIFEncoder
{
private:
    std::ofstream file;
    int width, height;
    int delay; // in hundredths of a second

    void writeLZWData(const std::vector<u8> &imageData)
    {
        // Simple LZW compression implementation
        std::vector<u8> output;

        // LZW minimum code size
        u8 minCodeSize = 8;
        output.push_back(minCodeSize);

        // For simplicity, we'll use basic encoding
        std::vector<int> codes;

        // Initialize dictionary
        std::vector<std::vector<u8>> dictionary;
        for (int i = 0; i < 256; i++)
        {
            dictionary.push_back({(u8)i});
        }

        std::vector<u8> current;
        for (u8 pixel : imageData)
        {
            std::vector<u8> next = current;
            next.push_back(pixel);

            bool found = false;
            for (size_t i = 0; i < dictionary.size(); i++)
            {
                if (dictionary[i] == next)
                {
                    found = true;
                    current = next;
                    break;
                }
            }

            if (!found)
            {
                // Add current to codes
                for (size_t i = 0; i < dictionary.size(); i++)
                {
                    if (dictionary[i] == current)
                    {
                        codes.push_back(i);
                        break;
                    }
                }

                // Add new sequence to dictionary
                dictionary.push_back(next);
                current = {pixel};
            }
        }

        // Add last code
        for (size_t i = 0; i < dictionary.size(); i++)
        {
            if (dictionary[i] == current)
            {
                codes.push_back(i);
                break;
            }
        }

        // End of information code
        codes.push_back(256);

        // Convert codes to bit stream (simplified)
        std::vector<u8> compressed;
        int currentByte = 0;
        int bitCount = 0;
        int codeSize = minCodeSize + 1;

        for (int code : codes)
        {
            for (int i = 0; i < codeSize; i++)
            {
                currentByte |= ((code >> i) & 1) << bitCount;
                bitCount++;
                if (bitCount == 8)
                {
                    compressed.push_back(currentByte);
                    currentByte = 0;
                    bitCount = 0;
                }
            }
        }

        if (bitCount > 0)
        {
            compressed.push_back(currentByte);
        }

        // Write compressed data in sub-blocks
        size_t pos = 0;
        while (pos < compressed.size())
        {
            u8 blockSize = (u8)std::min((size_t)255, compressed.size() - pos);
            output.push_back(blockSize);
            for (int i = 0; i < blockSize; i++)
            {
                output.push_back(compressed[pos++]);
            }
        }
        output.push_back(0); // Block terminator

        // Write all data blocks
        for (u8 byte : output)
        {
            file.put(byte);
        }
    }

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

        // Write Global Color Table (using 8-bit palette)
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

        // Convert image to indexed color
        std::vector<u8> indexedData;
        for (int y = 0; y < height; ++y)
        {
            for (int x = 0; x < width; ++x)
            {
                sf::Color pixel = image.getPixel(x, y);
                if (pixel.a == 0)
                {
                    indexedData.push_back(0); // Transparent -> black
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
                    indexedData.push_back(bestIndex);
                }
            }
        }

        // Write LZW compressed image data
        writeLZWData(indexedData);

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
        image.create(w, h, sf::Color(0, 0, 0, 0));
        updateThumbnail();
    }

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
        unsigned thumbSize = 48;
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

        thumbnail.loadFromImage(thumbImg);
    }
};

enum class Tool
{
    Pencil,
    Eraser,
    Fill
};

// NEW: File Browser Class
class FileBrowser
{
public:
    bool isOpen = false;
    std::string currentPath;
    std::vector<std::string> files;
    std::vector<std::string> directories;
    std::string selectedFile;
    std::string filenameInput;
    bool filenameInputActive = false;
    std::string title;
    std::string defaultExtension;
    std::vector<std::string> allowedExtensions;

    FileBrowser()
    {
#ifdef _WIN32
        currentPath = "C:\\";
#else
        char cwd[1024];
        if (getcwd(cwd, sizeof(cwd)) != NULL)
        {
            currentPath = cwd;
        }
        else
        {
            currentPath = "/";
        }
#endif
        refresh();
    }

    void refresh()
    {
        files.clear();
        directories.clear();

#ifdef _WIN32
        // Windows implementation
        std::string searchPath = currentPath + "\\*";
        WIN32_FIND_DATAA findData;
        HANDLE hFind = FindFirstFileA(searchPath.c_str(), &findData);

        if (hFind != INVALID_HANDLE_VALUE)
        {
            do
            {
                std::string name = findData.cFileName;
                if (name == "." || name == "..")
                    continue;

                if (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
                {
                    directories.push_back(name + "\\");
                }
                else
                {
                    files.push_back(name);
                }
            } while (FindNextFileA(hFind, &findData));
            FindClose(hFind);
        }
#else
        // Linux/Mac implementation
        DIR *dir = opendir(currentPath.c_str());
        if (dir)
        {
            struct dirent *entry;
            while ((entry = readdir(dir)) != NULL)
            {
                std::string name = entry->d_name;
                if (name == "." || name == "..")
                    continue;

                // Check if it's a directory
                std::string fullPath = currentPath + "/" + name;
                struct stat statbuf;
                if (stat(fullPath.c_str(), &statbuf) == 0)
                {
                    if (S_ISDIR(statbuf.st_mode))
                    {
                        directories.push_back(name + "/");
                    }
                    else
                    {
                        files.push_back(name);
                    }
                }
            }
            closedir(dir);
        }
#endif

        // Sort directories and files
        std::sort(directories.begin(), directories.end());
        std::sort(files.begin(), files.end());
    }

    void goUp()
    {
#ifdef _WIN32
        size_t pos = currentPath.find_last_of("\\");
        if (pos != std::string::npos && pos > 2) // Keep at least "C:\"
        {
            currentPath = currentPath.substr(0, pos);
            if (currentPath.size() == 2)
                currentPath += "\\"; // Ensure "C:" becomes "C:\"
        }
#else
        size_t pos = currentPath.find_last_of("/");
        if (pos != std::string::npos && pos > 0)
        {
            currentPath = currentPath.substr(0, pos);
        }
        else
        {
            currentPath = "/";
        }
#endif
        refresh();
        selectedFile.clear();
    }

    void enterDirectory(const std::string &dirName)
    {
#ifdef _WIN32
        currentPath = currentPath + "\\" + dirName.substr(0, dirName.size() - 1);
#else
        currentPath = currentPath + "/" + dirName.substr(0, dirName.size() - 1);
#endif
        refresh();
        selectedFile.clear();
    }

    bool isAllowedExtension(const std::string &filename) const
    {
        if (allowedExtensions.empty())
            return true;

        size_t dotPos = filename.find_last_of(".");
        if (dotPos == std::string::npos)
            return false;

        std::string ext = filename.substr(dotPos);
        for (const auto &allowed : allowedExtensions)
        {
            if (ext == allowed)
                return true;
        }
        return false;
    }

    void draw(sf::RenderWindow &window, sf::Font &font)
    {
        if (!isOpen)
            return;

        sf::Vector2u winSize = window.getSize();
        sf::Vector2f dialogSize(600, 400);
        sf::Vector2f dialogPos(winSize.x / 2 - dialogSize.x / 2, winSize.y / 2 - dialogSize.y / 2);

        // Background
        sf::RectangleShape bg(dialogSize);
        bg.setPosition(dialogPos);
        bg.setFillColor(sf::Color(UIColorTheme::PanelBackground.r, UIColorTheme::PanelBackground.g, UIColorTheme::PanelBackground.b));
        bg.setOutlineColor(sf::Color(UIColorTheme::SoftTeal.r, UIColorTheme::SoftTeal.g, UIColorTheme::SoftTeal.b));

        bg.setOutlineThickness(2);
        window.draw(bg);

        // Title
        sf::Text titleText(title, font, 18);
        titleText.setStyle(sf::Text::Bold);
        titleText.setPosition(dialogPos.x + 10, dialogPos.y + 10);
        titleText.setFillColor(sf::Color(EightBitColors::Yellow.r, EightBitColors::Yellow.g, EightBitColors::Yellow.b));
        window.draw(titleText);

        // Current path
        sf::Text pathText("Path: " + currentPath, font, 12);
        pathText.setPosition(dialogPos.x + 10, dialogPos.y + 40);
        pathText.setFillColor(sf::Color(EightBitColors::White.r, EightBitColors::White.g, EightBitColors::White.b));
        window.draw(pathText);

        // File list background
        sf::RectangleShape listBg({dialogSize.x - 20, 250});
        listBg.setPosition(dialogPos.x + 10, dialogPos.y + 60);
        listBg.setFillColor(sf::Color(EightBitColors::Black.r, EightBitColors::Black.g, EightBitColors::Black.b));
        listBg.setOutlineColor(sf::Color(EightBitColors::LightGray.r, EightBitColors::LightGray.g, EightBitColors::LightGray.b));
        listBg.setOutlineThickness(1);
        window.draw(listBg);

        // Draw directories and files
        float listY = dialogPos.y + 65;
        int maxItems = 15;
        int itemsDrawn = 0;

        // Up directory button
        if (itemsDrawn < maxItems)
        {
            bool isSelected = (selectedFile == "..");
            sf::Text dirText("[..]", font, 12);
            dirText.setPosition(dialogPos.x + 15, listY);
            dirText.setFillColor(isSelected ? sf::Color(EightBitColors::Yellow.r, EightBitColors::Yellow.g, EightBitColors::Yellow.b) : sf::Color(EightBitColors::Blue.r, EightBitColors::Blue.g, EightBitColors::Blue.b));
            window.draw(dirText);
            listY += 20;
            itemsDrawn++;
        }

        // Directories
        for (const auto &dir : directories)
        {
            if (itemsDrawn >= maxItems)
                break;
            bool isSelected = (selectedFile == dir);
            sf::Text dirText("[" + dir + "]", font, 12);
            dirText.setPosition(dialogPos.x + 15, listY);
            dirText.setFillColor(isSelected ? sf::Color(EightBitColors::Yellow.r, EightBitColors::Yellow.g, EightBitColors::Yellow.b) : sf::Color(EightBitColors::Blue.r, EightBitColors::Blue.g, EightBitColors::Blue.b));
            window.draw(dirText);
            listY += 20;
            itemsDrawn++;
        }

        // Files
        for (const auto &file : files)
        {
            if (itemsDrawn >= maxItems)
                break;
            if (!allowedExtensions.empty() && !isAllowedExtension(file))
                continue;

            bool isSelected = (selectedFile == file);
            sf::Text fileText(file, font, 12);
            fileText.setPosition(dialogPos.x + 15, listY);
            fileText.setFillColor(isSelected ? sf::Color(EightBitColors::Yellow.r, EightBitColors::Yellow.g, EightBitColors::Yellow.b) : sf::Color(EightBitColors::White.r, EightBitColors::White.g, EightBitColors::White.b));
            window.draw(fileText);
            listY += 20;
            itemsDrawn++;
        }

        // Filename input
        sf::Text filenameLabel("Filename:", font, 14);
        filenameLabel.setPosition(dialogPos.x + 10, dialogPos.y + 320);
        filenameLabel.setFillColor(sf::Color(EightBitColors::White.r, EightBitColors::White.g, EightBitColors::White.b));
        window.draw(filenameLabel);

        sf::RectangleShape filenameInputBox({400, 25});
        filenameInputBox.setPosition(dialogPos.x + 80, dialogPos.y + 320);
        filenameInputBox.setFillColor(sf::Color(EightBitColors::Black.r, EightBitColors::Black.g, EightBitColors::Black.b));
        filenameInputBox.setOutlineColor(filenameInputActive ? sf::Color(EightBitColors::Yellow.r, EightBitColors::Yellow.g, EightBitColors::Yellow.b) : sf::Color(EightBitColors::White.r, EightBitColors::White.g, EightBitColors::White.b));
        filenameInputBox.setOutlineThickness(2);
        window.draw(filenameInputBox);

        std::string displayText;
        if (!filenameInput.empty())
        {
            displayText = filenameInput;
        }
        else if (!selectedFile.empty() && selectedFile != ".." &&
                 std::find(directories.begin(), directories.end(), selectedFile) == directories.end())
        {
            displayText = selectedFile;
        }

        sf::Text filenameText(displayText, font, 14);
        filenameText.setPosition(dialogPos.x + 85, dialogPos.y + 325);
        filenameText.setFillColor(sf::Color(EightBitColors::White.r, EightBitColors::White.g, EightBitColors::White.b));
        window.draw(filenameText);

        // Buttons
        drawButton(window, sf::FloatRect(dialogPos.x + 490, dialogPos.y + 320, 100, 25), font, "OK", false, false);
        drawButton(window, sf::FloatRect(dialogPos.x + 490, dialogPos.y + 355, 100, 25), font, "CANCEL", false, false);
    }

    bool handleEvent(const sf::Event &event, sf::Vector2i mousePos, sf::RenderWindow &window)
    {
        if (!isOpen)
            return false;

        sf::Vector2u winSize = window.getSize();
        sf::Vector2f dialogSize(600, 400);
        sf::Vector2f dialogPos(winSize.x / 2 - dialogSize.x / 2, winSize.y / 2 - dialogSize.y / 2);

        if (event.type == sf::Event::MouseButtonPressed && event.mouseButton.button == sf::Mouse::Left)
        {
            // Check file list clicks
            float listStartY = dialogPos.y + 65;
            float itemHeight = 20;

            // Check ".." directory
            sf::FloatRect upDirRect(dialogPos.x + 10, listStartY, 580, itemHeight);
            if (upDirRect.contains((float)mousePos.x, (float)mousePos.y))
            {
                selectedFile = "..";
                filenameInput.clear();
                return true;
            }

            // Check directories
            float currentY = listStartY + itemHeight;
            for (const auto &dir : directories)
            {
                sf::FloatRect dirRect(dialogPos.x + 10, currentY, 580, itemHeight);
                if (dirRect.contains((float)mousePos.x, (float)mousePos.y))
                {
                    selectedFile = dir;
                    filenameInput.clear();
                    return true;
                }
                currentY += itemHeight;
            }

            // Check files
            for (const auto &file : files)
            {
                if (!allowedExtensions.empty() && !isAllowedExtension(file))
                    continue;
                sf::FloatRect fileRect(dialogPos.x + 10, currentY, 580, itemHeight);
                if (fileRect.contains((float)mousePos.x, (float)mousePos.y))
                {
                    selectedFile = file;
                    filenameInput = file;
                    return true;
                }
                currentY += itemHeight;
            }

            // Check filename input
            sf::FloatRect filenameRect(dialogPos.x + 80, dialogPos.y + 320, 400, 25);
            if (filenameRect.contains((float)mousePos.x, (float)mousePos.y))
            {
                filenameInputActive = true;
                return true;
            }

            // Check OK button
            sf::FloatRect okRect(dialogPos.x + 490, dialogPos.y + 320, 100, 25);
            if (okRect.contains((float)mousePos.x, (float)mousePos.y))
            {
                if (!filenameInput.empty())
                {
                    // Add default extension if needed
                    std::string finalFilename = filenameInput;
                    if (!defaultExtension.empty() && finalFilename.find('.') == std::string::npos)
                    {
                        finalFilename += defaultExtension;
                    }
                    selectedFile = finalFilename;
                    return true; // Dialog will be closed by caller
                }
                else if (!selectedFile.empty() && selectedFile != ".." &&
                         std::find(directories.begin(), directories.end(), selectedFile) == directories.end())
                {
                    // Use the selected file if no input was provided
                    return true;
                }
                return true; // Still return true even if no file selected
            }
            // Check Cancel button
            sf::FloatRect cancelRect(dialogPos.x + 490, dialogPos.y + 355, 100, 25);
            if (cancelRect.contains((float)mousePos.x, (float)mousePos.y))
            {
                isOpen = false;
                selectedFile.clear();
                return true;
            }

            // Click outside dialog
            sf::FloatRect dialogRect(dialogPos.x, dialogPos.y, dialogSize.x, dialogSize.y);
            if (!dialogRect.contains((float)mousePos.x, (float)mousePos.y))
            {
                filenameInputActive = false;
            }
        }
        else if (event.type == sf::Event::KeyPressed)
        {
            if (event.key.code == sf::Keyboard::Return)
            {
                if (!filenameInput.empty())
                {
                    std::string finalFilename = filenameInput;
                    if (!defaultExtension.empty() && finalFilename.find('.') == std::string::npos)
                    {
                        finalFilename += defaultExtension;
                    }
                    selectedFile = finalFilename;
                    return true;
                }
            }
            else if (event.key.code == sf::Keyboard::Escape)
            {
                isOpen = false;
                selectedFile.clear();
                return true;
            }
            else if (event.key.code == sf::Keyboard::BackSpace && filenameInputActive)
            {
                if (!filenameInput.empty())
                {
                    filenameInput.pop_back();
                }
            }
        }
        else if (event.type == sf::Event::TextEntered && filenameInputActive)
        {
            if (event.text.unicode >= 32 && event.text.unicode < 127)
            {
                filenameInput += static_cast<char>(event.text.unicode);
            }
        }

        return false;
    }

    void doubleClick(sf::Vector2i mousePos, sf::RenderWindow &window)
    {
        sf::Vector2u winSize = window.getSize();
        sf::Vector2f dialogSize(600, 400);
        sf::Vector2f dialogPos(winSize.x / 2 - dialogSize.x / 2, winSize.y / 2 - dialogSize.y / 2);

        float listStartY = dialogPos.y + 65;
        float itemHeight = 20;

        // Check ".." directory
        sf::FloatRect upDirRect(dialogPos.x + 10, listStartY, 580, itemHeight);
        if (upDirRect.contains((float)mousePos.x, (float)mousePos.y))
        {
            goUp();
            return;
        }

        // Check directories
        float currentY = listStartY + itemHeight;
        for (const auto &dir : directories)
        {
            sf::FloatRect dirRect(dialogPos.x + 10, currentY, 580, itemHeight);
            if (dirRect.contains((float)mousePos.x, (float)mousePos.y))
            {
                enterDirectory(dir);
                return;
            }
            currentY += itemHeight;
        }
    }

private:
    void drawButton(sf::RenderWindow &w, const sf::FloatRect &rect, const sf::Font &font,
                    const std::string &label, bool isActive, bool isHovered)
    {
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

        sf::Text t(label, font, 12);
        t.setStyle(sf::Text::Bold);
        sf::FloatRect textBounds = t.getLocalBounds();
        t.setPosition(rect.left + (rect.width - textBounds.width) / 2,
                      rect.top + (rect.height - textBounds.height) / 2 - 2);
        t.setFillColor(sf::Color(EightBitColors::White.r, EightBitColors::White.g, EightBitColors::White.b));
        w.draw(t);
    }
};

// NEW: Help Dialog Class
class HelpDialog
{
public:
    bool isOpen = false;
    sf::Vector2f position{200, 100};
    sf::Vector2f size{500, 400};

    void draw(sf::RenderWindow &window, sf::Font &font)
    {
        if (!isOpen)
            return;

        sf::Vector2u winSize = window.getSize();
        sf::Vector2f dialogPos(winSize.x / 2 - size.x / 2, winSize.y / 2 - size.y / 2);

        // Background with softer colors
        sf::RectangleShape bg(size);
        bg.setPosition(dialogPos);
        bg.setFillColor(sf::Color(50, 60, 90));       // Softer blue
        bg.setOutlineColor(sf::Color(120, 180, 255)); // Light blue border
        bg.setOutlineThickness(2);
        window.draw(bg);

        // Title
        sf::Text title("KEYBOARD SHORTCUTS", font, 18);
        title.setStyle(sf::Text::Bold);
        title.setPosition(dialogPos.x + 20, dialogPos.y + 20);
        title.setFillColor(sf::Color(255, 220, 100)); // Soft yellow
        window.draw(title);

        // Shortcuts list
        std::vector<std::pair<std::string, std::string>> shortcuts = {
            {"Ctrl + Z", "Undo"},
            {"Ctrl + Y", "Redo"},
            {"Ctrl + S", "Save Project"},
            {"Ctrl + O", "Open Project"},
            {"Ctrl + N", "New Project"},
            {"Ctrl + R", "Resize Canvas"},
            {"Ctrl + E", "Eraser Size"},
            {"Space", "Play/Stop Animation"},
            {"G", "Toggle Grid"},
            {"O", "Toggle Onion Skin"},
            {"Left/Right", "Previous/Next Frame"},
            {"Tab", "Switch between inputs"},
            {"Esc", "Close dialogs"}};

        float y = dialogPos.y + 60;
        for (const auto &shortcut : shortcuts)
        {
            // Shortcut key
            sf::Text keyText(shortcut.first, font, 14);
            keyText.setStyle(sf::Text::Bold);
            keyText.setPosition(dialogPos.x + 30, y);
            keyText.setFillColor(sf::Color(150, 255, 150)); // Soft green
            window.draw(keyText);

            // Description
            sf::Text descText(shortcut.second, font, 14);
            descText.setPosition(dialogPos.x + 200, y);
            descText.setFillColor(sf::Color(220, 220, 255)); // Light blue-white
            window.draw(descText);

            y += 23;
        }

        // Close button
        drawButton(window, sf::FloatRect(dialogPos.x + size.x - 120, dialogPos.y + size.y - 40, 100, 30),
                   font, "CLOSE", false, false);
    }

    bool handleClick(sf::Vector2i mousePos, sf::RenderWindow &window)
    {
        if (!isOpen)
            return false;

        sf::Vector2u winSize = window.getSize();
        sf::Vector2f dialogPos(winSize.x / 2 - size.x / 2, winSize.y / 2 - size.y / 2);

        // Check close button
        sf::FloatRect closeBtn(dialogPos.x + size.x - 120, dialogPos.y + size.y - 40, 100, 30);
        if (closeBtn.contains((float)mousePos.x, (float)mousePos.y))
        {
            isOpen = false;
            return true;
        }

        // Click outside dialog to close
        sf::FloatRect dialogRect(dialogPos.x, dialogPos.y, size.x, size.y);
        if (!dialogRect.contains((float)mousePos.x, (float)mousePos.y))
        {
            isOpen = false;
            return true;
        }

        return false;
    }

private:
    void drawButton(sf::RenderWindow &w, const sf::FloatRect &rect, const sf::Font &font,
                    const std::string &label, bool isActive, bool isHovered)
    {
        sf::Color bgColor = isActive ? sf::Color(UIColorTheme::ButtonActive.r, UIColorTheme::ButtonActive.g, UIColorTheme::ButtonActive.b) : isHovered ? sf::Color(UIColorTheme::ButtonHover.r, UIColorTheme::ButtonHover.g, UIColorTheme::ButtonHover.b)
                                                                                                                                                       : sf::Color(UIColorTheme::ButtonNormal.r, UIColorTheme::ButtonNormal.g, UIColorTheme::ButtonNormal.b);

        sf::RectangleShape rs({rect.width, rect.height});
        rs.setPosition(rect.left, rect.top);
        rs.setFillColor(bgColor);
        rs.setOutlineColor(sf::Color(140, 200, 255));
        rs.setOutlineThickness(2);
        w.draw(rs);

        sf::Text t(label, font, 12);
        t.setStyle(sf::Text::Bold);
        sf::FloatRect textBounds = t.getLocalBounds();
        t.setPosition(rect.left + (rect.width - textBounds.width) / 2,
                      rect.top + (rect.height - textBounds.height) / 2 - 2);
        t.setFillColor(sf::Color(UIColorTheme::SoftWhite.r, UIColorTheme::SoftWhite.g, UIColorTheme::SoftWhite.b));

        w.draw(t);
    }
};
struct Action
{
    enum Type
    {
        Draw,
        Fill,
        Erase
    };
    Type type;
    std::vector<std::pair<sf::Vector2i, sf::Color>> pixels; // Before state
    sf::Color newColor;                                     // For fill actions
    sf::Vector2i fillPos;                                   // For fill actions

    // Add constructor that takes Type
    Action(Type t) : type(t) {}

    // Default constructor
    Action() : type(Draw) {}
};

struct FrameHistory
{
    std::vector<Action> undoStack;
    std::vector<Action> redoStack;
    static const int MAX_HISTORY = 100; // Make it static

    // Add default constructor
    FrameHistory() = default;

    // Add copy constructor
    FrameHistory(const FrameHistory &other)
        : undoStack(other.undoStack), redoStack(other.redoStack) {}

    // Add assignment operator
    FrameHistory &operator=(const FrameHistory &other)
    {
        if (this != &other)
        {
            undoStack = other.undoStack;
            redoStack = other.redoStack;
        }
        return *this;
    }
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
    std::string currentFilename;

    // Undo/Redo methods
    bool canUndo() const
    {
        if (frames.empty() || currentFrame < 0 || currentFrame >= (int)frameHistories.size())
            return false;
        return !frameHistories[currentFrame].undoStack.empty();
    }

    bool canRedo() const
    {
        if (frames.empty() || currentFrame < 0 || currentFrame >= (int)frameHistories.size())
            return false;
        return !frameHistories[currentFrame].redoStack.empty();
    }

    bool undo()
    {
        if (frames.empty() || currentFrame < 0 || currentFrame >= (int)frameHistories.size())
            return false;

        auto &history = frameHistories[currentFrame];
        if (history.undoStack.empty())
            return false;

        Action action = history.undoStack.back();
        history.undoStack.pop_back();

        // Perform undo based on action type
        Frame &frame = frames[currentFrame];
        switch (action.type)
        {
        case Action::Draw:
        case Action::Erase:
            // Restore previous pixel states
            for (const auto &pixel : action.pixels)
            {
                if (pixel.first.x >= 0 && pixel.first.x < (int)width &&
                    pixel.first.y >= 0 && pixel.first.y < (int)height)
                {
                    frame.setPixel(pixel.first.x, pixel.first.y, pixel.second);
                }
            }
            break;
        case Action::Fill:
            // For fill, we stored the original color at the fill position
            if (!action.pixels.empty())
            {
                const auto &original = action.pixels[0];
                if (original.first.x >= 0 && original.first.x < (int)width &&
                    original.first.y >= 0 && original.first.y < (int)height)
                {
                    // Re-flood fill with the original color
                    floodFill(original.first.x, original.first.y, original.second);
                }
            }
            break;
        }

        // Add to redo stack
        history.redoStack.push_back(action);
        frame.updateThumbnail();
        return true;
    }

    bool redo()
    {
        if (frames.empty() || currentFrame < 0 || currentFrame >= (int)frameHistories.size())
            return false;

        auto &history = frameHistories[currentFrame];
        if (history.redoStack.empty())
            return false;

        Action action = history.redoStack.back();
        history.redoStack.pop_back();

        // Perform redo based on action type
        Frame &frame = frames[currentFrame];
        switch (action.type)
        {
        case Action::Draw:
            // Re-apply the drawing
            for (const auto &pixel : action.pixels)
            {
                if (pixel.first.x >= 0 && pixel.first.x < (int)width &&
                    pixel.first.y >= 0 && pixel.first.y < (int)height)
                {
                    frame.setPixel(pixel.first.x, pixel.first.y, action.newColor);
                }
            }
            break;
        case Action::Erase:
            // Re-apply erasure (transparent)
            for (const auto &pixel : action.pixels)
            {
                if (pixel.first.x >= 0 && pixel.first.x < (int)width &&
                    pixel.first.y >= 0 && pixel.first.y < (int)height)
                {
                    frame.setPixel(pixel.first.x, pixel.first.y, sf::Color(0, 0, 0, 0));
                }
            }
            break;
        case Action::Fill:
            // Re-apply the fill
            if (action.fillPos.x >= 0 && action.fillPos.x < (int)width &&
                action.fillPos.y >= 0 && action.fillPos.y < (int)height)
            {
                floodFill(action.fillPos.x, action.fillPos.y, action.newColor);
            }
            break;
        }

        // Add back to undo stack
        history.undoStack.push_back(action);
        frame.updateThumbnail();
        return true;
    }

    void clearRedoStack(int frameIndex)
    {
        if (frameIndex >= 0 && frameIndex < (int)frameHistories.size())
        {
            frameHistories[frameIndex].redoStack.clear();
        }
    }

    // Save methods
    bool saveProject()
    {
        if (currentFilename.empty())
        {
            return saveToPix("project.pix");
        }
        return saveToPix(currentFilename);
    }

    bool saveProjectAs(const std::string &filename)
    {
        if (saveToPix(filename))
        {
            currentFilename = filename; // Update current filename to the new one
            return true;
        }
        return false;
    }

    Canvas(unsigned w = 64, unsigned h = 64) : width(w), height(h)
    {
        if (w == 0 || h == 0 || w > 2048 || h > 2048)
        {
            std::cerr << "Invalid canvas dimensions: " << w << "x" << h << std::endl;
            width = 64;
            height = 64;
        }
        frames.emplace_back(width, height, "Frame 0");
        frameHistories.push_back(FrameHistory()); // Add history for first frame
    }

    // Add this method for single pixel drawing with history
    void setPixelAtCurrentFrame(int x, int y, const sf::Color &c)
    {
        if (x < 0 || y < 0 || x >= (int)width || y >= (int)height || frames.empty())
            return;

        Frame &f = frames[currentFrame];
        sf::Color oldColor = f.getPixel(x, y);

        // Only record if the color is actually changing
        if (oldColor != c)
        {
            std::vector<std::pair<sf::Vector2i, sf::Color>> changedPixels;
            changedPixels.push_back({{x, y}, oldColor});

            Action::Type actionType = (c.a == 0) ? Action::Erase : Action::Draw;
            Action action(actionType); // Use new constructor
            action.pixels = changedPixels;
            action.newColor = c;

            auto &history = frameHistories[currentFrame];
            history.redoStack.clear();
            history.undoStack.push_back(action);

            if (history.undoStack.size() > history.MAX_HISTORY)
            {
                history.undoStack.erase(history.undoStack.begin());
            }

            f.setPixel(x, y, c);
        }
    }
    // Multi-pixel drawing for eraser size
    void setPixelsAtCurrentFrame(int centerX, int centerY, const sf::Color &c, int size)
    {
        if (frames.empty())
            return;

        Frame &f = frames[currentFrame];
        int halfSize = size / 2;
        std::vector<std::pair<sf::Vector2i, sf::Color>> changedPixels;

        // Record all pixels that will be changed
        for (int y = centerY - halfSize; y <= centerY + halfSize; ++y)
        {
            for (int x = centerX - halfSize; x <= centerX + halfSize; ++x)
            {
                if (x >= 0 && y >= 0 && x < (int)width && y < (int)height)
                {
                    sf::Color oldColor = f.getPixel(x, y);
                    if (oldColor != c)
                    {
                        changedPixels.push_back({{x, y}, oldColor});
                    }
                }
            }
        }

        // Only push action if pixels actually changed
        if (!changedPixels.empty())
        {
            Action::Type actionType = (c.a == 0) ? Action::Erase : Action::Draw;
            Action action(actionType);
            action.pixels = changedPixels;
            action.newColor = c;

            auto &history = frameHistories[currentFrame];
            history.redoStack.clear();
            history.undoStack.push_back(action);

            if (history.undoStack.size() > history.MAX_HISTORY)
            {
                history.undoStack.erase(history.undoStack.begin());
            }

            // Apply the changes
            for (const auto &pixel : changedPixels)
            {
                f.setPixel(pixel.first.x, pixel.first.y, c);
            }
            f.updateThumbnail();
        }
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
        frameHistories.clear(); // Clear histories
        frames.emplace_back(w, h, "Frame 0");
        frameHistories.push_back(FrameHistory()); // Add history for first frame
        currentFrame = 0;
        zoom = 8.0f;
        pan = {0, 0};
        eraserSize = 1;
        currentFilename = "";
    }

    void addFrame()
    {
        if (frames.size() >= 1000)
        {
            std::cerr << "Maximum frame limit reached!" << std::endl;
            return;
        }
        frames.emplace_back(width, height, "Frame " + std::to_string(frames.size()));
        frameHistories.push_back(FrameHistory()); // Add history for new frame
        currentFrame = (int)frames.size() - 1;
    }

    void duplicateFrame()
    {
        if (frames.empty())
            return;

        if (frames.size() >= 1000)
        {
            std::cerr << "Maximum frame limit reached!" << std::endl;
            return;
        }

        Frame newFrame = frames[currentFrame];
        newFrame.name = frames[currentFrame].name + " copy";
        frames.insert(frames.begin() + currentFrame + 1, newFrame);
        frameHistories.insert(frameHistories.begin() + currentFrame + 1, FrameHistory()); // Duplicate history
        currentFrame++;
    }

    void deleteFrame(int index)
    {
        if (frames.size() <= 1 || index < 0 || index >= (int)frames.size())
            return;

        // Delete the specified frame and its history
        frames.erase(frames.begin() + index);
        frameHistories.erase(frameHistories.begin() + index);

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

            // Manual swap for frameHistories
            FrameHistory temp = frameHistories[currentFrame];
            frameHistories[currentFrame] = frameHistories[currentFrame - 1];
            frameHistories[currentFrame - 1] = temp;

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

            // Manual swap for frameHistories
            FrameHistory temp = frameHistories[currentFrame];
            frameHistories[currentFrame] = frameHistories[currentFrame + 1];
            frameHistories[currentFrame + 1] = temp;

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

    void floodFill(int sx, int sy, const sf::Color &newColor)
    {
        if (sx < 0 || sy < 0 || sx >= (int)width || sy >= (int)height || frames.empty())
            return;

        Frame &f = frames[currentFrame];
        sf::Color target = f.getPixel(sx, sy);
        if (target == newColor)
            return;

        // Record the original state before filling
        std::vector<std::pair<sf::Vector2i, sf::Color>> originalPixels;

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

            // Record original pixel before changing
            originalPixels.push_back({{x, y}, target});
            f.setPixel(x, y, newColor);

            stack.emplace_back(x + 1, y);
            stack.emplace_back(x - 1, y);
            stack.emplace_back(x, y + 1);
            stack.emplace_back(x, y - 1);
        }

        // Push fill action to history
        if (!originalPixels.empty())
        {
            pushFillAction(currentFrame, {sx, sy}, target, newColor);
        }

        f.updateThumbnail();
    }

    bool exportAsGIF(const std::string &filename, int frameDelay = 5, int gifWidth = -1, int gifHeight = -1) const
    {
        if (frames.empty())
            return false;

        try
        {
            // Create directory if it doesn't exist
            std::string dir = filename.substr(0, filename.find_last_of("/\\"));
            if (!dir.empty() && !createDirectory(dir))
            {
                std::cerr << "Failed to create directory: " << dir << std::endl;
            }

            // Use provided dimensions or default to canvas size
            int exportWidth = (gifWidth > 0) ? gifWidth : width;
            int exportHeight = (gifHeight > 0) ? gifHeight : height;

            SimpleGIFEncoder gif(filename, exportWidth, exportHeight, frameDelay);
            for (const auto &frame : frames)
            {
                // Create scaled version of the frame if dimensions are different
                sf::Image exportImage;
                exportImage.create(exportWidth, exportHeight, sf::Color(0, 0, 0, 0));

                // Scale the image using nearest-neighbor for pixel art
                for (int y = 0; y < exportHeight; ++y)
                {
                    for (int x = 0; x < exportWidth; ++x)
                    {
                        int srcX = (x * width) / exportWidth;
                        int srcY = (y * height) / exportHeight;
                        if (srcX < (int)width && srcY < (int)height)
                        {
                            exportImage.setPixel(x, y, frame.image.getPixel(srcX, srcY));
                        }
                    }
                }

                if (!gif.addFrame(exportImage))
                {
                    std::cerr << "Failed to add frame to GIF" << std::endl;
                    return false;
                }
            }
            return true;
        }
        catch (const std::exception &e)
        {
            std::cerr << "GIF export error: " << e.what() << std::endl;
            return false;
        }
        catch (...)
        {
            std::cerr << "Unknown GIF export error" << std::endl;
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

    // Save and load .pix custom format
    bool saveToPix(const std::string &filename)
    {
        std::ofstream ofs(filename, std::ios::binary);
        if (!ofs)
            return false;

        // Write header
        ofs.write("PIX1", 4); // magic

        auto write32 = [&](u32 v)
        {
            char bytes[4];
            bytes[0] = (v >> 0) & 0xFF;
            bytes[1] = (v >> 8) & 0xFF;
            bytes[2] = (v >> 16) & 0xFF;
            bytes[3] = (v >> 24) & 0xFF;
            ofs.write(bytes, 4);
        };

        write32(width);
        write32(height);
        write32((u32)frames.size());

        for (const Frame &f : frames)
        {
            // Write frame name
            u32 nameLen = (u32)f.name.size();
            write32(nameLen);
            ofs.write(f.name.c_str(), nameLen);

            // Write frame pixels (RGBA)
            const sf::Image &img = f.image;
            auto size = img.getSize();

            // Convert pixels to byte array
            std::vector<sf::Uint8> pixels(size.x * size.y * 4);
            for (unsigned y = 0; y < size.y; ++y)
            {
                for (unsigned x = 0; x < size.x; ++x)
                {
                    sf::Color pixel = img.getPixel(x, y);
                    unsigned index = (y * size.x + x) * 4;
                    pixels[index] = pixel.r;
                    pixels[index + 1] = pixel.g;
                    pixels[index + 2] = pixel.b;
                    pixels[index + 3] = pixel.a;
                }
            }

            ofs.write((char *)pixels.data(), pixels.size());
        }

        currentFilename = filename; // NEW: Update current filename
        return true;
    }

    bool loadFromPix(const std::string &filename)
    {
        std::ifstream ifs(filename, std::ios::binary);
        if (!ifs)
            return false;

        // Read and verify magic
        char magic[5] = {0};
        ifs.read(magic, 4);
        if (std::string(magic) != "PIX1")
            return false;

        auto read32 = [&]() -> u32
        {
            char bytes[4];
            ifs.read(bytes, 4);
            if (ifs.gcount() != 4)
                return 0;
            return (u32)((bytes[0] << 0) | (bytes[1] << 8) | (bytes[2] << 16) | (bytes[3] << 24));
        };

        u32 w = read32(), h = read32();
        u32 fcount = read32();

        // Validate dimensions
        if (w == 0 || h == 0 || w > 2048 || h > 2048 || fcount > 1000)
        {
            std::cerr << "Invalid file format or dimensions too large!" << std::endl;
            return false;
        }

        frames.clear();
        frameHistories.clear();
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

            // Read pixel data
            std::vector<sf::Uint8> pixels(width * height * 4);
            ifs.read((char *)pixels.data(), pixels.size());

            if (ifs.gcount() != (std::streamsize)pixels.size())
            {
                std::cerr << "Failed to read pixel data for frame " << fi << std::endl;
                return false;
            }

            // Create image from pixel data
            f.image.create(width, height, pixels.data());
            f.updateThumbnail();
            frames.push_back(std::move(f));
            frameHistories.push_back(FrameHistory());
        }

        currentFrame = 0;
        eraserSize = 1;             // Reset eraser size
        currentFilename = filename; // NEW: Update current filename
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

private:
    std::vector<FrameHistory> frameHistories;

    void pushDrawAction(int frameIndex, const std::vector<std::pair<sf::Vector2i, sf::Color>> &changedPixels)
    {
        if (frameIndex < 0 || frameIndex >= (int)frameHistories.size())
            return;

        auto &history = frameHistories[frameIndex];

        // Clear redo stack when new action is performed
        history.redoStack.clear();

        // Add to undo stack
        Action action(Action::Draw); // Use new constructor
        action.pixels = changedPixels;
        history.undoStack.push_back(action);

        // Limit history size
        if (history.undoStack.size() > history.MAX_HISTORY)
        {
            history.undoStack.erase(history.undoStack.begin());
        }
    }

    void pushFillAction(int frameIndex, const sf::Vector2i &pos, const sf::Color &oldColor, const sf::Color &newColor)
    {
        if (frameIndex < 0 || frameIndex >= (int)frameHistories.size())
            return;

        auto &history = frameHistories[frameIndex];
        history.redoStack.clear();

        Action action(Action::Fill); // Use new constructor
        action.fillPos = pos;
        action.newColor = newColor;
        // Store the old color at the fill position
        action.pixels.push_back({pos, oldColor});
        history.undoStack.push_back(action);

        if (history.undoStack.size() > history.MAX_HISTORY)
        {
            history.undoStack.erase(history.undoStack.begin());
        }
    }
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
};
class ColorWheel
{
public:
    bool isOpen = false;
    sf::Vector2f position{520, 80};
    sf::Vector2f size{300, 350};
    Color currentColor{255, 0, 0, 255};
    sf::Image wheelImage;
    sf::Texture wheelTexture;
    bool needsUpdate = true;

    ColorWheel()
    {
        wheelImage.create(256, 256, sf::Color::White);
        updateWheel();
    }

    void updateWheel()
    {
        if (!needsUpdate)
            return;

        int centerX = 128;
        int centerY = 128;
        int radius = 120;

        for (int y = 0; y < 256; y++)
        {
            for (int x = 0; x < 256; x++)
            {
                int dx = x - centerX;
                int dy = y - centerY;
                float distance = std::sqrt(dx * dx + dy * dy);

                if (distance <= radius)
                {
                    float angle = std::atan2(dy, dx);
                    if (angle < 0)
                        angle += 2 * M_PI;

                    float hue = angle / (2 * M_PI);
                    float saturation = distance / radius;
                    float value = 1.0f;

                    // HSV to RGB conversion
                    int hi = static_cast<int>(hue * 6);
                    float f = hue * 6 - hi;
                    float p = value * (1 - saturation);
                    float q = value * (1 - f * saturation);
                    float t = value * (1 - (1 - f) * saturation);

                    float r, g, b;
                    switch (hi)
                    {
                    case 0:
                        r = value;
                        g = t;
                        b = p;
                        break;
                    case 1:
                        r = q;
                        g = value;
                        b = p;
                        break;
                    case 2:
                        r = p;
                        g = value;
                        b = t;
                        break;
                    case 3:
                        r = p;
                        g = q;
                        b = value;
                        break;
                    case 4:
                        r = t;
                        g = p;
                        b = value;
                        break;
                    case 5:
                        r = value;
                        g = p;
                        b = q;
                        break;
                    default:
                        r = g = b = 0;
                        break;
                    }

                    wheelImage.setPixel(x, y, sf::Color(static_cast<sf::Uint8>(r * 255), static_cast<sf::Uint8>(g * 255), static_cast<sf::Uint8>(b * 255)));
                }
                else
                {
                    wheelImage.setPixel(x, y, sf::Color::Transparent);
                }
            }
        }

        wheelTexture.loadFromImage(wheelImage);
        needsUpdate = false;
    }

    void draw(sf::RenderWindow &window, sf::Font &font)
    {
        if (!isOpen)
            return;

        updateWheel();

        // Background with 8-bit style
        sf::RectangleShape bg(size);
        bg.setPosition(position);
        bg.setFillColor(sf::Color(40, 40, 80));     // Dark blue-purple
        bg.setOutlineColor(sf::Color(255, 255, 0)); // Yellow
        bg.setOutlineThickness(2);
        window.draw(bg);

        // Title
        sf::Text title("COLOR", font, 16);
        title.setStyle(sf::Text::Bold);
        title.setPosition(position.x + 10, position.y + 10);
        title.setFillColor(sf::Color(255, 255, 0)); // Yellow
        window.draw(title);

        // Draw color wheel
        sf::Sprite wheelSprite(wheelTexture);
        wheelSprite.setPosition(position.x + (size.x - 256) / 2, position.y + 70);
        window.draw(wheelSprite);

        // Current color preview
        sf::RectangleShape preview({80, 60});
        preview.setPosition(position.x + size.x - 90, position.y + 10);
        preview.setFillColor(sf::Color(currentColor.r, currentColor.g, currentColor.b));
        preview.setOutlineColor(sf::Color::White);
        preview.setOutlineThickness(2);
        window.draw(preview);

        // RGB values
        sf::Text rgbText("RGB: " + std::to_string(currentColor.r) + ", " +
                             std::to_string(currentColor.g) + ", " +
                             std::to_string(currentColor.b),
                         font, 12);
        rgbText.setPosition(position.x + size.x - 290, position.y + 40);
        rgbText.setFillColor(sf::Color::White);
        window.draw(rgbText);

        // Close button
        drawButton(window, sf::FloatRect(position.x + size.x - 90, position.y + size.y - 35, 80, 25),
                   font, "CLOSE", false, false);
    }

    bool handleClick(sf::Vector2i mousePos, Color &targetColor)
    {
        if (!isOpen)
            return false;

        // Check color wheel clicks
        sf::FloatRect wheelRect(position.x + (size.x - 256) / 2, position.y + 40, 256, 256);
        if (wheelRect.contains((float)mousePos.x, (float)mousePos.y))
        {
            int localX = mousePos.x - wheelRect.left;
            int localY = mousePos.y - wheelRect.top;

            if (localX >= 0 && localX < 256 && localY >= 0 && localY < 256)
            {
                sf::Color pixel = wheelImage.getPixel(localX, localY);
                if (pixel.a > 0) // Not transparent
                {
                    currentColor = Color(pixel.r, pixel.g, pixel.b);
                    targetColor = currentColor;
                    return true;
                }
            }
        }

        // Check close button
        sf::FloatRect closeBtn(position.x + size.x - 90, position.y + size.y - 35, 80, 25);
        if (closeBtn.contains((float)mousePos.x, (float)mousePos.y))
        {
            isOpen = false;
            return true;
        }

        return false;
    }

private:
    void drawButton(sf::RenderWindow &w, const sf::FloatRect &rect, const sf::Font &font,
                    const std::string &label, bool isActive, bool isHovered)
    {
        sf::Color bgColor = isActive ? sf::Color(0, 0, 255) : sf::Color(128, 0, 128);
        if (isHovered && !isActive)
            bgColor = sf::Color(0, 0, 168);

        sf::RectangleShape rs({rect.width, rect.height});
        rs.setPosition(rect.left, rect.top);
        rs.setFillColor(bgColor);
        rs.setOutlineColor(isActive ? sf::Color(255, 255, 0) : sf::Color(170, 170, 170));
        rs.setOutlineThickness(2);
        w.draw(rs);

        sf::Text t(label, font, 12);
        t.setStyle(sf::Text::Bold);
        sf::FloatRect textBounds = t.getLocalBounds();
        t.setPosition(rect.left + (rect.width - textBounds.width) / 2,
                      rect.top + (rect.height - textBounds.height) / 2 - 2);
        t.setFillColor(sf::Color::White);
        w.draw(t);
    }
};

void drawButton(sf::RenderWindow &w, const sf::FloatRect &rect, const sf::Font &font,
                const std::string &label, bool isActive = false, bool isHovered = false)
{
    // Improved button colors with better contrast
    sf::Color bgColor = isActive ? sf::Color(0, 0, 255) : // Blue when active
                            isHovered ? sf::Color(0, 0, 168)
                                      :             // Dark blue when hovered
                            sf::Color(128, 0, 128); // Purple by default

    sf::RectangleShape rs({rect.width, rect.height});
    rs.setPosition(rect.left, rect.top);
    rs.setFillColor(bgColor);
    rs.setOutlineColor(isActive ? sf::Color(255, 255, 0) : // Yellow border when active
                           sf::Color(200, 200, 200));      // Light gray otherwise
    rs.setOutlineThickness(2);
    w.draw(rs);

    // 8-bit text style with shadow for better readability
    sf::Text t(label, font, 12);
    t.setStyle(sf::Text::Bold);
    sf::FloatRect textBounds = t.getLocalBounds();
    t.setPosition(rect.left + (rect.width - textBounds.width) / 2,
                  rect.top + (rect.height - textBounds.height) / 2 - 2);
    t.setFillColor(sf::Color::White);
    w.draw(t);
}
// Draw 8-bit style panel with gradient effect
void drawPanel(sf::RenderWindow &w, const sf::FloatRect &rect, const std::string &title = "", const sf::Font &font = sf::Font())
{
    // Main panel with gradient-like effect using multiple rectangles
    sf::RectangleShape panel({rect.width, rect.height});
    panel.setPosition(rect.left, rect.top);
    panel.setFillColor(sf::Color(40, 40, 80)); // Dark blue-purple base

    // Top highlight
    sf::RectangleShape topHighlight({rect.width, 2});
    topHighlight.setPosition(rect.left, rect.top);
    topHighlight.setFillColor(sf::Color(100, 100, 200));

    // Bottom shadow
    sf::RectangleShape bottomShadow({rect.width, 2});
    bottomShadow.setPosition(rect.left, rect.top + rect.height - 2);
    bottomShadow.setFillColor(sf::Color(20, 20, 60));

    // Border
    panel.setOutlineColor(sf::Color(170, 170, 170)); // Light gray
    panel.setOutlineThickness(2);

    w.draw(panel);
    w.draw(topHighlight);
    w.draw(bottomShadow);

    // Title if provided
    if (!title.empty() && font.getInfo().family != "")
    {
        sf::Text titleText(title, font, 14);
        titleText.setStyle(sf::Text::Bold);
        titleText.setPosition(rect.left + 10, rect.top + 5);
        titleText.setFillColor(sf::Color(255, 255, 0)); // Yellow
        w.draw(titleText);
    }
}

int main()
{
    // NEW: GIF export variables
    std::string gifWidthStr = std::to_string(64);
    std::string gifHeightStr = std::to_string(64);
    std::string gifDelayStr = "5";

    bool ctrlPressed = false;
    bool undoPressed = false;
    bool redoPressed = false;

    bool gifWidthInputActive = false;
    bool gifHeightInputActive = false;
    bool gifDelayInputActive = false;

    bool showSaveDialog = false;
    bool showOpenDialog = false;
    std::string saveFilename = "project.pix";
    std::string openFilename = "project.pix";

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

    ColorWheel colorWheel;

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

    // NEW: File browser
    FileBrowser fileBrowser;

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

    // NEW: Double click tracking for file browser
    sf::Clock doubleClickClock;
    sf::Vector2i lastClickPos;

    sf::Clock clock;
    HelpDialog helpDialog;
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

                // If file browser is open, only process events relevant to it
                if (fileBrowser.isOpen &&
                    ev.type != sf::Event::MouseButtonPressed &&
                    ev.type != sf::Event::MouseButtonReleased &&
                    ev.type != sf::Event::KeyPressed &&
                    ev.type != sf::Event::TextEntered)
                {
                    continue; // Skip other events when file browser is open
                }
                // Handle file browser events first if it's open
                if (fileBrowser.isOpen)
                {
                    bool fileBrowserHandledEvent = fileBrowser.handleEvent(ev, sf::Mouse::getPosition(window), window);

                    if (fileBrowserHandledEvent)
                    {
                        // Check for double click
                        if (ev.type == sf::Event::MouseButtonPressed && ev.mouseButton.button == sf::Mouse::Left)
                        {
                            sf::Vector2i currentClickPos = sf::Mouse::getPosition(window);

                            if (doubleClickClock.getElapsedTime().asMilliseconds() < 500 &&
                                std::abs(currentClickPos.x - lastClickPos.x) < 10 &&
                                std::abs(currentClickPos.y - lastClickPos.y) < 10)
                            {
                                fileBrowser.doubleClick(currentClickPos, window);
                            }

                            doubleClickClock.restart();
                            lastClickPos = currentClickPos;
                        }

                        // If a file was selected and OK was clicked, handle the operation
                        if (!fileBrowser.selectedFile.empty() && fileBrowser.selectedFile != ".." &&
                            std::find(fileBrowser.directories.begin(), fileBrowser.directories.end(), fileBrowser.selectedFile) == fileBrowser.directories.end())
                        {
                            std::string fullPath = fileBrowser.currentPath;
#ifdef _WIN32
                            fullPath += "\\" + fileBrowser.selectedFile;
#else
                            fullPath += "/" + fileBrowser.selectedFile;
#endif

                            // Check which type of operation this is based on the browser title
                            if (fileBrowser.title.find("Export GIF") != std::string::npos)
                            {
                                // GIF Export operation
                                try
                                {
                                    int width = std::stoi(gifWidthStr);
                                    int height = std::stoi(gifHeightStr);
                                    int delay = std::stoi(gifDelayStr);
                                    if (canvas.exportAsGIF(fullPath, delay, width, height))
                                    {
                                        exportStatus = "Exported GIF: " + fullPath;
                                        exportStatusTimer = 3.0f;
                                    }
                                    else
                                    {
                                        exportStatus = "Failed to export GIF";
                                        exportStatusTimer = 3.0f;
                                    }
                                }
                                catch (...)
                                {
                                    exportStatus = "Invalid GIF values";
                                    exportStatusTimer = 3.0f;
                                }
                            }
                            else if (fileBrowser.title.find("Export PNG") != std::string::npos)
                            {
                                // PNG Export operation
                                if (canvas.exportCurrentFramePNG(fullPath))
                                {
                                    exportStatus = "Exported PNG: " + fullPath;
                                    exportStatusTimer = 3.0f;
                                }
                                else
                                {
                                    exportStatus = "Failed to export PNG";
                                    exportStatusTimer = 3.0f;
                                }
                            }

                            else if (fileBrowser.title.find("Save Project As") != std::string::npos) // === ADD THIS NEW CASE ===
                            {
                                // Save Project As operation
                                if (canvas.saveProjectAs(fullPath))
                                {
                                    exportStatus = "Saved as " + fullPath;
                                    exportStatusTimer = 3.0f;
                                }
                                else
                                {
                                    exportStatus = "Failed to save " + fullPath;
                                    exportStatusTimer = 3.0f;
                                }
                            }
                            else if (fileBrowser.title.find("Open Project") != std::string::npos)
                            {
                                // Open Project operation
                                if (canvas.loadFromPix(fullPath))
                                {
                                    exportStatus = "Opened " + fullPath;
                                    exportStatusTimer = 3.0f;
                                }
                                else
                                {
                                    exportStatus = "Failed to open " + fullPath;
                                    exportStatusTimer = 3.0f;
                                }
                            }

                            fileBrowser.isOpen = false;
                            fileBrowser.selectedFile.clear(); // Clear selection after operation
                        }
                        // Mark that the file browser handled this event
                        uiElementClicked = true;
                        continue; // Skip further event processing for this event
                    }
                }
                else
                {
                    // Original event handling when file browser is not open
                    if (ev.type == sf::Event::MouseWheelScrolled)
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
                    else if (ev.type == sf::Event::KeyReleased)
                    {
                        if (ev.key.code == sf::Keyboard::LControl || ev.key.code == sf::Keyboard::RControl)
                        {
                            ctrlPressed = false;
                        }
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
                        if (ctrl && ev.key.code == sf::Keyboard::Z)
                        {
                            if (canvas.undo())
                            {
                                // Successfully performed undo
                            }
                            undoPressed = true;
                        }
                        else if (ctrl && ev.key.code == sf::Keyboard::Y)
                        {
                            if (canvas.redo())
                            {
                                // Successfully performed redo
                            }
                            redoPressed = true;
                        }

                        else if (ctrl && ev.key.code == sf::Keyboard::S)
                        {
                            // Quick save with current filename
                            if (canvas.currentFilename.empty())
                            {
                                fileBrowser.isOpen = true;
                                fileBrowser.title = "Save Project As";
                                fileBrowser.defaultExtension = ".pix";
                                fileBrowser.allowedExtensions = {".pix"};
                                fileBrowser.filenameInput = "project.pix";
                                fileBrowser.filenameInputActive = true;
                            }
                            else
                            {
                                if (canvas.saveProject())
                                {
                                    exportStatus = "Saved " + canvas.currentFilename;
                                    exportStatusTimer = 3.0f;
                                }
                                else
                                {
                                    exportStatus = "Failed to save " + canvas.currentFilename;
                                    exportStatusTimer = 3.0f;
                                }
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
                            else if (showGIFExportDialog)
                            {
                                if (gifWidthInputActive)
                                {
                                    gifWidthInputActive = false;
                                    gifHeightInputActive = true;
                                }
                                else if (gifHeightInputActive)
                                {
                                    gifHeightInputActive = false;
                                    gifDelayInputActive = true;
                                }
                                else
                                {
                                    gifDelayInputActive = false;
                                    gifWidthInputActive = true;
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
                                // Show file browser for GIF export location
                                fileBrowser.isOpen = true;
                                fileBrowser.title = "Export GIF";
                                fileBrowser.defaultExtension = ".gif";
                                fileBrowser.allowedExtensions = {".gif"};
                                fileBrowser.filenameInput = "animation.gif";
                                fileBrowser.filenameInputActive = true;
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
                            else if (helpDialog.isOpen)
                            {
                                helpDialog.isOpen = false;
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
                            else if (showSaveDialog)
                            {
                                showSaveDialog = false;
                            }
                            else if (showOpenDialog)
                            {
                                showOpenDialog = false;
                            }
                            else if (colorWheel.isOpen)
                            {
                                colorWheel.isOpen = false;
                            }
                            else if (fileBrowser.isOpen)
                            {
                                fileBrowser.isOpen = false;
                            }
                        }

                        else if (ctrl && ev.key.code == sf::Keyboard::O)
                        {
                            // NEW: Open project
                            fileBrowser.isOpen = true;
                            fileBrowser.title = "Open Project";
                            fileBrowser.defaultExtension = ".pix";
                            fileBrowser.allowedExtensions = {".pix"};
                            fileBrowser.filenameInput = canvas.currentFilename.empty() ? "project.pix" : canvas.currentFilename;
                            fileBrowser.filenameInputActive = false;
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
                                if (gifWidthInputActive && !gifWidthStr.empty())
                                    gifWidthStr.pop_back();
                                else if (gifHeightInputActive && !gifHeightStr.empty())
                                    gifHeightStr.pop_back();
                                else if (gifDelayInputActive && !gifDelayStr.empty())
                                    gifDelayStr.pop_back();
                            }
                            else if (ev.text.unicode >= '0' && ev.text.unicode <= '9')
                            {
                                if (gifWidthInputActive)
                                {
                                    if (gifWidthStr.length() < 4)
                                        gifWidthStr += static_cast<char>(ev.text.unicode);
                                }
                                else if (gifHeightInputActive)
                                {
                                    if (gifHeightStr.length() < 4)
                                        gifHeightStr += static_cast<char>(ev.text.unicode);
                                }
                                else if (gifDelayInputActive)
                                {
                                    if (gifDelayStr.length() < 3)
                                        gifDelayStr += static_cast<char>(ev.text.unicode);
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

            // Handle color wheel clicks - ONLY on mouse press, not hold
            if (leftMousePressedThisFrame && !uiElementClicked && !fileBrowser.isOpen)
            {
                if (colorWheel.handleClick(mpos, canvas.drawColor))
                {
                    uiElementClicked = true;
                }
            }
            // Handle help dialog clicks - ONLY on mouse press, not hold
            if (leftMousePressedThisFrame && !uiElementClicked && !fileBrowser.isOpen && helpDialog.isOpen)
            {
                if (helpDialog.handleClick(mpos, window))
                {
                    uiElementClicked = true;
                }
            }
            // Drawing: convert mouse to pixel coords
            if (leftMouseDown && mouseInCanvas && !uiElementClicked && !colorWheel.isOpen && !showResizeDialog && !showEraserSizeDialog && !showGIFExportDialog && !showGodotExportDialog && !renamingFrame && !fileBrowser.isOpen)
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

            // Use a darker, more consistent background color
            window.clear(sf::Color(UIColorTheme::DarkBackground.r, UIColorTheme::DarkBackground.g, UIColorTheme::DarkBackground.b));

            // Draw main panels with 8-bit style
            drawPanel(window, sf::FloatRect(4, 4, (float)winSize.x - sidebarW - 8, toolbarH - 4), "TOOLS", font);
            drawPanel(window, sf::FloatRect(canvasArea.left - 4, canvasArea.top - 4, canvasArea.width + 8, canvasArea.height + 8), "CANVAS", font);
            drawPanel(window, sf::FloatRect(canvasArea.left + canvasArea.width + 4, 4, sidebarW - 8, (float)winSize.y - 8), "ANIMATION", font);

            // Toolbar buttons: Pencil, Eraser, Fill
            float x = 8;
            float y = 8;
            float bw = 64, bh = 32, spacing = 6;

            // Check hover states for tool buttons
            if (!uiElementClicked && !colorWheel.isOpen && !showResizeDialog && !showEraserSizeDialog && !showGIFExportDialog && !showGodotExportDialog && !renamingFrame && !fileBrowser.isOpen)
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
            if (leftMousePressedThisFrame && hoveredToolButton == 0 && !uiElementClicked && !fileBrowser.isOpen)
            {
                canvas.currentTool = Tool::Pencil;
                uiElementClicked = true;
            }

            x += bw + spacing;

            // Eraser button
            drawButton(window, sf::FloatRect(x, y, bw, bh), font, "ERASER",
                       canvas.currentTool == Tool::Eraser, hoveredToolButton == 1);
            if (leftMousePressedThisFrame && hoveredToolButton == 1 && !uiElementClicked && !fileBrowser.isOpen)
            {
                canvas.currentTool = Tool::Eraser;
                uiElementClicked = true;
            }

            x += bw + spacing;

            // Fill button
            drawButton(window, sf::FloatRect(x, y, bw, bh), font, "FILL",
                       canvas.currentTool == Tool::Fill, hoveredToolButton == 2);
            if (leftMousePressedThisFrame && hoveredToolButton == 2 && !uiElementClicked && !fileBrowser.isOpen)
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

            // Color wheel button
            bool colorBtnHovered = (mpos.x >= (int)(colorX + 40) && mpos.x <= (int)(colorX + 40 + 80) &&
                                    mpos.y >= (int)y && mpos.y <= (int)(y + bh));
            drawButton(window, sf::FloatRect(colorX + 40, y, 80, bh), font, "COLOR", false, colorBtnHovered);
            if (leftMousePressedThisFrame && colorBtnHovered && !uiElementClicked && !fileBrowser.isOpen)
            {
                colorWheel.isOpen = !colorWheel.isOpen;
                uiElementClicked = true;
            }

            // NEW: Eraser size button
            bool eraserSizeBtnHovered = (mpos.x >= (int)(colorX + 110) && mpos.x <= (int)(colorX + 110 + 80) &&
                                         mpos.y >= (int)y && mpos.y <= (int)(y + bh));
            drawButton(window, sf::FloatRect(colorX + 110, y, 80, bh), font,
                       "ERASER: " + std::to_string(canvas.eraserSize), false, eraserSizeBtnHovered);
            if (leftMousePressedThisFrame && eraserSizeBtnHovered && !uiElementClicked && !fileBrowser.isOpen)
            {
                showEraserSizeDialog = !showEraserSizeDialog;
                eraserSizeStr = std::to_string(canvas.eraserSize);
                uiElementClicked = true;
            }

            // Resize canvas button
            bool resizeBtnHovered = (mpos.x >= (int)(colorX + 200) && mpos.x <= (int)(colorX + 200 + 80) &&
                                     mpos.y >= (int)y && mpos.y <= (int)(y + bh));
            drawButton(window, sf::FloatRect(colorX + 200, y, 80, bh), font, "RESIZE", false, resizeBtnHovered);
            if (leftMousePressedThisFrame && resizeBtnHovered && !uiElementClicked && !fileBrowser.isOpen)
            {
                showResizeDialog = !showResizeDialog;
                newWidthStr = std::to_string(canvas.width);
                newHeightStr = std::to_string(canvas.height);
                widthInputActive = true;
                heightInputActive = false;
                uiElementClicked = true;
            }

            // Help button in bottom right corner
            bool helpBtnHovered = (mpos.x >= (int)(winSize.x - 40) && mpos.x <= (int)(winSize.x - 10) &&
                                   mpos.y >= (int)(winSize.y - 30) && mpos.y <= (int)(winSize.y - 10));
            drawButton(window, sf::FloatRect(winSize.x - 40, winSize.y - 30, 30, 20), font, "?", false, helpBtnHovered);
            if (leftMousePressedThisFrame && helpBtnHovered && !uiElementClicked && !fileBrowser.isOpen)
            {
                helpDialog.isOpen = !helpDialog.isOpen;
                uiElementClicked = true;
            }
            // Save button - placed after Open button
            bool saveBtnHovered = (mpos.x >= (int)(colorX + 370) && mpos.x <= (int)(colorX + 370 + 80) &&
                                   mpos.y >= (int)y && mpos.y <= (int)(y + bh));
            drawButton(window, sf::FloatRect(colorX + 370, y, 80, bh), font, "SAVE", false, saveBtnHovered);
            if (leftMousePressedThisFrame && saveBtnHovered && !uiElementClicked && !fileBrowser.isOpen)
            {
                if (canvas.currentFilename.empty())
                {
                    // No filename yet, show Save As dialog
                    fileBrowser.isOpen = true;
                    fileBrowser.title = "Save Project As";
                    fileBrowser.defaultExtension = ".pix";
                    fileBrowser.allowedExtensions = {".pix"};
                    fileBrowser.filenameInput = "project.pix";
                    fileBrowser.filenameInputActive = true;
                }
                else
                {
                    // Save to current file
                    if (canvas.saveProject())
                    {
                        exportStatus = "Saved " + canvas.currentFilename;
                        exportStatusTimer = 3.0f;
                    }
                    else
                    {
                        exportStatus = "Failed to save " + canvas.currentFilename;
                        exportStatusTimer = 3.0f;
                    }
                }
                uiElementClicked = true;
            }
            // Save As button - placed after Save button
            bool saveAsBtnHovered = (mpos.x >= (int)(colorX + 450) && mpos.x <= (int)(colorX + 450 + 80) &&
                                     mpos.y >= (int)y && mpos.y <= (int)(y + bh));
            drawButton(window, sf::FloatRect(colorX + 450, y, 80, bh), font, "SAVE AS", false, saveAsBtnHovered);
            if (leftMousePressedThisFrame && saveAsBtnHovered && !uiElementClicked && !fileBrowser.isOpen)
            {
                fileBrowser.isOpen = true;
                fileBrowser.title = "Save Project As";
                fileBrowser.defaultExtension = ".pix";
                fileBrowser.allowedExtensions = {".pix"};
                fileBrowser.filenameInput = canvas.currentFilename.empty() ? "project.pix" : canvas.currentFilename;
                fileBrowser.filenameInputActive = true;
                uiElementClicked = true;
            }
            // Open button
            bool openBtnHovered = (mpos.x >= (int)(colorX + 290) && mpos.x <= (int)(colorX + 290 + 80) &&
                                   mpos.y >= (int)y && mpos.y <= (int)(y + bh));
            drawButton(window, sf::FloatRect(colorX + 290, y, 80, bh), font, "OPEN", false, openBtnHovered);
            if (leftMousePressedThisFrame && openBtnHovered && !uiElementClicked && !fileBrowser.isOpen)
            {
                fileBrowser.isOpen = true;
                fileBrowser.title = "Open Project";
                fileBrowser.defaultExtension = ".pix";
                fileBrowser.allowedExtensions = {".pix"};
                fileBrowser.filenameInput = canvas.currentFilename.empty() ? "project.pix" : canvas.currentFilename;
                fileBrowser.filenameInputActive = false;
                // Don't set uiElementClicked = true here!
            }

            // Draw canvas background (8-bit style checkerboard)
            sf::RectangleShape canvasBg({canvasArea.width, canvasArea.height});
            canvasBg.setPosition(canvasArea.left, canvasArea.top);
            canvasBg.setFillColor(sf::Color(UIColorTheme::CanvasBg.r, UIColorTheme::CanvasBg.g, UIColorTheme::CanvasBg.b));
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
                                            sf::Color(UIColorTheme::GridLines.r, UIColorTheme::GridLines.g, UIColorTheme::GridLines.b)));
                    lines.append(sf::Vertex({canvasArea.left + canvas.pan.x + canvas.width * canvas.zoom, syp},
                                            sf::Color(UIColorTheme::GridLines.r, UIColorTheme::GridLines.g, UIColorTheme::GridLines.b)));
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
            if (leftMousePressedThisFrame && playBtnHovered && !uiElementClicked && !fileBrowser.isOpen)
            {
                playing = !playing;
                uiElementClicked = true;
            }

            // Prev frame button
            bool prevBtnHovered = (mpos.x >= (int)(sidebar.left + 76) && mpos.x <= (int)(sidebar.left + 104) &&
                                   mpos.y >= (int)controlY && mpos.y <= (int)(controlY + 28));
            drawButton(window, sf::FloatRect(sidebar.left + 76, controlY, 28, 28), font, "<", false, prevBtnHovered);
            if (leftMousePressedThisFrame && prevBtnHovered && !uiElementClicked && !fileBrowser.isOpen)
            {
                canvas.prevFrame();
                uiElementClicked = true;
            }

            // Next frame button
            bool nextBtnHovered = (mpos.x >= (int)(sidebar.left + 112) && mpos.x <= (int)(sidebar.left + 140) &&
                                   mpos.y >= (int)controlY && mpos.y <= (int)(controlY + 28));
            drawButton(window, sf::FloatRect(sidebar.left + 112, controlY, 28, 28), font, ">", false, nextBtnHovered);
            if (leftMousePressedThisFrame && nextBtnHovered && !uiElementClicked && !fileBrowser.isOpen)
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
                if (leftMousePressedThisFrame && !uiElementClicked && !renamingFrame && !fileBrowser.isOpen)
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
            if (leftMousePressedThisFrame && addFrameHovered && !renamingFrame && !uiElementClicked && !fileBrowser.isOpen)
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
            if (leftMousePressedThisFrame && gifExportHovered && !uiElementClicked && !fileBrowser.isOpen)
            {
                showGIFExportDialog = !showGIFExportDialog;
                gifWidthStr = std::to_string(canvas.width);
                gifHeightStr = std::to_string(canvas.height);
                gifDelayStr = "5";
                gifWidthInputActive = true;
                gifHeightInputActive = false;
                gifDelayInputActive = false;
                uiElementClicked = true;
            }
            // Godot Export button
            bool godotExportHovered = (mpos.x >= (int)(sidebar.left + 96) && mpos.x <= (int)(sidebar.left + 176) &&
                                       mpos.y >= (int)(exportY) && mpos.y <= (int)(exportY + 28));
            drawButton(window, sf::FloatRect(sidebar.left + 96, exportY, 80, 28), font, "GODOT", false, godotExportHovered);
            if (leftMousePressedThisFrame && godotExportHovered && !uiElementClicked && !fileBrowser.isOpen)
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
            if (leftMousePressedThisFrame && pngExportHovered && !uiElementClicked && !fileBrowser.isOpen)
            {
                fileBrowser.isOpen = true;
                fileBrowser.title = "Export PNG";
                fileBrowser.defaultExtension = ".png";
                fileBrowser.allowedExtensions = {".png"};
                fileBrowser.filenameInput = "frame.png";
                fileBrowser.filenameInputActive = true;
                // Don't set uiElementClicked = true here!
            }
            // Draw color wheel if open
            colorWheel.draw(window, font);

            // Draw file browser if open
            if (fileBrowser.isOpen)
            {
                fileBrowser.draw(window, font);
            }
            // Draw help dialog if open
            if (helpDialog.isOpen && !fileBrowser.isOpen)
            {
                helpDialog.draw(window, font);
            }

            // Draw resize dialog with 8-bit style
            if (showResizeDialog && !fileBrowser.isOpen)
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
                if (leftMousePressedThisFrame && !uiElementClicked && !fileBrowser.isOpen)
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
            if (showEraserSizeDialog && !fileBrowser.isOpen)
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
                if (leftMousePressedThisFrame && !uiElementClicked && !fileBrowser.isOpen)
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

            // Draw GIF export dialog with 8-bit style
            if (showGIFExportDialog && !fileBrowser.isOpen)
            {
                sf::Vector2f dialogSize(300, 200);
                sf::Vector2f dialogPos(winSize.x / 2 - dialogSize.x / 2, winSize.y / 2 - dialogSize.y / 2);

                // Background with 8-bit style
                sf::RectangleShape dialogBg(dialogSize);
                dialogBg.setPosition(dialogPos);
                dialogBg.setFillColor(sf::Color(EightBitColors::DarkBlue.r, EightBitColors::DarkBlue.g, EightBitColors::DarkBlue.b));
                dialogBg.setOutlineColor(sf::Color(EightBitColors::Yellow.r, EightBitColors::Yellow.g, EightBitColors::Yellow.b));
                dialogBg.setOutlineThickness(2);
                window.draw(dialogBg);

                // Title
                sf::Text title("EXPORT GIF SETTINGS", font, 16);
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
                widthInput.setOutlineColor(gifWidthInputActive ? sf::Color(EightBitColors::Yellow.r, EightBitColors::Yellow.g, EightBitColors::Yellow.b) : sf::Color(EightBitColors::White.r, EightBitColors::White.g, EightBitColors::White.b));
                widthInput.setOutlineThickness(2);
                window.draw(widthInput);

                sf::Text widthText(gifWidthStr, font, 14);
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
                heightInput.setOutlineColor(gifHeightInputActive ? sf::Color(EightBitColors::Yellow.r, EightBitColors::Yellow.g, EightBitColors::Yellow.b) : sf::Color(EightBitColors::White.r, EightBitColors::White.g, EightBitColors::White.b));
                heightInput.setOutlineThickness(2);
                window.draw(heightInput);

                sf::Text heightText(gifHeightStr, font, 14);
                heightText.setStyle(sf::Text::Bold);
                heightText.setPosition(dialogPos.x + 85, dialogPos.y + 80);
                heightText.setFillColor(sf::Color(EightBitColors::White.r, EightBitColors::White.g, EightBitColors::White.b));
                window.draw(heightText);

                // Delay input
                sf::Text delayLabel("DELAY (1-100):", font, 14);
                delayLabel.setStyle(sf::Text::Bold);
                delayLabel.setPosition(dialogPos.x + 20, dialogPos.y + 110);
                delayLabel.setFillColor(sf::Color(EightBitColors::White.r, EightBitColors::White.g, EightBitColors::White.b));
                window.draw(delayLabel);

                sf::RectangleShape delayInput({80, 25});
                delayInput.setPosition(dialogPos.x + 120, dialogPos.y + 110);
                delayInput.setFillColor(sf::Color(EightBitColors::Black.r, EightBitColors::Black.g, EightBitColors::Black.b));
                delayInput.setOutlineColor(gifDelayInputActive ? sf::Color(EightBitColors::Yellow.r, EightBitColors::Yellow.g, EightBitColors::Yellow.b) : sf::Color(EightBitColors::White.r, EightBitColors::White.g, EightBitColors::White.b));
                delayInput.setOutlineThickness(2);
                window.draw(delayInput);

                sf::Text delayText(gifDelayStr, font, 14);
                delayText.setStyle(sf::Text::Bold);
                delayText.setPosition(dialogPos.x + 125, dialogPos.y + 115);
                delayText.setFillColor(sf::Color(EightBitColors::White.r, EightBitColors::White.g, EightBitColors::White.b));
                window.draw(delayText);

                // Apply button
                bool applyHovered = (mpos.x >= (int)(dialogPos.x + 50) && mpos.x <= (int)(dialogPos.x + 110) &&
                                     mpos.y >= (int)(dialogPos.y + 150) && mpos.y <= (int)(dialogPos.y + 175));
                drawButton(window, sf::FloatRect(dialogPos.x + 50, dialogPos.y + 150, 60, 25), font, "EXPORT", false, applyHovered);

                // Cancel button
                bool cancelHovered = (mpos.x >= (int)(dialogPos.x + 140) && mpos.x <= (int)(dialogPos.x + 200) &&
                                      mpos.y >= (int)(dialogPos.y + 150) && mpos.y <= (int)(dialogPos.y + 175));
                drawButton(window, sf::FloatRect(dialogPos.x + 140, dialogPos.y + 150, 60, 25), font, "CANCEL", false, cancelHovered);

                // Handle GIF export dialog clicks - ONLY on mouse press
                if (leftMousePressedThisFrame && !uiElementClicked && !fileBrowser.isOpen)
                {
                    sf::Vector2i mm = sf::Mouse::getPosition(window);

                    // Define all rects first
                    sf::FloatRect widthInputRect(dialogPos.x + 80, dialogPos.y + 40, 80, 25);
                    sf::FloatRect heightInputRect(dialogPos.x + 80, dialogPos.y + 75, 80, 25);
                    sf::FloatRect delayInputRect(dialogPos.x + 120, dialogPos.y + 110, 80, 25);
                    sf::FloatRect applyRect(dialogPos.x + 50, dialogPos.y + 150, 60, 25);
                    sf::FloatRect cancelRect(dialogPos.x + 140, dialogPos.y + 150, 60, 25);
                    sf::FloatRect dialogRect(dialogPos.x, dialogPos.y, dialogSize.x, dialogSize.y);

                    if (widthInputRect.contains((float)mm.x, (float)mm.y))
                    {
                        gifWidthInputActive = true;
                        gifHeightInputActive = false;
                        gifDelayInputActive = false;
                        uiElementClicked = true;
                    }
                    else if (heightInputRect.contains((float)mm.x, (float)mm.y))
                    {
                        gifHeightInputActive = true;
                        gifWidthInputActive = false;
                        gifDelayInputActive = false;
                        uiElementClicked = true;
                    }
                    else if (delayInputRect.contains((float)mm.x, (float)mm.y))
                    {
                        gifDelayInputActive = true;
                        gifWidthInputActive = false;
                        gifHeightInputActive = false;
                        uiElementClicked = true;
                    }
                    else if (applyRect.contains((float)mm.x, (float)mm.y))
                    {
                        // Show file browser for GIF export location
                        fileBrowser.isOpen = true;
                        fileBrowser.title = "Export GIF";
                        fileBrowser.defaultExtension = ".gif";
                        fileBrowser.allowedExtensions = {".gif"};
                        fileBrowser.filenameInput = "animation.gif";
                        fileBrowser.filenameInputActive = true;

                        // Close the GIF settings dialog but DON'T mark as UI clicked
                        // This allows the file browser to open properly
                        showGIFExportDialog = false;
                        // Don't set uiElementClicked = true here!
                    }
                    else if (cancelRect.contains((float)mm.x, (float)mm.y))
                    {
                        showGIFExportDialog = false;
                        uiElementClicked = true;
                    }
                    else if (!dialogRect.contains((float)mm.x, (float)mm.y))
                    {
                        // Clicked outside dialog - close it
                        showGIFExportDialog = false;
                        uiElementClicked = true;
                    }
                    else
                    {
                        // Clicked on the dialog background, just consume the click
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

            // Update the status text to show undo/redo info
            std::string toolName = canvas.currentTool == Tool::Pencil ? "PENCIL" : canvas.currentTool == Tool::Eraser ? "ERASER"
                                                                                                                      : "FILL";
            std::string undoStatus = " | UNDO: " + std::string(canvas.canUndo() ? "READY" : "UNAVAILABLE") +
                                     " | REDO: " + std::string(canvas.canRedo() ? "READY" : "UNAVAILABLE");
            sf::Text status("TOOL: " + toolName + "  FRAME: " + std::to_string(canvas.currentFrame) +
                                "  ZOOM: " + std::to_string((int)canvas.zoom) + "x" +
                                (canvas.currentTool == Tool::Eraser ? "  ERASER SIZE: " + std::to_string(canvas.eraserSize) : "") +
                                undoStatus,
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