#pragma once
#include <string_view>
#include <vector>

#include "gui/font.h"
#include "gui/renderer.h"

namespace gui {

// A small immediate-mode widget layer: each frame the game calls button(),
// toggle(), slider() and so on with a rectangle, and they draw themselves and
// report whether they were used. Mouse events arrive between frames; a press
// that lands on a widget (or anywhere while a modal is open) belongs to the UI
// and isn't passed on to the board.
//
// Each widget needs an id that stays the same from frame to frame, so a press
// and its release are matched to the same widget.
class Ui {
public:
    // Input, in window points. mouseDown/mouseUp return true when the UI owns
    // the event, in which case the caller should ignore it.
    bool mouseDown(float x, float y);
    bool mouseUp(float x, float y);
    void mouseMove(float x, float y);

    void begin(Renderer& r, Font& regular, Font& bold);
    void end();

    // While false, widgets draw but don't react (e.g. under an open modal).
    void setInteractive(bool on) { interactive_ = on; }

    // Dims `area` around a modal `card` and makes the UI own every press. Returns
    // true when the user clicks the dimmed backdrop: pressed and released outside
    // the card. Call it before the card's own widgets.
    bool modal(int id, Rect area, Rect card);

    // A push button. `primary` uses the accent colour.
    bool button(int id, Rect r, std::string_view label, bool primary = false, bool enabled = true);
    // A selectable row or chip. A disabled option shows `note` (e.g. "Coming soon").
    bool option(int id, Rect r, std::string_view label, bool selected, bool enabled = true,
                std::string_view note = {});
    // A selectable tile with a main label and a smaller line under it.
    bool tile(int id, Rect r, std::string_view label, std::string_view sublabel, bool selected,
              bool enabled = true);
    // A borderless icon button (e.g. "×" to close a card); highlights on hover.
    bool iconButton(int id, Rect r, std::string_view icon);
    // A labelled on/off switch filling row `r`. Returns true when flipped.
    bool toggle(int id, Rect r, std::string_view label, bool& value);
    // A horizontal slider for value in [0, 1]. Returns true while it changes;
    // `released` is set on the frame the drag ends.
    bool slider(int id, Rect r, float& value, bool* released = nullptr);

    // A rounded card for modals and banners.
    void card(Rect r);

    bool hovered(Rect r) const;

private:
    // Press/release bookkeeping shared by every widget. Returns true on a click:
    // pressed and released inside r.
    bool interact(int id, Rect r, bool enabled);

    Renderer* r_ = nullptr;
    Font* regular_ = nullptr;
    Font* bold_ = nullptr;

    std::vector<Rect> regions_, lastRegions_;  // widget areas drawn this / last frame
    bool modal_ = false, lastModal_ = false;
    bool interactive_ = true;

    float mx_ = -1, my_ = -1;
    bool down_ = false;      // mouse button held, after a press the UI owns
    bool pressPending_ = false, releasePending_ = false;
    float pressX_ = 0, pressY_ = 0, releaseX_ = 0, releaseY_ = 0;
    int active_ = -1;        // widget being pressed or dragged
};

}  // namespace gui
