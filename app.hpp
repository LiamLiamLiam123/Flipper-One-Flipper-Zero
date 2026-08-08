#pragma once
#include "easy_flipper/easy_flipper.h"

#define TAG         "CPU Start"
#define VERSION     "1.0"
#define VERSION_TAG TAG " " VERSION
#define APP_ID      "cpu_start"

struct FakeFile;

typedef enum {
    CPUStartMainDesktop = 0,
    CPUStartMainBootMenu = 1,
    CPUStartMainApps = 2,
    CPUStartMainFiles = 3,
    CPUStartMainNetwork = 4,
    CPUStartMainSettings = 5,
    CPUStartMainBootProfiles = 6,
} CPUStartMainIndex;

typedef enum {
    CPUStartAppsInternetRadio = 0,
    CPUStartAppsVoiceRecorder = 1,
    CPUStartAppsWalkieTalkie = 2,
    CPUStartAppsTVMediaBox = 3,
    CPUStartAppsTerminal = 4,
    CPUStartAppsGames = 5,
} CPUStartAppsIndex;

typedef enum {
    CPUStartGamesSnake = 0,
    CPUStartGamesTetris = 1,
    CPUStartGamesPong = 2,
} CPUStartGamesIndex;

typedef enum {
    CPUStartBootProfilesRouter = 0,
    CPUStartBootProfilesTVMedia = 1,
    CPUStartBootProfilesDesktop = 2,
    CPUStartBootProfilesMinimal = 3,
    CPUStartBootProfilesSDCard = 4,
} CPUStartBootProfilesIndex;

typedef enum {
    CPUStartViewUBoot = 0,
    CPUStartViewMainMenu = 1,
    CPUStartViewAppsMenu = 2,
    CPUStartViewBootProfiles = 3,
    CPUStartViewFiles = 4,
    CPUStartViewNetwork = 5,
    CPUStartViewSettings = 6,
    CPUStartViewTerminal = 7,
    CPUStartViewDesktop = 8,
    CPUStartViewBoot = 9,
    CPUStartViewFakeApp = 10,
    CPUStartViewTerminalInput = 11,
    CPUStartViewGamesMenu = 12,
} CPUStartView;

typedef enum {
    CPUStartUBootDesktop = 0,
    CPUStartUBootTVMediaBox = 1,
    CPUStartUBootRouter = 2,
    CPUStartUBootMinimal = 3,
    CPUStartUBootNoGraphics = 4,
} CPUStartUBootIndex;

typedef enum {
    CPUStartFakeRadio = 0,
    CPUStartFakeRecorder = 1,
    CPUStartFakeWalkie = 2,
    CPUStartFakeTV = 3,
    CPUStartFakeRouter = 4,
    CPUStartFakeSnake = 5,
    CPUStartFakeTetris = 6,
    CPUStartFakePong = 7,
} CPUStartFakeIndex;

#define SNAKE_COLS 32
#define SNAKE_ROWS 13
#define SNAKE_MAX 80
#define TETRIS_COLS 10
#define TETRIS_ROWS 12
#define PADDLE_H 16
#define PLAY_TOP 20
#define PLAY_BOTTOM 60

typedef enum {
    CPUStartCustomEventTick = 0,
} CPUStartCustomEvent;

class CPUStartApp {
private:
    Gui* gui = nullptr;
    ViewDispatcher* viewDispatcher = nullptr;

    // U-Boot menu (custom view, framed box)
    View* ubootView = nullptr;
    uint8_t ubootSelected = 0;

    // Submenus
    Submenu* mainMenu = nullptr;
    Submenu* appsMenu = nullptr;
    Submenu* bootProfilesMenu = nullptr;

    // Files / Network / Settings views (custom)
    View* filesView = nullptr;
    uint8_t fileLevel = 0;
    uint8_t fileIndex = 0;
    uint8_t fileDir = 0;
    View* networkView = nullptr;
    uint8_t netState = 0; // 0 scanning, 1 list, 2 connecting, 3 connected
    uint8_t netIndex = 0;
    uint32_t netStartTick = 0;
    View* settingsView = nullptr;
    uint8_t setIndex = 0;
    uint8_t setValues[5] = {3, 0, 1, 1, 50};

    // Boot sequence view (custom)
    View* bootView = nullptr;
    uint8_t bootRevealed = 0;
    uint32_t bootTargetView = CPUStartViewMainMenu;
    const char* bootName = "";

    // Fake app view (custom)
    View* fakeAppView = nullptr;
    uint8_t fakeAppIndex = CPUStartFakeRadio;
    uint8_t fakeAppSelected = 0;
    uint8_t fakeAppState = 0;
    uint32_t fakeAppStartTick = 0;
    uint32_t fakeAppPrevView = CPUStartViewAppsMenu;

    // Games menu
    Submenu* gamesMenu = nullptr;

    // Snake game
    uint8_t snakeBody[SNAKE_MAX][2];
    uint8_t snakeLen;
    uint8_t snakeDir;
    uint8_t snakeFoodX;
    uint8_t snakeFoodY;
    uint32_t snakeScore;
    bool snakeStarted;
    bool snakePaused;
    bool snakeOver;

