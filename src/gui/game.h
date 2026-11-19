#pragma once
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <string>
#include <thread>
#include <vector>

#include "engine/board.h"
#include "engine/clock.h"
#include "engine/movegen.h"
#include "engine/search.h"
#include "gui/audio.h"
#include "gui/font.h"
#include "gui/renderer.h"
#include "gui/settings.h"
#include "gui/ui.h"

namespace gui {

// Who you play: another person at the same device, or the computer.
enum class Mode { Local, Computer };

// "3+2" means 3 minutes each, plus 2 seconds added after every move.
struct TimeControl {
    const char* label;
    const char* category;  // the usual name for games this fast
    int minutes;           // 0 = unlimited
    int incrementSeconds;  // added after each move
};

inline constexpr TimeControl kTimeControls[] = {
    {"Unlimited", "No clock", 0, 0}, {"1+0", "Bullet", 1, 0},  {"3+2", "Blitz", 3, 2},
    {"5+0", "Blitz", 5, 0},          {"10+0", "Rapid", 10, 0}, {"15+10", "Rapid", 15, 10},
};

// How strongly the computer plays. Weaker levels search less deeply, misjudge
// positions on purpose (evaluation noise, in centipawns), and Easy also skips the
// capture search and makes deliberate mistakes. timeMs is the most it spends per
// move, less when its clock is short. See chess::SearchLimits for each setting.
struct ComputerLevel {
    const char* name;
    int maxDepth;
    int64_t timeMs;
    int evalNoise;
    bool quiescence;
    int blunderPercent;
    int blunderMaxLoss;  // centipawns; deliberate mistakes lose between 1 pawn and this
};

inline constexpr ComputerLevel kComputerLevels[] = {
    // Easy: 1 ply, misses recaptures, noise bigger than a minor piece, and on about
    // every other move a deliberate mistake of 1-6 pawns: hanging a piece or even a
    // rook, or not taking one you left hanging.
    {"Easy", 1, 400, 400, false, 50, 600},
    // Medium: sees replies and finishes exchanges, but judges positions loosely.
    {"Medium", 2, 600, 120, true, 0, 0},
    // Hard: a solid 4-ply search with a little noise so it's not machine-perfect.
    {"Hard", 4, 1200, 30, true, 0, 0},
    // Max: the engine at full strength.
    {"Max", chess::kMaxPly, 4000, 0, true, 0, 0},
};

// The playable chess view: the board, the side panel and the menus. It owns the
// match (players, score, time control), the current game record and the clock,
// and turns mouse/keyboard input into moves. All chess rules live in the engine;
// this class only asks it.
class Game {
public:
    ~Game();      // stops a computer search that's still running
    bool init();  // load textures, fonts, sounds and settings; needs the GL context
    void shutdown();

    void draw(Renderer& r, float windowW, float windowH);

    // Smallest window, in points, that still fits a usable board and the panel
    // (beside the board in a wide window, below it in a tall one).
    static void minimumWindowSize(int& width, int& height);

    // Mouse positions are in window points. A press on one of your pieces picks it
    // up; releasing on a legal square plays the move (drag), and so does pressing a
    // legal square while a piece is selected (click, click).
    void onMouseDown(float x, float y);
    void onMouseUp(float x, float y);
    void onMouseMove(float x, float y);
    void onScroll(float dy);  // mouse wheel / trackpad; positive = scroll up
    void onKey(int key, bool repeat);

    // True once the player picks Quit in the menu; the frame loop then closes the window.
    bool quitRequested() const { return quitRequested_; }

private:
    bool quitRequested_ = false;
    enum class Modal { None, Menu, Settings };
    enum class PanelSide { Right, Bottom };

    // How a game ended. Kept apart from state_ (which describes the position on
    // screen), because a game can also end on time, and reviewing earlier moves
    // doesn't undo a result.
    enum class Outcome { None, WhiteWins, BlackWins, Draw };
    struct Result {
        Outcome outcome = Outcome::None;
        std::string reason;  // "Checkmate", "White ran out of time", ...
    };

    // --- Layout, recomputed every frame so it follows the window ---
    Rect boardRect_{};
    float squareSize_ = 0;
    bool showPanel_ = false;
    PanelSide panelSide_ = PanelSide::Right;
    Rect panelRect_{};
    Rect listRect_{};  // the scrolling move list inside the panel
    float rowHeight_ = 24;
    float whiteX_ = 0, blackX_ = 0;  // left edges of the move columns
    float windowW_ = 0, windowH_ = 0;

    // First visible move-list row. It follows the current move whenever ply_
    // changes; between changes the mouse wheel can scroll it.
    int scrollRow_ = 0;
    float scrollAccum_ = 0;  // fractional trackpad scrolling
    std::size_t scrolledForPly_ = std::size_t(-1);

