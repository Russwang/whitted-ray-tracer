#include "image.h"
#include <cctype>
#include <fstream>
#include <limits>
#include <stdexcept>

namespace {

std::size_t pixelCount(int width, int height) {
    if (width <= 0 || height <= 0) {
        throw std::runtime_error("Image dimensions must be positive");
    }
    const std::size_t w = static_cast<std::size_t>(width);
    const std::size_t h = static_cast<std::size_t>(height);
    if (w > std::vector<Pixel>().max_size() / h) {
        throw std::runtime_error("Image dimensions are too large");
    }
    return w * h;
}

// Leave the token's trailing whitespace in the stream. P6 must consume only
// its header separator, because binary pixels can also contain whitespace.
std::string readToken(std::istream& file) {
    while (true) {
        int ch = file.peek();
        if (ch == std::char_traits<char>::eof()) {
            throw std::runtime_error("Truncated PPM header or pixel data");
        }
        if (std::isspace(static_cast<unsigned char>(ch))) {
            file.get();
        } else if (ch == '#') {
            file.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
        } else {
            break;
        }
    }

    std::string token;
    while (true) {
        int ch = file.peek();
        if (ch == std::char_traits<char>::eof() || ch == '#' ||
            std::isspace(static_cast<unsigned char>(ch))) {
            break;
        }
        token.push_back(static_cast<char>(file.get()));
    }
    return token;
}

int readInteger(std::istream& file) {
    const std::string token = readToken(file);
    std::size_t consumed = 0;
    int value = std::stoi(token, &consumed);
    if (consumed != token.size()) {
        throw std::runtime_error("Invalid integer in PPM file: " + token);
    }
    return value;
}

unsigned char readChannel(std::istream& file) {
    int value = readInteger(file);
    if (value < 0 || value > 255) {
        throw std::runtime_error("PPM color channel must be in [0, 255]");
    }
    return static_cast<unsigned char>(value);
}

} // namespace

Image::Image(int w, int h) : width(w), height(h), data(pixelCount(w, h)) {}

Image::Image(const std::string& filename) : width(0), height(0) {
    std::ifstream file(filename, std::ios::binary);
    if (!file) throw std::runtime_error("Cannot open texture file: " + filename);

    const std::string header = readToken(file);
    if (header != "P3" && header != "P6") {
        throw std::runtime_error("Texture must be a P3 or P6 PPM file: " + filename);
    }

    width = readInteger(file);
    height = readInteger(file);
    int maxVal = readInteger(file);
    if (maxVal != 255) {
        throw std::runtime_error("Only PPM max value 255 is supported");
    }
    data.resize(pixelCount(width, height));

    if (header == "P3") {
        for (Pixel& pixel : data) {
            pixel = {readChannel(file), readChannel(file), readChannel(file)};
        }
    } else {
        int separator = file.get();
        if (separator == std::char_traits<char>::eof() ||
            !std::isspace(static_cast<unsigned char>(separator))) {
            throw std::runtime_error("Missing P6 header separator");
        }
        if (separator == '\r' && file.peek() == '\n') file.get();

        for (Pixel& pixel : data) {
            char channels[3];
            if (!file.read(channels, sizeof(channels))) {
                throw std::runtime_error("Truncated P6 pixel data: " + filename);
            }
            pixel = {static_cast<unsigned char>(channels[0]),
                     static_cast<unsigned char>(channels[1]),
                     static_cast<unsigned char>(channels[2])};
        }
    }
}

Pixel& Image::getPixel(int x, int y) {
    return data[static_cast<std::size_t>(y) * width + x];
}

void Image::setPixel(int x, int y, const Pixel& color) {
    data[static_cast<std::size_t>(y) * width + x] = color;
}

void Image::save(const std::string& filename) const {
    std::ofstream file(filename, std::ios::binary);
    if (!file) throw std::runtime_error("Cannot open output file: " + filename);
    file << "P6\n" << width << " " << height << "\n255\n";

    for (const Pixel& pixel : data) {
        file.put(pixel.r);
        file.put(pixel.g);
        file.put(pixel.b);
    }
    file.flush();
    if (!file) throw std::runtime_error("Failed to write output file: " + filename);
}
