#pragma once

#include <filesystem>
#include <fstream>
#include <string>

namespace DesktopTouch {
// Separate from SDL/Qt log filters and stderr redirection. No input payloads.
class DiagnosticFile {
public:
    bool open(const std::filesystem::path& path) {
        stream.open(path, std::ios::out | std::ios::trunc | std::ios::binary);
        return stream.is_open();
    }
    bool write(const std::string& line) {
        if (!stream.is_open() || lines >= 256 || line.size() > 2048) return false;
        stream << line << '\n';
        stream.flush(); // Readable while streaming, even without a clean exit.
        ++lines;
        return stream.good();
    }
private:
    std::ofstream stream;
    unsigned lines = 0;
};
}
