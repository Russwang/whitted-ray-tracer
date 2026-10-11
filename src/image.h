#pragma once
#include <vector>
#include <string>

struct Pixel {
    unsigned char r, g, b;
};

class Image {
public:
    int width, height;
    std::vector<Pixel> data;

    Image(int w, int h);
    Image(const std::string& filename); // 从文件读取
    Pixel& getPixel(int x, int y);
    void setPixel(int x, int y, const Pixel& color);
    void save(const std::string& filename) const;
};