    // Tetris game
    uint8_t tetrisBoard[TETRIS_COLS][TETRIS_ROWS];
    uint8_t tetrisCells[4][2];
    uint8_t tetrisPiece;
    uint8_t tetrisNext;
    int8_t tetrisOX;
    int8_t tetrisOY;
    uint32_t tetrisScore;
    uint8_t tetrisGrav;
    bool tetrisSoftDrop;
    bool tetrisOver;

    // Pong game
    uint8_t pongPaddleL;
    uint8_t pongPaddleR;
    int16_t pongBallX;
    int16_t pongBallY;
    int8_t pongBallVX;
    int8_t pongBallVY;
    uint8_t pongScoreL;
    uint8_t pongScoreR;
    uint8_t pongSpeed;
    bool pongStarted;
    bool pongOver;

    // Terminal view (custom)
    View* terminalView = nullptr;
    char termLines[6][48];
    uint8_t termLineCount = 0;
    uint8_t termCmdIndex = 0;
    char termInput[64];
    TextInput* terminalInput = nullptr;

    // Desktop view (custom)
    View* desktopView = nullptr;
    FuriTimer* animationTimer = nullptr;
    uint8_t desktopSelected = 0;
    uint32_t desktopStartTick = 0;

    const struct FakeFile* filesCurrent(uint8_t* count);
    void settingsValueText(uint8_t i, char* buf, uint8_t len);

    static uint32_t callbackExitApp(void* context);
    static uint32_t callbackReturnToMain(void* context);
    static uint32_t callbackReturnToApps(void* context);
    static void callbackSubmenuMain(void* context, uint32_t index);
    static void callbackSubmenuApps(void* context, uint32_t index);
    static void callbackSubmenuBootProfiles(void* context, uint32_t index);
    static bool callbackUBootInput(InputEvent* event, void* context);
    static void callbackUBootDraw(Canvas* canvas, void* context);
    static bool callbackBootInput(InputEvent* event, void* context);
    static void callbackBootDraw(Canvas* canvas, void* context);
    static bool callbackBootCustomEvent(uint32_t event, void* context);
    static bool callbackFakeAppInput(InputEvent* event, void* context);
    static void callbackFakeAppDraw(Canvas* canvas, void* context);
    static bool callbackFakeAppCustomEvent(uint32_t event, void* context);
    static void drawRadio(Canvas* canvas, CPUStartApp* app);
    static void drawRecorder(Canvas* canvas, CPUStartApp* app);
    static void drawWalkie(Canvas* canvas, CPUStartApp* app);
    static void drawTV(Canvas* canvas, CPUStartApp* app);
    static void drawRouter(Canvas* canvas, CPUStartApp* app);
    static void drawSnake(Canvas* canvas, CPUStartApp* app);
    static void drawTetris(Canvas* canvas, CPUStartApp* app);
    static void drawPong(Canvas* canvas, CPUStartApp* app);
    static void snakeInit(CPUStartApp* app);
    static bool snakeOccupies(CPUStartApp* app, uint8_t x, uint8_t y);
    static void snakePlaceFood(CPUStartApp* app);
    static void snakeUpdate(CPUStartApp* app);
    static void tetrisInit(CPUStartApp* app);
    static void tetrisSpawn(CPUStartApp* app);
    static bool tetrisCollides(
        CPUStartApp* app, int8_t ox, int8_t oy, const uint8_t cells[4][2]);
    static bool tetrisStepDown(CPUStartApp* app);
    static void tetrisLock(CPUStartApp* app);
    static void tetrisClearLines(CPUStartApp* app);
    static void tetrisMove(CPUStartApp* app, int8_t dx);
    static void tetrisRotate(CPUStartApp* app);
    static void tetrisHardDrop(CPUStartApp* app);
    static void tetrisUpdate(CPUStartApp* app);
    static void pongInit(CPUStartApp* app);
    static void pongResetBall(CPUStartApp* app);
    static void pongUpdate(CPUStartApp* app);
    static void callbackSubmenuGames(void* context, uint32_t index);
    static uint32_t callbackReturnFromFakeApp(void* context);
    static bool callbackFilesInput(InputEvent* event, void* context);
    static void callbackFilesDraw(Canvas* canvas, void* context);
    static bool callbackFilesCustomEvent(uint32_t event, void* context);
    static bool callbackNetworkInput(InputEvent* event, void* context);
    static void callbackNetworkDraw(Canvas* canvas, void* context);
    static bool callbackNetworkCustomEvent(uint32_t event, void* context);
    static bool callbackSettingsInput(InputEvent* event, void* context);
    static void callbackSettingsDraw(Canvas* canvas, void* context);
    static bool callbackSettingsCustomEvent(uint32_t event, void* context);
    static bool callbackTerminalInput(InputEvent* event, void* context);
    static void callbackTerminalDraw(Canvas* canvas, void* context);
    static bool callbackTerminalCustomEvent(uint32_t event, void* context);
    static void callbackTerminalInputResult(void* context);
    static bool callbackDesktopCustomEvent(uint32_t event, void* context);
    static bool callbackDesktopInput(InputEvent* event, void* context);
    static void callbackDesktopDraw(Canvas* canvas, void* context);
    static void callbackTimer(void* context);
    static uint32_t callbackReturnToTerminal(void* context);
    void startBoot(const char* name, uint32_t target);

public:
    CPUStartApp();
    ~CPUStartApp();
    void runDispatcher();
};
