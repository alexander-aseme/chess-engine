#pragma once
#include <string>

namespace gui {

// Preferences that survive restarts, stored as "key=value" lines in a small
// text file in the user's settings folder:
//   macOS    ~/Library/Application Support/Chess/settings.ini
//   Linux    $XDG_CONFIG_HOME/chess/settings.ini (or ~/.config/chess/...)
//   Windows  %APPDATA%\Chess\settings.ini
// In the browser nothing is saved; the defaults are used each time.
struct Settings {
    float volume = 0.8f;    // sound effects, 0..1
    bool autoFlip = true;   // local games: turn the board to face the side to move

    static Settings load();  // defaults for anything missing or unreadable
    void save() const;
};

std::string settingsPath();  // empty where settings aren't saved

}  // namespace gui
