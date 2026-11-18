#include "gui/settings.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>

namespace gui {

std::string settingsPath() {
#if defined(__EMSCRIPTEN__)
    return {};
#else
    auto env = [](const char* name) -> std::string {
        const char* v = std::getenv(name);
        return v && *v ? v : "";
    };
    std::filesystem::path dir;
#if defined(_WIN32)
    if (auto appData = env("APPDATA"); !appData.empty()) dir = std::filesystem::path(appData) / "Chess";
#elif defined(__APPLE__)
    if (auto home = env("HOME"); !home.empty()) dir = std::filesystem::path(home) / "Library/Application Support/Chess";
#else
    if (auto xdg = env("XDG_CONFIG_HOME"); !xdg.empty()) dir = std::filesystem::path(xdg) / "chess";
    else if (auto home = env("HOME"); !home.empty()) dir = std::filesystem::path(home) / ".config/chess";
#endif
    return dir.empty() ? std::string() : (dir / "settings.ini").string();
#endif
}

Settings Settings::load() {
    Settings s;
    std::string path = settingsPath();
    if (path.empty()) return s;
    std::ifstream in(path);
    std::string line;
    while (std::getline(in, line)) {
        auto eq = line.find('=');
        if (eq == std::string::npos) continue;
        std::string key = line.substr(0, eq), value = line.substr(eq + 1);
        if (key == "volume") s.volume = std::clamp(float(std::atof(value.c_str())), 0.0f, 1.0f);
        else if (key == "auto_flip") s.autoFlip = value == "1" || value == "true";
    }
    return s;
}

void Settings::save() const {
    std::string path = settingsPath();
    if (path.empty()) return;
    std::error_code ec;
    std::filesystem::create_directories(std::filesystem::path(path).parent_path(), ec);
    std::ofstream out(path);
    if (!out) {
        std::fprintf(stderr, "Could not save settings to %s\n", path.c_str());
        return;
    }
    out << "volume=" << volume << "\n";
    out << "auto_flip=" << (autoFlip ? 1 : 0) << "\n";
}

}  // namespace gui