    // --- Match: a series of games between the same two players ---
    // Player 0 is "Player 1". Colours swap every game; points are counted in
    // half points (win 2, draw 1) so a draw's ½ stays an integer.
    // Against the computer, player 0 is the human ("You") and player 1 the computer.
    Mode mode_ = Mode::Local;
    int timeControl_ = 0;        // index into kTimeControls for this match
    int level_ = 1;              // index into kComputerLevels
    int colorChoice_ = 0;        // the human's colour vs the computer: 0 White, 1 Black, 2 Random
    bool matchStarted_ = false;  // false until the first Start, so the menu has no Resume
    bool player1White_ = true;

    // The menu's selections, applied when Start is pressed.
    Mode menuMode_ = Mode::Local;
    int menuTimeControl_ = 0;
    int menuLevel_ = 1;
    int menuColor_ = 0;
    int halfPoints_[2] = {0, 0};
    int gameNumber_ = 1;

    // --- Current game ---
    // Game record: positions_[0] is the start and positions_[i + 1] follows moves_[i].
    // ply_ is the position on screen; the arrow keys step through the record.
    // In untimed games, playing a move from an earlier ply takes back the rest; in
    // timed games moves can only be played at the latest position.
    // sans_[i] is moves_[i] in algebraic notation, for the move list.
    std::vector<chess::Board> positions_{chess::Board{}};
    std::vector<chess::Move> moves_;
    std::vector<std::string> sans_;
    std::size_t ply_ = 0;
    chess::GameState state_ = chess::GameState::Ongoing;  // of the position on screen
    Result result_;
    bool bannerHidden_ = false;  // "Review" hides the game-over banner

    chess::ChessClock clock_;
    bool clockPausedForModal_ = false;

    // --- Computer opponent ---
    // The search runs on searchThread_ so the window stays responsive. It works on a
    // copy of the game record; when it finishes, updateComputer() plays the move on
    // the main thread. searchGen_ changes whenever the record changes in a way that
    // makes a running search stale (new game, a takeback), and a result from an
    // older generation is thrown away.
    std::thread searchThread_;
    std::atomic<bool> stopSearch_{false};
    std::atomic<bool> searchDone_{false};
    bool searching_ = false;
    chess::SearchResult searchResult_;  // written by the thread before searchDone_ is set
    // Shared by the computer's searches within a game, so each move builds on what
    // the previous ones learned. Only the search thread touches it while it runs.
    chess::TranspositionTable table_{32};
    uint64_t searchGen_ = 0, searchStartGen_ = 0;
    int64_t searchStartMs_ = 0;

    chess::Square selected_ = chess::NoSquare;
    std::vector<chess::Move> selectedMoves_;  // legal moves from selected_

    bool dragging_ = false;             // selected_ is following the mouse
    bool pressedSelectedPiece_ = false;  // the press was on the already-selected piece
    float mouseX_ = 0, mouseY_ = 0;

    // from/to of a promotion waiting for the player to pick a piece; to == NoSquare
    // when no picker is open.
    chess::Move pendingPromotion_{};

    bool flipped_ = false;  // view from black's side
    Modal modal_ = Modal::Menu;
    Modal settingsReturn_ = Modal::None;  // where "Done" in Settings goes back to

    Settings settings_;
    Texture pieces_;
    Font font_, boldFont_;
    Audio audio_;
    Ui ui_;

    const chess::Board& board() const { return positions_[ply_]; }
    const chess::Board& latest() const { return positions_.back(); }
    chess::Move lastMove() const;
    bool gameOver() const { return result_.outcome != Outcome::None; }
    bool canMove() const;
    int playerOf(chess::Color c) const { return (c == chess::White) == player1White_ ? 0 : 1; }
    std::string playerName(int player) const;
    chess::Color computerColor() const { return player1White_ ? chess::Black : chess::White; }
    bool computerToMove() const;  // vs computer, at the latest position, its turn, game on

    void updateComputer();  // each frame: start a search when it's the computer's turn, play its result
    void startSearch();
    void cancelSearch();

    chess::Square squareAt(float x, float y) const;
    Rect squareRect(chess::Square s) const;
    chess::Square promotionChoiceSquare(int i) const;
    int moveAt(float x, float y) const;  // index into moves_ under (x, y) in the list, or -1

    void select(chess::Square s);
    bool tryMove(chess::Square from, chess::Square to);
    void play(const chess::Move& m);
    void goTo(std::size_t ply);
    void faceSideToMove();

    void newMatch();  // scores back to 0-0, Player 1 white, new game
    void newGame();   // same players and colours, fresh board and clock
    void nextGame();  // swap colours, keep the score
    void finish(Outcome outcome, std::string reason);
    void tickClock();
    void setModal(Modal m);
    void resumeGame();  // close the menu or settings and return to the board

    void layout(float windowW, float windowH);
    void drawBoard(Renderer& r);
    void drawCoordinates(Renderer& r);
    void drawPanel(Renderer& r);
    void drawPlayerCard(Renderer& r, Rect card, chess::Color side);
    void drawPromotionPicker(Renderer& r);
    void drawGameOver(Renderer& r);
    void drawMenu(Renderer& r);
    void drawSettings(Renderer& r);
};

}  // namespace gui
