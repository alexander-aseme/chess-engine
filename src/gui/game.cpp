#include "gui/game.h"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <string>

#include "engine/eval.h"
#include "engine/notation.h"

namespace gui {

using namespace chess;

namespace {

constexpr Color kLightSquare{.r = 0.93f, .g = 0.85f, .b = 0.71f, .a = 1.0f};
constexpr Color kDarkSquare{.r = 0.71f, .g = 0.53f, .b = 0.39f, .a = 1.0f};
constexpr Color kSelected{.r = 0.98f, .g = 0.85f, .b = 0.25f, .a = 0.55f};
constexpr Color kLastMove{.r = 0.80f, .g = 0.82f, .b = 0.30f, .a = 0.40f};
constexpr Color kMoveDot{.r = 0.10f, .g = 0.10f, .b = 0.10f, .a = 0.28f};
constexpr Color kCheck{.r = 0.95f, .g = 0.10f, .b = 0.05f, .a = 0.35f};
constexpr Color kOverlay{.r = 0.0f, .g = 0.0f, .b = 0.0f, .a = 0.45f};
constexpr Color kPickerBack{.r = 0.96f, .g = 0.95f, .b = 0.92f, .a = 1.0f};

// Panel, cards and text.
constexpr Color kPanelBack{.r = 0.21f, .g = 0.20f, .b = 0.19f, .a = 1.0f};
constexpr Color kCardBack{.r = 0.26f, .g = 0.25f, .b = 0.23f, .a = 1.0f};
constexpr Color kAccent{.r = 0.45f, .g = 0.56f, .b = 0.27f, .a = 1.0f};
constexpr Color kRowAlt{.r = 1.0f, .g = 1.0f, .b = 1.0f, .a = 0.03f};
constexpr Color kDivider{.r = 1.0f, .g = 1.0f, .b = 1.0f, .a = 0.08f};
constexpr Color kCurrentMove{.r = 0.80f, .g = 0.82f, .b = 0.30f, .a = 0.35f};
constexpr Color kHoverMove{.r = 1.0f, .g = 1.0f, .b = 1.0f, .a = 0.08f};
constexpr Color kText{.r = 0.93f, .g = 0.91f, .b = 0.87f, .a = 1.0f};
constexpr Color kMutedText{.r = 0.62f, .g = 0.60f, .b = 0.56f, .a = 1.0f};

// Clocks.
constexpr Color kClockIdle{.r = 0.17f, .g = 0.16f, .b = 0.15f, .a = 1.0f};
constexpr Color kClockRunning{.r = 0.93f, .g = 0.91f, .b = 0.87f, .a = 1.0f};
constexpr Color kClockLow{.r = 0.78f, .g = 0.24f, .b = 0.19f, .a = 1.0f};
constexpr Color kClockDarkText{.r = 0.12f, .g = 0.11f, .b = 0.10f, .a = 1.0f};
constexpr Color kWhiteDisc{.r = 0.96f, .g = 0.95f, .b = 0.92f, .a = 1.0f};
constexpr Color kBlackDisc{.r = 0.08f, .g = 0.08f, .b = 0.08f, .a = 1.0f};
constexpr Color kDiscOutline{.r = 0.55f, .g = 0.53f, .b = 0.50f, .a = 1.0f};

// Layout, in points.
constexpr float kMargin = 24;                 // around the board and between board and panel
constexpr float kMinBoard = 256;              // 32 points per square
constexpr float kPanelWidth = 248;            // panel beside the board
constexpr float kBottomPanelMinHeight = 260;  // panel below the board
constexpr float kBottomPanelMaxHeight = 420;
constexpr float kPanelPad = 12;
constexpr float kCardHeight = 44;
constexpr float kButtonHeight = 32;
constexpr int64_t kLowTimeMs = 20'000;  // clocks turn red and show tenths below this

// Widget ids for Ui; each must stay the same from frame to frame.
enum : int {
    IdMenuButton = 1, IdSettingsButton, IdNextGame, IdBannerNext, IdBannerReview,
    IdStart, IdResume, IdMenuSettings, IdSettingsDone, IdVolume, IdAutoFlip, IdCloseMenu,
    IdCloseSettings, IdMenuBackdrop, IdSettingsBackdrop, IdQuit,
    IdModeBase = 100, IdTimeBase = 200,
};

constexpr int kSpriteCell = 128;  // must match tools/make_piece_atlas.py

// Order the promotion picker lists the pieces in, starting on the promotion square.
constexpr PieceType kPromotionChoices[] = {Queen, Knight, Rook, Bishop};

int64_t nowMs() {
    using namespace std::chrono;
    return duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
}

// The atlas cell for p: column = piece type, row = colour.
Rect spriteRect(Piece p) {
    return {float(typeOf(p) * kSpriteCell), float(colorOf(p) * kSpriteCell),
            float(kSpriteCell), float(kSpriteCell)};
}

Rect centredRect(float cx, float cy, float size) {
    return {cx - size / 2, cy - size / 2, size, size};
}

Square kingSquare(const Board& b, chess::Color c) {
    for (Square s = 0; s < 64; ++s)
        if (b.at(s) == makePiece(c, King)) return s;
    return NoSquare;
}

// Which effect to play for m, played from `before` and leading to `after`, where
// `stateAfter` is the game state once it's played (it can depend on the whole game,
// so the caller works it out). The most important event wins: game over, then
// check, then the move itself.
Sound soundFor(const Board& before, const Move& m, const Board& after, GameState stateAfter) {
    if (stateAfter != GameState::Ongoing) return Sound::GameEnd;
    if (isInCheck(after, after.sideToMove())) return Sound::Check;
    if (m.promotion != NoType) return Sound::Promote;

    PieceType type = typeOf(before.at(m.from));
    if (type == King && std::abs(fileOf(m.to) - fileOf(m.from)) == 2) return Sound::Castle;
    bool enPassant = type == Pawn && m.to == before.enPassantSquare();
    return !before.isEmptyAt(m.to) || enPassant ? Sound::Capture : Sound::Move;
}

const char* stateReason(GameState state) {
    switch (state) {
        case GameState::Checkmate:            return "Checkmate";
        case GameState::Stalemate:            return "Stalemate";
        case GameState::FiftyMoveRule:        return "Fifty-move rule";
        case GameState::ThreefoldRepetition:  return "Threefold repetition";
        case GameState::InsufficientMaterial: return "Insufficient material";
        default:                              return "";
    }
}

// Half points as a score: 3 -> "1½", 1 -> "½", 4 -> "2".
std::string formatPoints(int halfPoints) {
    int whole = halfPoints / 2;
    if (halfPoints % 2 == 0) return std::to_string(whole);
    return (whole ? std::to_string(whole) : std::string()) + "\xC2\xBD";  // UTF-8 ½
}

// "1:05:00", "4:32", or "0:09.4" when time is short.
std::string formatClock(int64_t ms) {
    char buf[32];
    int64_t s = ms / 1000;
    if (ms >= 3'600'000)
        std::snprintf(buf, sizeof buf, "%d:%02d:%02d", int(s / 3600), int(s / 60 % 60), int(s % 60));
    else if (ms < kLowTimeMs)
        std::snprintf(buf, sizeof buf, "%d:%02d.%d", int(s / 60), int(s % 60), int(ms % 1000 / 100));
    else
        std::snprintf(buf, sizeof buf, "%d:%02d", int(s / 60), int(s % 60));
    return buf;
}

}  // namespace

// --------------------------------------------------------------------------
// Setup
// --------------------------------------------------------------------------

void Game::minimumWindowSize(int& width, int& height) {
    // Narrowest: the smallest board with the panel underneath it.
    width = int(kMinBoard + 2 * kMargin);
    height = int(kMinBoard + 3 * kMargin + kBottomPanelMinHeight);
}

bool Game::init() {
    pieces_ = loadTexture("assets/pieces.png");
    if (!pieces_.id) {
        std::fprintf(stderr, "Run the game from the project or build folder so "
                             "assets/pieces.png can be found.\n");
        return false;
    }
    if (!font_.load("assets/fonts/DejaVuSans.ttf") || !boldFont_.load("assets/fonts/DejaVuSans-Bold.ttf"))
        return false;

    settings_ = Settings::load();
    audio_.setVolume(settings_.volume);
    audio_.init();  // optional: the game runs silently without an audio device
    newGame();      // a board to show behind the menu; Start begins the match
    return true;
}

void Game::shutdown() {
    audio_.shutdown();
    font_.shutdown();
    boldFont_.shutdown();
    glDeleteTextures(1, &pieces_.id);
}

// --------------------------------------------------------------------------
// Match and game flow
// --------------------------------------------------------------------------

void Game::newMatch() {
    halfPoints_[0] = halfPoints_[1] = 0;
    player1White_ = true;
    gameNumber_ = 1;
    matchStarted_ = true;
    newGame();
}

void Game::newGame() {
    positions_ = {Board{}};
    moves_.clear();
    sans_.clear();
    result_ = {};
    bannerHidden_ = false;
    const TimeControl& tc = kTimeControls[timeControl_];
    clock_.reset(int64_t(tc.minutes) * 60'000, int64_t(tc.incrementSeconds) * 1000);
    clockPausedForModal_ = false;
    scrollRow_ = 0;
    goTo(0);
}

void Game::nextGame() {
    player1White_ = !player1White_;  // take turns playing white
    ++gameNumber_;
    newGame();
}

void Game::finish(Outcome outcome, std::string reason) {
    if (gameOver()) return;
    result_ = {outcome, std::move(reason)};
    bannerHidden_ = false;
    clock_.stop(nowMs());
    if (outcome == Outcome::Draw) {
        ++halfPoints_[0];
        ++halfPoints_[1];
    } else {
        halfPoints_[playerOf(outcome == Outcome::WhiteWins ? White : Black)] += 2;
    }
    std::printf("Game %d: %s. Match %s - %s\n", gameNumber_, result_.reason.c_str(),
                formatPoints(halfPoints_[0]).c_str(), formatPoints(halfPoints_[1]).c_str());
}

// Ends the game when the running clock reaches zero. Running out of time loses,
// unless the opponent has nothing left to mate with, which makes it a draw.
void Game::tickClock() {
    if (gameOver() || !clock_.flagged(nowMs())) return;
    chess::Color loser = clock_.runningSide();
    chess::Color winner = opposite(loser);
    std::string reason = std::string(loser == White ? "White" : "Black") + " ran out of time";
    if (cannotMate(latest(), winner)) finish(Outcome::Draw, reason);
    else finish(winner == White ? Outcome::WhiteWins : Outcome::BlackWins, reason);
    pendingPromotion_ = {};
    dragging_ = false;
    select(NoSquare);
    audio_.play(Sound::GameEnd);
}

// Menus pause the clock: it's a local game, so stepping away is fair.
void Game::setModal(Modal m) {
    if (m == modal_) return;
    int64_t now = nowMs();
    if (modal_ == Modal::None && clock_.running()) {
        clock_.stop(now);
        clockPausedForModal_ = true;
    }
    if (m == Modal::None && clockPausedForModal_) {
        clock_.start(latest().sideToMove(), now);
        clockPausedForModal_ = false;
    }
    modal_ = m;
    dragging_ = false;
}

// Back to the game from the menu or settings (×, a click outside the card, or Esc
// in the menu). Before the first Start there's no match yet, so this begins one
// with the current settings.
void Game::resumeGame() {
    matchStarted_ = true;
    setModal(Modal::None);
}

bool Game::canMove() const {
    if (modal_ != Modal::None || gameOver() || mode_ != Mode::Local) return false;
    // With a clock running, takebacks would let a player rewind time spent; only the
    // latest position is playable. Untimed games allow playing from earlier moves.
    return !clock_.enabled() || ply_ + 1 == positions_.size();
}

Move Game::lastMove() const { return ply_ > 0 ? moves_[ply_ - 1] : Move{}; }

void Game::select(Square s) {
    selected_ = s;
    selectedMoves_.clear();
    if (s == NoSquare) return;
    for (const Move& m : generateLegalMoves(board()))
        if (m.from == s) selectedMoves_.push_back(m);
}

// Plays from -> to if it's legal. A promotion opens the picker instead, since four
// generated moves share that target. Returns false if nothing legal goes there.
bool Game::tryMove(Square from, Square to) {
    long matches = std::ranges::count_if(selectedMoves_, [&](const Move& m) { return m.to == to; });
    if (matches == 0) return false;
    if (matches > 1) {
        pendingPromotion_ = {.from = from, .to = to};
        selectedMoves_.clear();
        dragging_ = false;
        return true;
    }
    play(*std::ranges::find_if(selectedMoves_, [&](const Move& m) { return m.to == to; }));
    return true;
}

void Game::play(const Move& m) {
    Board next = board();
    next.makeMove(m);

    positions_.resize(ply_ + 1);  // drop any moves that were undone
    moves_.resize(ply_);
    sans_.resize(ply_);
    sans_.push_back(toSAN(board(), m));  // before ply_ advances: SAN is relative to the old position
    positions_.push_back(next);
    moves_.push_back(m);

    // White's first move is free; Black's clock starts once it's made.
    if (clock_.enabled()) {
        if (moves_.size() == 1) clock_.start(Black, nowMs());
        else clock_.press(nowMs());
    }

    goTo(ply_ + 1);
    if (state_ == GameState::Checkmate) {
        finish(board().sideToMove() == White ? Outcome::BlackWins : Outcome::WhiteWins, stateReason(state_));
    } else if (state_ != GameState::Ongoing) {
        finish(Outcome::Draw, stateReason(state_));
    }
    audio_.play(soundFor(positions_[ply_ - 1], m, board(), state_));
}

void Game::goTo(std::size_t ply) {
    ply_ = ply;
    state_ = gameState(std::span(positions_).first(ply_ + 1));  // the game up to this ply
    pendingPromotion_ = {};
    dragging_ = false;
    select(NoSquare);
    if (ply_ + 1 == positions_.size()) faceSideToMove();  // reviewing old moves doesn't spin the board
}

void Game::faceSideToMove() {
    if (settings_.autoFlip && mode_ == Mode::Local) flipped_ = latest().sideToMove() == Black;
}

// --------------------------------------------------------------------------
// Geometry
// --------------------------------------------------------------------------

Rect Game::squareRect(Square s) const {
    int file = fileOf(s), rank = rankOf(s);
    int col = flipped_ ? 7 - file : file;
    int row = flipped_ ? rank : 7 - rank;  // row 0 is the top of the screen
    return {.x = boardRect_.x + col * squareSize_, .y = boardRect_.y + row * squareSize_,
            .w = squareSize_, .h = squareSize_};
}

Square Game::squareAt(float x, float y) const {
    float lx = x - boardRect_.x, ly = y - boardRect_.y;
    if (lx < 0 || ly < 0 || lx >= boardRect_.w || ly >= boardRect_.h) return NoSquare;
    int col = int(lx / squareSize_), row = int(ly / squareSize_);
    int file = flipped_ ? 7 - col : col;
    int rank = flipped_ ? row : 7 - row;
    return makeSquare(file, rank);
}

// The picker is a column of four squares starting on the promotion square and
// running toward the middle of the board.
Square Game::promotionChoiceSquare(int i) const {
    Square to = pendingPromotion_.to;
    int step = rankOf(to) == 7 ? -1 : 1;
    return makeSquare(fileOf(to), rankOf(to) + step * i);
}

int Game::moveAt(float x, float y) const {
    if (!showPanel_ || x < listRect_.x || x >= listRect_.x + listRect_.w || y < listRect_.y ||
        y >= listRect_.y + listRect_.h)
        return -1;
    int row = scrollRow_ + int((y - listRect_.y) / rowHeight_);
    int index = x >= blackX_ - 6 ? row * 2 + 1 : x >= whiteX_ - 6 ? row * 2 : -1;
    return index >= 0 && index < int(moves_.size()) ? index : -1;
}

// The largest board that fits, with the panel either beside it (wide windows) or
// below it (tall, narrow ones), whichever leaves the bigger board. The pair is
// centred in the window. If even the smaller layout doesn't fit (a browser canvas
// can be any size), the panel is hidden.
void Game::layout(float windowW, float windowH) {
    windowW_ = windowW;
    windowH_ = windowH;
    float beside = std::min(windowH - 2 * kMargin, windowW - 3 * kMargin - kPanelWidth);
    float below = std::min(windowW - 2 * kMargin, windowH - 3 * kMargin - kBottomPanelMinHeight);
    showPanel_ = std::max(beside, below) >= 160;

    if (!showPanel_) {
        float size = std::max(64.0f, std::min(windowW, windowH) - 2 * kMargin);
        boardRect_ = {(windowW - size) / 2, (windowH - size) / 2, size, size};
    } else if (beside >= below) {
        panelSide_ = PanelSide::Right;
        float groupW = beside + kMargin + kPanelWidth;
        boardRect_ = {(windowW - groupW) / 2, (windowH - beside) / 2, beside, beside};
        panelRect_ = {boardRect_.x + beside + kMargin, boardRect_.y, kPanelWidth, beside};
    } else {
        panelSide_ = PanelSide::Bottom;
        float panelH = std::clamp(windowH - below - 3 * kMargin, kBottomPanelMinHeight, kBottomPanelMaxHeight);
        float groupH = below + kMargin + panelH;
        boardRect_ = {(windowW - below) / 2, (windowH - groupH) / 2, below, below};
        panelRect_ = {boardRect_.x, boardRect_.y + below + kMargin, below, panelH};
    }
    squareSize_ = boardRect_.w / 8.0f;
}

// --------------------------------------------------------------------------
// Drawing
// --------------------------------------------------------------------------

void Game::draw(Renderer& r, float windowW, float windowH) {
    tickClock();
    layout(windowW, windowH);
    ui_.begin(r, font_, boldFont_);
    ui_.setInteractive(modal_ == Modal::None);

    drawBoard(r);
    if (pendingPromotion_.to != NoSquare) drawPromotionPicker(r);
    if (gameOver() && ply_ + 1 == positions_.size() && !bannerHidden_) drawGameOver(r);
    if (showPanel_) drawPanel(r);

    ui_.setInteractive(true);
    if (modal_ == Modal::Menu) drawMenu(r);
    else if (modal_ == Modal::Settings) drawSettings(r);
    ui_.end();
}

void Game::drawBoard(Renderer& r) {
    for (Square s = 0; s < 64; ++s) {
        bool light = (fileOf(s) + rankOf(s)) % 2 == 1;
        r.fillRect(squareRect(s), light ? kLightSquare : kDarkSquare);
    }

    Move last = lastMove();
    if (last.from != NoSquare) {
        r.fillRect(squareRect(last.from), kLastMove);
        r.fillRect(squareRect(last.to), kLastMove);
    }
    if (selected_ != NoSquare) r.fillRect(squareRect(selected_), kSelected);

    // Red glow under a king in check: two stacked circles, darker in the middle.
    if (isInCheck(board(), board().sideToMove())) {
        Rect k = squareRect(kingSquare(board(), board().sideToMove()));
        r.drawCircle(k.x + k.w / 2, k.y + k.h / 2, k.w * 0.50f, kCheck);
        r.drawCircle(k.x + k.w / 2, k.y + k.h / 2, k.w * 0.32f, kCheck);
    }

    drawCoordinates(r);

    for (Square s = 0; s < 64; ++s) {
        Piece p = board().at(s);
        if (p == NoPiece || (dragging_ && s == selected_)) continue;  // dragged piece is drawn last
        r.drawSprite(pieces_, spriteRect(p), squareRect(s));
    }

    for (const Move& m : selectedMoves_) {
        Rect sq = squareRect(m.to);
        r.drawCircle(sq.x + sq.w / 2, sq.y + sq.h / 2, sq.w * 0.16f, kMoveDot);
    }

    // The dragged piece follows the mouse, slightly enlarged, above everything else.
    if (dragging_ && selected_ != NoSquare)
        r.drawSprite(pieces_, spriteRect(board().at(selected_)),
                     centredRect(mouseX_, mouseY_, squareSize_ * 1.1f));
}

// Rank numbers down the left edge and file letters along the bottom edge, inside
// the corner of each square, in the colour of the other kind of square.
void Game::drawCoordinates(Renderer& r) {
    float size = std::max(9.0f, squareSize_ * 0.19f);
    float pad = squareSize_ * 0.06f;
    for (int i = 0; i < 8; ++i) {
        // Left column (screen column 0), row i from the top.
        Square left = flipped_ ? makeSquare(7, i) : makeSquare(0, 7 - i);
        Rect sq = squareRect(left);
        bool light = (fileOf(left) + rankOf(left)) % 2 == 1;
        char rank[2] = {char('1' + rankOf(left)), 0};
        boldFont_.draw(r, rank, sq.x + pad, sq.y + pad * 0.5f, size, light ? kDarkSquare : kLightSquare);

        // Bottom row (screen row 7), column i from the left.
        Square bottom = flipped_ ? makeSquare(7 - i, 7) : makeSquare(i, 0);
        sq = squareRect(bottom);
        light = (fileOf(bottom) + rankOf(bottom)) % 2 == 1;
        char file[2] = {char('a' + fileOf(bottom)), 0};
        float w = boldFont_.measure(r, file, size);
        boldFont_.draw(r, file, sq.x + sq.w - pad - w, sq.y + sq.h - size - pad * 0.5f, size,
                       light ? kDarkSquare : kLightSquare);
    }
}

// One player: colour, name, match score and clock. The side to move gets an
// accent bar; its clock is light while running and red when time is short.
void Game::drawPlayerCard(Renderer& r, Rect card, chess::Color side) {
    r.fillRoundedRect(card, 6, kCardBack);
    if (!gameOver() && latest().sideToMove() == side)
        r.fillRoundedRect({card.x, card.y + 8, 3, card.h - 16}, 1.5f, kAccent);

    float cy = card.y + card.h / 2;
    r.drawCircle(card.x + 18, cy, 8, kDiscOutline, 24);
    r.drawCircle(card.x + 18, cy, 6.5f, side == White ? kWhiteDisc : kBlackDisc, 24);

    int player = playerOf(side);
    float tx = card.x + 34;
    boldFont_.draw(r, player == 0 ? "Player 1" : "Player 2", tx, card.y + 6, 14, kText);
    font_.draw(r, "Score " + formatPoints(halfPoints_[player]), tx, card.y + 25, 12, kMutedText);

    if (!clock_.enabled()) return;
    int64_t ms = clock_.remaining(side, nowMs());
    bool running = clock_.running() && clock_.runningSide() == side;
    bool low = ms < kLowTimeMs && (running || ms == 0);
    Rect box{card.x + card.w - 8 - 84, cy - 15, 84, 30};
    r.fillRoundedRect(box, 5, low ? kClockLow : running ? kClockRunning : kClockIdle);
    std::string text = formatClock(ms);
    float w = boldFont_.measure(r, text, 17);
    boldFont_.draw(r, text, box.x + box.w - 10 - w, box.y + (box.h - 17) / 2, 17,
                   running && !low ? kClockDarkText : kText);
}

// Player cards at the top and bottom (matching the board's orientation), the
// status with evaluation and move counters, the move list (click a move to go to
// it), and the Menu / Settings / Next game buttons.
void Game::drawPanel(Renderer& r) {
    r.fillRoundedRect(panelRect_, 8, kPanelBack);
    float x = panelRect_.x + kPanelPad;
    float w = panelRect_.w - 2 * kPanelPad;
    float right = x + w;
    float y = panelRect_.y + kPanelPad;
    chess::Color topSide = flipped_ ? White : Black;

    drawPlayerCard(r, {x, y, w, kCardHeight}, topSide);
    y += kCardHeight + 10;

    // Status and counters for the position on screen.
    bool atEnd = ply_ + 1 == positions_.size();
    char line[64];
    if (gameOver() && atEnd) {
        boldFont_.draw(r, result_.reason, x, y, 16, kText);
        const char* score = result_.outcome == Outcome::WhiteWins   ? "1-0"
                            : result_.outcome == Outcome::BlackWins ? "0-1"
                                                                    : "\xC2\xBD-\xC2\xBD";
        const char* who = result_.outcome == Outcome::WhiteWins   ? "White wins"
                          : result_.outcome == Outcome::BlackWins ? "Black wins"
                                                                  : "Draw";
        std::snprintf(line, sizeof line, "%s   %s", score, who);
    } else {
        chess::Color side = board().sideToMove();
        const char* status = isInCheck(board(), side) ? (side == White ? "White is in check" : "Black is in check")
                                                      : (side == White ? "White to move" : "Black to move");
        boldFont_.draw(r, status, x, y, 16, kText);
        char eval[16];
        std::snprintf(eval, sizeof eval, "%+.2f", evaluateWhite(board()) / 100.0);
        font_.draw(r, eval, right - font_.measure(r, eval, 14), y + 1, 14, kMutedText);
        std::snprintf(line, sizeof line, "Move %d   Fifty-move %d/100", board().fullmoveNumber(),
                      board().halfmoveClock());
    }
    y += 22;
    font_.draw(r, line, x, y, 13, kMutedText);
    y += 22;
    r.fillRect({panelRect_.x, y, panelRect_.w, 1}, kDivider);
    y += 2;

    // From the bottom up: buttons, Next game, the other player's card.
    float bottom = panelRect_.y + panelRect_.h - kPanelPad;
    float half = (w - 8) / 2;
    float by = bottom - kButtonHeight;
    if (ui_.button(IdMenuButton, {x, by, half, kButtonHeight}, "Menu")) setModal(Modal::Menu);
    if (ui_.button(IdSettingsButton, {x + half + 8, by, half, kButtonHeight}, "Settings")) {
        settingsReturn_ = Modal::None;
        setModal(Modal::Settings);
    }
    by -= 8;
    if (gameOver()) {
        by -= kButtonHeight;
        if (ui_.button(IdNextGame, {x, by, w, kButtonHeight}, "Next game", true)) nextGame();
        by -= 8;
    }
    by -= kCardHeight;
    drawPlayerCard(r, {x, by, w, kCardHeight}, opposite(topSide));
    by -= 8;
    r.fillRect({panelRect_.x, by, panelRect_.w, 1}, kDivider);

    // Move list in the space left between the two dividers.
    listRect_ = {panelRect_.x, y, panelRect_.w, std::max(0.0f, by - 2 - y)};
    whiteX_ = x + 40;
    blackX_ = whiteX_ + (right - whiteX_) / 2;

    int moveRows = int(moves_.size() + 1) / 2;
    bool resultRow = gameOver();
    int totalRows = moveRows + (resultRow ? 1 : 0);
    int fitRows = int(listRect_.h / rowHeight_);
    int visibleRows = std::max(1, fitRows);

    // Follow the current move after it changes; otherwise leave the wheel's position.
    if (scrolledForPly_ != ply_) {
        int target = resultRow && ply_ == moves_.size() ? totalRows - 1 : ply_ > 0 ? int(ply_ - 1) / 2 : 0;
        if (target < scrollRow_) scrollRow_ = target;
        if (target >= scrollRow_ + visibleRows) scrollRow_ = target - visibleRows + 1;
        scrolledForPly_ = ply_;
    }
    scrollRow_ = std::clamp(scrollRow_, 0, std::max(0, totalRows - visibleRows));

    float textSize = 14;
    float textDy = (rowHeight_ - textSize) / 2;
    if (moves_.empty() && fitRows > 0) font_.draw(r, "No moves yet", x, y + textDy, textSize, kMutedText);

    int hovered = modal_ == Modal::None ? moveAt(mouseX_, mouseY_) : -1;
    for (int row = scrollRow_; row < std::min(totalRows, scrollRow_ + fitRows); ++row) {
        float ry = listRect_.y + float(row - scrollRow_) * rowHeight_;
        if (row % 2 == 1) r.fillRect({panelRect_.x, ry, panelRect_.w, rowHeight_}, kRowAlt);

        if (row >= moveRows) {  // the result line after the last move
            const char* score = result_.outcome == Outcome::WhiteWins   ? "1-0"
                                : result_.outcome == Outcome::BlackWins ? "0-1"
                                                                        : "\xC2\xBD-\xC2\xBD";
            float end = boldFont_.draw(r, score, x, ry + textDy, textSize, kText);
            font_.draw(r, result_.reason, end + 10, ry + textDy, 12, kMutedText);
            continue;
        }

        std::snprintf(line, sizeof line, "%d.", row + 1);
        font_.draw(r, line, x, ry + textDy, textSize, kMutedText);

        for (int col = 0; col < 2; ++col) {
            int index = row * 2 + col;
            if (index >= int(moves_.size())) break;
            float cx = col == 0 ? whiteX_ : blackX_;
            Rect cell{cx - 6, ry + 2, (col == 0 ? blackX_ - whiteX_ : right - blackX_) - 2, rowHeight_ - 4};
            bool current = index == int(ply_) - 1;
            if (current) r.fillRoundedRect(cell, 4, kCurrentMove);
            else if (index == hovered) r.fillRoundedRect(cell, 4, kHoverMove);
            (current ? boldFont_ : font_).draw(r, sans_[index], cx, ry + textDy, textSize,
                                               index < int(ply_) ? kText : kMutedText);
        }
    }
}

void Game::drawPromotionPicker(Renderer& r) {
    r.fillRect(boardRect_, kOverlay);
    chess::Color side = board().sideToMove();
    for (int i = 0; i < 4; ++i) {
        Rect sq = squareRect(promotionChoiceSquare(i));
        r.drawCircle(sq.x + sq.w / 2, sq.y + sq.h / 2, sq.w * 0.48f, kPickerBack);
        r.drawSprite(pieces_, spriteRect(makePiece(side, kPromotionChoices[i])), sq);
    }
}

// A card over the dimmed board: how the game ended, who won, the match score,
// and buttons for the next game or reviewing this one.
void Game::drawGameOver(Renderer& r) {
    r.fillRect(boardRect_, kOverlay);

    const char* score = result_.outcome == Outcome::WhiteWins   ? "1-0"
                        : result_.outcome == Outcome::BlackWins ? "0-1"
                                                                : "\xC2\xBD-\xC2\xBD";
    const char* who = result_.outcome == Outcome::WhiteWins   ? "White wins"
                      : result_.outcome == Outcome::BlackWins ? "Black wins"
                                                              : "Draw";
    std::string outcome = std::string(score) + "   " + who;
    std::string match = "Player 1  " + formatPoints(halfPoints_[0]) + " - " + formatPoints(halfPoints_[1]) +
                        "  Player 2";

    float titleSize = std::clamp(squareSize_ * 0.36f, 18.0f, 34.0f);
    float outcomeSize = std::max(14.0f, titleSize * 0.6f);
    float matchSize = 13;
    float pad = 22, gap = 10;
    float buttonW = 116;

    float contentW = std::max({boldFont_.measure(r, result_.reason, titleSize), font_.measure(r, outcome, outcomeSize),
                               font_.measure(r, match, matchSize), 2 * buttonW + 10});
    float w = std::min(contentW + 2 * pad, boardRect_.w - 16);
    float h = pad + titleSize + gap + outcomeSize + gap + matchSize + gap * 2 + kButtonHeight + pad;
    float cx = boardRect_.x + boardRect_.w / 2;
    Rect card{cx - w / 2, boardRect_.y + (boardRect_.h - h) / 2, w, h};
    ui_.card(card);

    float y = card.y + pad;
    auto centred = [&](Font& f, std::string_view text, float size, Color c) {
        f.draw(r, text, cx - f.measure(r, text, size) / 2, y, size, c);
    };
    centred(boldFont_, result_.reason, titleSize, kText);
    y += titleSize + gap;
    centred(font_, outcome, outcomeSize, kText);
    y += outcomeSize + gap;
    centred(font_, match, matchSize, kMutedText);
    y += matchSize + gap * 2;

    if (ui_.button(IdBannerNext, {cx - buttonW - 5, y, buttonW, kButtonHeight}, "Next game", true)) nextGame();
    if (ui_.button(IdBannerReview, {cx + 5, y, buttonW, kButtonHeight}, "Review")) bannerHidden_ = true;
}

// Start screen and pause menu: opponent, time control, start / resume.
void Game::drawMenu(Renderer& r) {
    float pad = 20;
    float cw = std::min(380.0f, windowW_ - 32);
    float h = pad + 36 + 20 + 3 * 46 + 10 + 18 + 22 + 2 * 54 + 8 + 42 + 8 + 36 + pad;
    Rect card{(windowW_ - cw) / 2, std::max(16.0f, (windowH_ - h) / 2), cw, h};
    if (ui_.modal(IdMenuBackdrop, {0, 0, windowW_, windowH_}, card)) return resumeGame();
    ui_.card(card);

    float x = card.x + pad, w = cw - 2 * pad, y = card.y + pad;
    boldFont_.draw(r, matchStarted_ ? "Menu" : "New match", x, y, 22, kText);
    if (ui_.iconButton(IdCloseMenu, {card.x + cw - 12 - 32, card.y + 12, 32, 32}, "\xC3\x97"))  // UTF-8 ×
        resumeGame();
    y += 36;

    font_.draw(r, "OPPONENT", x, y, 11, kMutedText);
    y += 20;
    struct ModeOption {
        const char* label;
        Mode mode;
        bool available;
    };
    constexpr ModeOption modes[] = {
        {"Two players on this device", Mode::Local, true},
        {"Play the computer", Mode::Computer, false},
        {"Play online", Mode::Online, false},
    };
    for (int i = 0; i < 3; ++i) {
        if (ui_.option(IdModeBase + i, {x, y, w, 40}, modes[i].label, mode_ == modes[i].mode, modes[i].available,
                       modes[i].available ? "" : "Coming soon"))
            mode_ = modes[i].mode;
        y += 46;
    }
    y += 10;

    font_.draw(r, "TIME CONTROL", x, y, 11, kMutedText);
    y += 18;
    font_.draw(r, "Minutes each + seconds per move", x, y, 12, kMutedText);
    y += 22;
    float tileW = (w - 16) / 3;
    for (int i = 0; i < int(std::size(kTimeControls)); ++i) {
        const TimeControl& tc = kTimeControls[i];
        Rect tile{x + float(i % 3) * (tileW + 8), y + float(i / 3) * 54, tileW, 48};
        if (ui_.tile(IdTimeBase + i, tile, tc.label, tc.category, menuTimeControl_ == i)) menuTimeControl_ = i;
    }
    y += 2 * 54 + 8;

    if (ui_.button(IdStart, {x, y, w, 42}, matchStarted_ ? "Start new match (score 0-0)" : "Start", true)) {
        timeControl_ = menuTimeControl_;
        newMatch();
        setModal(Modal::None);
    }
    y += 50;

    // Bottom row, equal widths: [Resume] Settings [Quit]. Resume only once a match
    // exists; Quit only on desktop, since a browser tab can't close itself.
    const char* labels[3];
    int ids[3], count = 0;
    if (matchStarted_) labels[count] = "Resume", ids[count++] = IdResume;
    labels[count] = "Settings", ids[count++] = IdMenuSettings;
#ifndef __EMSCRIPTEN__
    labels[count] = "Quit", ids[count++] = IdQuit;
#endif
    float bw = (w - 8 * float(count - 1)) / float(count);
    for (int i = 0; i < count; ++i) {
        if (!ui_.button(ids[i], {x + float(i) * (bw + 8), y, bw, 36}, labels[i])) continue;
        if (ids[i] == IdResume) {
            setModal(Modal::None);
        } else if (ids[i] == IdMenuSettings) {
            settingsReturn_ = Modal::Menu;
            setModal(Modal::Settings);
        } else {
            quitRequested_ = true;
        }
    }
}

// × and clicking outside go straight back to the game; Done (and Esc) go back to
// wherever Settings was opened from, the menu or the game.
void Game::drawSettings(Renderer& r) {
    struct Key {
        const char* key;
        const char* action;
    };
    constexpr Key keys[] = {
        {"Left / Right", "Step through moves"}, {"Up / Down", "First / latest move"},
        {"F", "Flip the board"},                {"N", "Next game"},
        {"R", "Reset match to 0-0"},            {"Esc", "Menu"},
    };
    float pad = 20, keyRow = 18;
    float cw = std::min(400.0f, windowW_ - 32);
    float h = pad + 36 + 20 + 32 + 16 + 36 + 18 + 16 + 20 + std::size(keys) * keyRow + 16 + 40 + pad;
    Rect card{(windowW_ - cw) / 2, std::max(16.0f, (windowH_ - h) / 2), cw, h};
    if (ui_.modal(IdSettingsBackdrop, {0, 0, windowW_, windowH_}, card)) return resumeGame();
    ui_.card(card);

    float x = card.x + pad, w = cw - 2 * pad, y = card.y + pad;
    boldFont_.draw(r, "Settings", x, y, 22, kText);
    if (ui_.iconButton(IdCloseSettings, {card.x + cw - 12 - 32, card.y + 12, 32, 32}, "\xC3\x97"))  // UTF-8 ×
        resumeGame();
    y += 36;

    font_.draw(r, "SOUND EFFECTS", x, y, 11, kMutedText);
    y += 20;
    bool released = false;
    if (ui_.slider(IdVolume, {x + 8, y, w - 64, 32}, settings_.volume, &released)) audio_.setVolume(settings_.volume);
    if (released) {
        settings_.save();
        audio_.play(Sound::Move);  // let the player hear the new level
    }
    std::string percent = std::to_string(int(settings_.volume * 100 + 0.5f)) + "%";
    font_.draw(r, percent, x + w - font_.measure(r, percent, 14), y + 9, 14, kText);
    y += 32 + 16;

    if (ui_.toggle(IdAutoFlip, {x, y, w, 36}, "Flip board each turn", settings_.autoFlip)) {
        settings_.save();
        if (settings_.autoFlip) faceSideToMove();
    }
    y += 36;
    font_.draw(r, "Two-player games: the board faces the side to move.", x, y, 11, kMutedText);
    y += 18 + 16;

    font_.draw(r, "KEYBOARD", x, y, 11, kMutedText);
    y += 20;
    for (const Key& k : keys) {
        boldFont_.draw(r, k.key, x, y, 12, kText);
        font_.draw(r, k.action, x + 96, y, 12, kMutedText);
        y += keyRow;
    }
    y += 16;

    if (ui_.button(IdSettingsDone, {x, y, w, 40}, "Done", true)) setModal(settingsReturn_);
}

// --------------------------------------------------------------------------
// Input
// --------------------------------------------------------------------------

void Game::onMouseDown(float x, float y) {
    mouseX_ = x;
    mouseY_ = y;
    if (ui_.mouseDown(x, y) || modal_ != Modal::None) return;  // a button, or a menu is open

    if (int index = moveAt(x, y); index >= 0) return goTo(std::size_t(index) + 1);  // jump to that move

    Square s = squareAt(x, y);
    if (pendingPromotion_.to != NoSquare) {
        for (int i = 0; i < 4; ++i) {
            if (s == promotionChoiceSquare(i)) {
                Move m = pendingPromotion_;
                m.promotion = kPromotionChoices[i];
                return play(m);
            }
        }
        pendingPromotion_ = {};  // a click anywhere else cancels the promotion
        return select(NoSquare);
    }

    if (!canMove() || s == NoSquare) return select(NoSquare);

    if (isColor(board().at(s), board().sideToMove())) {
        pressedSelectedPiece_ = s == selected_;
        if (s != selected_) select(s);
        dragging_ = true;
        return;
    }
    if (selected_ != NoSquare && tryMove(selected_, s)) return;
    select(NoSquare);
}

void Game::onMouseUp(float x, float y) {
    if (ui_.mouseUp(x, y)) return;
    if (!dragging_) return;
    dragging_ = false;

    Square s = squareAt(x, y);
    if (s == selected_) {
        // Released where it was picked up: a plain click. Clicking the piece that was
        // already selected puts it down; otherwise it stays selected for click-to-move.
        if (pressedSelectedPiece_) select(NoSquare);
        return;
    }
    if (s != NoSquare) tryMove(selected_, s);  // an illegal drop leaves the piece selected
}

void Game::onMouseMove(float x, float y) {
    ui_.mouseMove(x, y);
    mouseX_ = x;
    mouseY_ = y;
}

void Game::onScroll(float dy) {
    if (modal_ != Modal::None || !showPanel_ || mouseX_ < listRect_.x || mouseX_ >= listRect_.x + listRect_.w ||
        mouseY_ < listRect_.y || mouseY_ >= listRect_.y + listRect_.h)
        return;
    scrollAccum_ += dy;
    int rows = int(scrollAccum_);  // trackpads send fractions; scroll whole rows
    scrollAccum_ -= float(rows);
    scrollRow_ -= rows;  // drawPanel clamps it to the list
}

void Game::onKey(const int key, const bool repeat) {
    if (key == GLFW_KEY_ESCAPE) {
        if (repeat) return;
        if (modal_ == Modal::Settings) setModal(settingsReturn_);
        else if (modal_ == Modal::Menu) resumeGame();
        else setModal(Modal::Menu);
        return;
    }
    if (modal_ != Modal::None) return;

    switch (key) {
        case GLFW_KEY_LEFT:  // step back (repeats while held)
            if (ply_ > 0) goTo(ply_ - 1);
            return;
        case GLFW_KEY_RIGHT:  // step forward, with the move's sound
            if (ply_ < moves_.size()) {
                goTo(ply_ + 1);
                audio_.play(soundFor(positions_[ply_ - 1], moves_[ply_ - 1], board(), state_));
            }
            return;
        default: break;
    }
    if (repeat) return;

    switch (key) {
        // First / latest move. Up/Down work on every keyboard; Mac keyboards have
        // no Home/End keys (fn+Left/Right sends them).
        case GLFW_KEY_UP:
        case GLFW_KEY_HOME: goTo(0); break;
        case GLFW_KEY_DOWN:
        case GLFW_KEY_END: goTo(moves_.size()); break;
        case GLFW_KEY_F: flipped_ = !flipped_; break;   // flip the board
        case GLFW_KEY_N:                                 // next game, once this one is over
            if (gameOver()) nextGame();
            break;
        case GLFW_KEY_R: newMatch(); break;  // reset the match: score 0-0, fresh game
        default: break;
    }
}

}  // namespace gui
