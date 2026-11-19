#include "gui/ui.h"

#include <algorithm>

namespace gui {

namespace {

constexpr Color kDim{.r = 0.0f, .g = 0.0f, .b = 0.0f, .a = 0.55f};
constexpr Color kCard{.r = 0.20f, .g = 0.19f, .b = 0.18f, .a = 1.0f};
constexpr Color kButton{.r = 0.30f, .g = 0.29f, .b = 0.27f, .a = 1.0f};
constexpr Color kButtonHover{.r = 0.36f, .g = 0.35f, .b = 0.33f, .a = 1.0f};
constexpr Color kButtonDisabled{.r = 0.25f, .g = 0.24f, .b = 0.23f, .a = 1.0f};
constexpr Color kAccent{.r = 0.45f, .g = 0.56f, .b = 0.27f, .a = 1.0f};
constexpr Color kAccentHover{.r = 0.51f, .g = 0.63f, .b = 0.31f, .a = 1.0f};
constexpr Color kTrack{.r = 0.36f, .g = 0.35f, .b = 0.33f, .a = 1.0f};
constexpr Color kKnob{.r = 0.95f, .g = 0.94f, .b = 0.91f, .a = 1.0f};
constexpr Color kText{.r = 0.93f, .g = 0.91f, .b = 0.87f, .a = 1.0f};
constexpr Color kMuted{.r = 0.58f, .g = 0.56f, .b = 0.52f, .a = 1.0f};

constexpr float kRadius = 6;
constexpr float kTextSize = 14;

bool contains(Rect r, float x, float y) { return x >= r.x && x < r.x + r.w && y >= r.y && y < r.y + r.h; }

// The text size that fits `text` in `width` points: `size`, or smaller if needed.
float fitSize(const Renderer& r, Font& f, std::string_view text, float size, float width) {
    float w = f.measure(r, text, size);
    return w > width && w > 0 ? std::max(9.0f, size * width / w) : size;
}

}  // namespace

bool Ui::mouseDown(float x, float y) {
    mx_ = x;
    my_ = y;
    bool mine = lastModal_ || std::ranges::any_of(lastRegions_, [&](Rect r) { return contains(r, x, y); });
    if (mine) {
        pressPending_ = true;
        pressX_ = x;
        pressY_ = y;
        down_ = true;
    }
    return mine;
}

bool Ui::mouseUp(float x, float y) {
    mx_ = x;
    my_ = y;
    if (!down_) return false;
    down_ = false;
    releasePending_ = true;
    releaseX_ = x;
    releaseY_ = y;
    return true;
}

void Ui::mouseMove(float x, float y) {
    mx_ = x;
    my_ = y;
}

void Ui::begin(Renderer& r, Font& regular, Font& bold) {
    r_ = &r;
    regular_ = &regular;
    bold_ = &bold;
    regions_.clear();
    modal_ = false;
    interactive_ = true;
}

void Ui::end() {
    // Events no widget claimed this frame are dropped.
    pressPending_ = false;
    if (releasePending_) {
        releasePending_ = false;
        active_ = -1;
    }
    lastRegions_.swap(regions_);
    lastModal_ = modal_;
}

bool Ui::modal(int id, Rect area, Rect card) {
    r_->fillRect(area, kDim);
    modal_ = true;
    regions_.push_back(area);

    // Only a press outside the card starts a backdrop click; presses on the card are
    // left for its widgets, so dragging a slider off the card doesn't close it.
    if (pressPending_ && contains(area, pressX_, pressY_) && !contains(card, pressX_, pressY_)) {
        active_ = id;
        pressPending_ = false;
    }
    if (releasePending_ && active_ == id) {
        releasePending_ = false;
        active_ = -1;
        return !contains(card, releaseX_, releaseY_);
    }
    return false;
}

bool Ui::hovered(Rect r) const {
    return interactive_ && contains(r, mx_, my_) && (active_ < 0 || !down_);
}

bool Ui::interact(int id, Rect r, bool enabled) {
    regions_.push_back(r);
    if (!interactive_ || !enabled) return false;
    if (pressPending_ && contains(r, pressX_, pressY_)) {
        active_ = id;
        pressPending_ = false;
    }
    if (releasePending_ && active_ == id) {
        releasePending_ = false;
        active_ = -1;
        return contains(r, releaseX_, releaseY_);  // dragging off a button cancels it
    }
    return false;
}

void Ui::card(Rect r) { r_->fillRoundedRect(r, 10, kCard); }

bool Ui::button(int id, Rect r, std::string_view label, bool primary, bool enabled) {
    bool clicked = interact(id, r, enabled);
    bool hot = enabled && hovered(r);
    Color bg = !enabled ? kButtonDisabled : primary ? (hot ? kAccentHover : kAccent) : (hot ? kButtonHover : kButton);
    r_->fillRoundedRect(r, kRadius, bg);
    Font& f = primary ? *bold_ : *regular_;
    float size = fitSize(*r_, f, label, kTextSize, r.w - 12);
    float w = f.measure(*r_, label, size);
    f.draw(*r_, label, r.x + (r.w - w) / 2, r.y + (r.h - size) / 2, size, enabled ? kText : kMuted);
    return clicked;
}

bool Ui::option(int id, Rect r, std::string_view label, bool selected, bool enabled, std::string_view note) {
    bool clicked = interact(id, r, enabled);
    bool hot = enabled && hovered(r);
    Color bg = selected ? kAccent : !enabled ? kButtonDisabled : hot ? kButtonHover : kButton;
    r_->fillRoundedRect(r, kRadius, bg);

    float y = r.y + (r.h - kTextSize) / 2;
    Font& f = selected ? *bold_ : *regular_;  // measure with the font it's drawn in
    if (note.empty()) {  // centred, like a chip
        float size = fitSize(*r_, f, label, kTextSize, r.w - 10);
        float w = f.measure(*r_, label, size);
        f.draw(*r_, label, r.x + (r.w - w) / 2, r.y + (r.h - size) / 2, size, enabled ? kText : kMuted);
    } else {  // label left, note right, like a list row
        f.draw(*r_, label, r.x + 12, y, kTextSize, enabled ? kText : kMuted);
        float noteSize = 12;
        float w = regular_->measure(*r_, note, noteSize);
        regular_->draw(*r_, note, r.x + r.w - 12 - w, r.y + (r.h - noteSize) / 2, noteSize, kMuted);
    }
    return clicked;
}

bool Ui::tile(int id, Rect r, std::string_view label, std::string_view sublabel, bool selected, bool enabled) {
    bool clicked = interact(id, r, enabled);
    Color bg = selected ? kAccent : !enabled ? kButtonDisabled : hovered(r) ? kButtonHover : kButton;
    r_->fillRoundedRect(r, kRadius, bg);

    float gap = 3;
    Font& f = selected ? *bold_ : *regular_;
    float size = fitSize(*r_, f, label, kTextSize, r.w - 10);
    float subSize = fitSize(*r_, *regular_, sublabel, 11, r.w - 10);
    float top = r.y + (r.h - size - gap - subSize) / 2;
    f.draw(*r_, label, r.x + (r.w - f.measure(*r_, label, size)) / 2, top, size, enabled ? kText : kMuted);
    Color subColor = selected ? Color{kText.r, kText.g, kText.b, 0.8f} : kMuted;
    regular_->draw(*r_, sublabel, r.x + (r.w - regular_->measure(*r_, sublabel, subSize)) / 2,
                   top + size + gap, subSize, subColor);
    return clicked;
}

bool Ui::iconButton(int id, Rect r, std::string_view icon) {
    bool clicked = interact(id, r, true);
    bool hot = hovered(r);
    if (hot) r_->fillRoundedRect(r, r.w / 2, kButtonHover);
    float size = r.h * 0.62f;
    regular_->draw(*r_, icon, r.x + (r.w - regular_->measure(*r_, icon, size)) / 2, r.y + (r.h - size) / 2 - 1,
                   size, hot ? kText : kMuted);
    return clicked;
}

bool Ui::toggle(int id, Rect r, std::string_view label, bool& value) {
    bool clicked = interact(id, r, true);
    if (clicked) value = !value;

    regular_->draw(*r_, label, r.x, r.y + (r.h - kTextSize) / 2, kTextSize, kText);
    float tw = 38, th = 22;
    Rect track{r.x + r.w - tw, r.y + (r.h - th) / 2, tw, th};
    r_->fillRoundedRect(track, th / 2, value ? kAccent : (hovered(r) ? kButtonHover : kTrack));
    float knobX = value ? track.x + tw - th / 2 : track.x + th / 2;
    r_->drawCircle(knobX, track.y + th / 2, th / 2 - 3, kKnob, 24);
    return clicked;
}

bool Ui::slider(int id, Rect r, float& value, bool* released) {
    if (released) *released = false;
    regions_.push_back(r);
    bool changed = false;
    if (interactive_) {
        if (pressPending_ && contains(r, pressX_, pressY_)) {
            active_ = id;
            pressPending_ = false;
        }
        if (active_ == id) {
            float v = std::clamp((mx_ - r.x) / r.w, 0.0f, 1.0f);
            changed = v != value;
            value = v;
            if (releasePending_) {
                releasePending_ = false;
                active_ = -1;
                if (released) *released = true;
            }
        }
    }

    float cy = r.y + r.h / 2;
    r_->fillRoundedRect({r.x, cy - 3, r.w, 6}, 3, kTrack);
    r_->fillRoundedRect({r.x, cy - 3, r.w * value, 6}, 3, kAccent);
    r_->drawCircle(r.x + r.w * value, cy, active_ == id || hovered(r) ? 9.0f : 8.0f, kKnob, 24);
    return changed;
}

}  // namespace gui
