#include "app.hpp"

#define BOOT_TICK_PERIOD_MS 200u

static const char* const bootLogLines[] = {
    "U-Boot 2024.01 (Aug 08 2026 - 10:00:00 +0000)",
    "CPU: STM32WB55RG @ 64MHz",
    "DRAM:  256 MiB",
    "MMC:   mmc@2a31: 0",
    "Loading Environment from FAT...",
    "OK",
    "Checking for update... OK",
    "Booting \"%s\"...",
    "[    0.000000] Booting Linux on physical CPU",
    "[    0.000100] Memory: 64K available",
    "[    0.010000] mmc0: new high speed SD card",
    "[    0.020000] mmcblk0: mmc0:0000 4GB",
    "[    0.100000] Freeing unused kernel memory",
    "[    0.150000] Run /sbin/init as init process",
    "[    0.300000] Starting systemd...",
    "[    0.400000] Reached target Graphical Interface",
    "Login: root",
};
#define BOOT_LOG_LINES_COUNT (sizeof(bootLogLines) / sizeof(bootLogLines[0]))
#define BOOT_LOG_VISIBLE     7
#define BOOT_DONE_HOLD_TICKS 4

static const char* const radioStations[] = {
    "Retro FM",
    "Lo-Fi",
    "News 24",
    "Rock 105",
    "Talk Radio",
};
#define RADIO_STATIONS_COUNT (sizeof(radioStations) / sizeof(radioStations[0]))

static const char* const radioFreq[] = {
    "88.6 MHz",
    "92.1 MHz",
    "100.7 MHz",
    "105.3 MHz",
    "107.9 MHz",
};

static const char* const walkieChannels[] = {
    "462.5625 MHz",
    "462.5875 MHz",
    "462.6125 MHz",
    "462.6375 MHz",
    "462.6625 MHz",
};
#define WALKIE_CHANNELS_COUNT (sizeof(walkieChannels) / sizeof(walkieChannels[0]))

static const char* const tvChannels[] = {
    "Morning News",
    "Sports 24",
    "Movie Night",
    "Documentary",
    "Music Box",
};
#define TV_CHANNELS_COUNT (sizeof(tvChannels) / sizeof(tvChannels[0]))

static const int8_t tetrisShapes[7][4][2] = {
    {{0, 0}, {1, 0}, {2, 0}, {3, 0}}, // I
    {{0, 0}, {1, 0}, {0, 1}, {1, 1}}, // O
    {{1, 0}, {0, 1}, {1, 1}, {2, 1}}, // T
    {{1, 0}, {2, 0}, {0, 1}, {1, 1}}, // S
    {{0, 0}, {1, 0}, {1, 1}, {2, 1}}, // Z
    {{0, 0}, {0, 1}, {1, 1}, {2, 1}}, // J
    {{2, 0}, {0, 1}, {1, 1}, {2, 1}}, // L
};

CPUStartApp::CPUStartApp() {
    gui = static_cast<Gui*>(furi_record_open(RECORD_GUI));

    if(!easy_flipper_set_view_dispatcher(&viewDispatcher, gui, this)) {
        FURI_LOG_E(TAG, "Failed to allocate view dispatcher");
        return;
    }

    // ---- U-Boot menu (custom view, framed box) ----
    ubootView = view_alloc();
    if(!ubootView) {
        FURI_LOG_E(TAG, "Failed to allocate uboot view");
        return;
    }
    view_allocate_model(ubootView, ViewModelTypeLockFree, sizeof(CPUStartApp*));
    *(CPUStartApp**)view_get_model(ubootView) = this;
    view_set_context(ubootView, this);
    view_set_draw_callback(ubootView, callbackUBootDraw);
    view_set_input_callback(ubootView, callbackUBootInput);
    view_set_previous_callback(ubootView, callbackReturnToMain);
    view_dispatcher_add_view(viewDispatcher, CPUStartViewUBoot, ubootView);

    // ---- Main menu ----
    if(!easy_flipper_set_submenu(
           &mainMenu, CPUStartViewMainMenu, VERSION_TAG, callbackExitApp, &viewDispatcher)) {
        FURI_LOG_E(TAG, "Failed to allocate main menu");
        return;
    }
    submenu_add_item(mainMenu, "Desktop Computer", CPUStartMainDesktop, callbackSubmenuMain, this);
    submenu_add_item(mainMenu, "Boot menu", CPUStartMainBootMenu, callbackSubmenuMain, this);
    submenu_add_item(mainMenu, "Apps", CPUStartMainApps, callbackSubmenuMain, this);
    submenu_add_item(mainMenu, "Files", CPUStartMainFiles, callbackSubmenuMain, this);
    submenu_add_item(mainMenu, "Network", CPUStartMainNetwork, callbackSubmenuMain, this);
    submenu_add_item(mainMenu, "Settings", CPUStartMainSettings, callbackSubmenuMain, this);
    submenu_add_item(
        mainMenu, "Boot profiles", CPUStartMainBootProfiles, callbackSubmenuMain, this);

    // ---- Apps menu ----
    if(!easy_flipper_set_submenu(
           &appsMenu, CPUStartViewAppsMenu, "Apps", callbackReturnToMain, &viewDispatcher)) {
        FURI_LOG_E(TAG, "Failed to allocate apps menu");
        return;
    }
    submenu_add_item(
        appsMenu, "Internet radio", CPUStartAppsInternetRadio, callbackSubmenuApps, this);
    submenu_add_item(
        appsMenu, "Voice recorder", CPUStartAppsVoiceRecorder, callbackSubmenuApps, this);
    submenu_add_item(
        appsMenu, "Walkie talkie", CPUStartAppsWalkieTalkie, callbackSubmenuApps, this);
    submenu_add_item(appsMenu, "TV media box", CPUStartAppsTVMediaBox, callbackSubmenuApps, this);
    submenu_add_item(appsMenu, "Terminal", CPUStartAppsTerminal, callbackSubmenuApps, this);
    submenu_add_item(appsMenu, "Games", CPUStartAppsGames, callbackSubmenuApps, this);

    // ---- Games menu ----
    if(!easy_flipper_set_submenu(
           &gamesMenu, CPUStartViewGamesMenu, "Games", callbackReturnToApps, &viewDispatcher)) {
        FURI_LOG_E(TAG, "Failed to allocate games menu");
        return;
    }
    submenu_add_item(
        gamesMenu, "Snake", CPUStartGamesSnake, callbackSubmenuGames, this);
    submenu_add_item(
        gamesMenu, "Tetris", CPUStartGamesTetris, callbackSubmenuGames, this);
    submenu_add_item(
        gamesMenu, "Pong", CPUStartGamesPong, callbackSubmenuGames, this);

    // ---- Boot profiles menu ----
    if(!easy_flipper_set_submenu(
           &bootProfilesMenu,
           CPUStartViewBootProfiles,
           "Boot profiles",
           callbackReturnToMain,
           &viewDispatcher)) {
        FURI_LOG_E(TAG, "Failed to allocate boot profiles menu");
        return;
    }
    submenu_add_item(
        bootProfilesMenu, "Router", CPUStartBootProfilesRouter, callbackSubmenuBootProfiles, this);
    submenu_add_item(
        bootProfilesMenu,
        "TV Media Box",
        CPUStartBootProfilesTVMedia,
        callbackSubmenuBootProfiles,
        this);
    submenu_add_item(
        bootProfilesMenu,
        "Desktop",
        CPUStartBootProfilesDesktop,
        callbackSubmenuBootProfiles,
        this);
    submenu_add_item(
        bootProfilesMenu,
        "Minimal System",
        CPUStartBootProfilesMinimal,
        callbackSubmenuBootProfiles,
        this);
    submenu_add_item(
        bootProfilesMenu,
        "Boot from sd card",
        CPUStartBootProfilesSDCard,
        callbackSubmenuBootProfiles,
        this);

    // ---- Files view (custom) ----
    filesView = view_alloc();
    if(!filesView) {
        FURI_LOG_E(TAG, "Failed to allocate files view");
        return;
    }
    view_allocate_model(filesView, ViewModelTypeLockFree, sizeof(CPUStartApp*));
    *(CPUStartApp**)view_get_model(filesView) = this;
    view_set_context(filesView, this);
    view_set_draw_callback(filesView, callbackFilesDraw);
    view_set_input_callback(filesView, callbackFilesInput);
    view_set_custom_callback(filesView, callbackFilesCustomEvent);
    view_set_previous_callback(filesView, callbackReturnToMain);
    view_dispatcher_add_view(viewDispatcher, CPUStartViewFiles, filesView);

    // ---- Network view (custom) ----
    networkView = view_alloc();
    if(!networkView) {
        FURI_LOG_E(TAG, "Failed to allocate network view");
        return;
    }
    view_allocate_model(networkView, ViewModelTypeLockFree, sizeof(CPUStartApp*));
    *(CPUStartApp**)view_get_model(networkView) = this;
    view_set_context(networkView, this);
    view_set_draw_callback(networkView, callbackNetworkDraw);
    view_set_input_callback(networkView, callbackNetworkInput);
    view_set_custom_callback(networkView, callbackNetworkCustomEvent);
    view_set_previous_callback(networkView, callbackReturnToMain);
    view_dispatcher_add_view(viewDispatcher, CPUStartViewNetwork, networkView);

    // ---- Settings view (custom) ----
    settingsView = view_alloc();
    if(!settingsView) {
        FURI_LOG_E(TAG, "Failed to allocate settings view");
        return;
    }
    view_allocate_model(settingsView, ViewModelTypeLockFree, sizeof(CPUStartApp*));
    *(CPUStartApp**)view_get_model(settingsView) = this;
    view_set_context(settingsView, this);
    view_set_draw_callback(settingsView, callbackSettingsDraw);
    view_set_input_callback(settingsView, callbackSettingsInput);
    view_set_custom_callback(settingsView, callbackSettingsCustomEvent);
    view_set_previous_callback(settingsView, callbackReturnToMain);
    view_dispatcher_add_view(viewDispatcher, CPUStartViewSettings, settingsView);

    // ---- Boot sequence view (custom) ----
    bootView = view_alloc();
    if(!bootView) {
        FURI_LOG_E(TAG, "Failed to allocate boot view");
        return;
    }
    view_allocate_model(bootView, ViewModelTypeLockFree, sizeof(CPUStartApp*));
    *(CPUStartApp**)view_get_model(bootView) = this;
    view_set_context(bootView, this);
    view_set_draw_callback(bootView, callbackBootDraw);
    view_set_input_callback(bootView, callbackBootInput);
    view_set_custom_callback(bootView, callbackBootCustomEvent);
    view_set_previous_callback(bootView, callbackReturnToMain);
    view_dispatcher_add_view(viewDispatcher, CPUStartViewBoot, bootView);

    // ---- Fake app view (custom) ----
    fakeAppView = view_alloc();
    if(!fakeAppView) {
        FURI_LOG_E(TAG, "Failed to allocate fake app view");
        return;
    }
    view_allocate_model(fakeAppView, ViewModelTypeLockFree, sizeof(CPUStartApp*));
    *(CPUStartApp**)view_get_model(fakeAppView) = this;
    view_set_context(fakeAppView, this);
    view_set_draw_callback(fakeAppView, callbackFakeAppDraw);
    view_set_input_callback(fakeAppView, callbackFakeAppInput);
    view_set_custom_callback(fakeAppView, callbackFakeAppCustomEvent);
    view_set_previous_callback(fakeAppView, callbackReturnFromFakeApp);
    view_dispatcher_add_view(viewDispatcher, CPUStartViewFakeApp, fakeAppView);

    // ---- Terminal view (custom) ----
    terminalView = view_alloc();
    if(!terminalView) {
        FURI_LOG_E(TAG, "Failed to allocate terminal view");
        return;
    }
    view_allocate_model(terminalView, ViewModelTypeLockFree, sizeof(CPUStartApp*));
    *(CPUStartApp**)view_get_model(terminalView) = this;
    view_set_context(terminalView, this);
    view_set_draw_callback(terminalView, callbackTerminalDraw);
    view_set_input_callback(terminalView, callbackTerminalInput);
    view_set_custom_callback(terminalView, callbackTerminalCustomEvent);
    view_set_previous_callback(terminalView, callbackReturnToApps);
    view_dispatcher_add_view(viewDispatcher, CPUStartViewTerminal, terminalView);

    // Terminal keyboard input (text_input module)
    terminalInput = text_input_alloc();
    if(!terminalInput) {
        FURI_LOG_E(TAG, "Failed to allocate terminal input");
        return;
    }
    text_input_set_result_callback(
        terminalInput, callbackTerminalInputResult, this, termInput, sizeof(termInput), true);
    text_input_set_header_text(terminalInput, "Type a command");
    view_set_previous_callback(text_input_get_view(terminalInput), callbackReturnToTerminal);
    view_dispatcher_add_view(
        viewDispatcher, CPUStartViewTerminalInput, text_input_get_view(terminalInput));

    // ---- Desktop view (custom) ----
    desktopView = view_alloc();
    if(!desktopView) {
        FURI_LOG_E(TAG, "Failed to allocate desktop view");
        return;
    }
    view_allocate_model(desktopView, ViewModelTypeLockFree, sizeof(CPUStartApp*));
    *(CPUStartApp**)view_get_model(desktopView) = this;
    view_set_context(desktopView, this);
    view_set_draw_callback(desktopView, callbackDesktopDraw);
    view_set_input_callback(desktopView, callbackDesktopInput);
    view_set_custom_callback(desktopView, callbackDesktopCustomEvent);
    view_set_previous_callback(desktopView, callbackReturnToMain);
    view_dispatcher_add_view(viewDispatcher, CPUStartViewDesktop, desktopView);

    // Animation timer (drives boot sequence + fake app animations)
    animationTimer = furi_timer_alloc(callbackTimer, FuriTimerTypePeriodic, this);
    if(animationTimer) {
        furi_timer_start(animationTimer, BOOT_TICK_PERIOD_MS);
    }

    // Boot straight into U-Boot
    view_dispatcher_switch_to_view(viewDispatcher, CPUStartViewUBoot);
}

CPUStartApp::~CPUStartApp() {
    if(animationTimer) {
        furi_timer_stop(animationTimer);
        furi_timer_free(animationTimer);
        animationTimer = nullptr;
    }

    if(ubootView) {
        view_dispatcher_remove_view(viewDispatcher, CPUStartViewUBoot);
        view_free(ubootView);
    }
    if(bootView) {
        view_dispatcher_remove_view(viewDispatcher, CPUStartViewBoot);
        view_free(bootView);
    }
    if(fakeAppView) {
        view_dispatcher_remove_view(viewDispatcher, CPUStartViewFakeApp);
        view_free(fakeAppView);
    }
    if(terminalView) {
        view_dispatcher_remove_view(viewDispatcher, CPUStartViewTerminal);
        view_free(terminalView);
    }
    if(desktopView) {
        view_dispatcher_remove_view(viewDispatcher, CPUStartViewDesktop);
        view_free(desktopView);
    }
    if(mainMenu) {
        view_dispatcher_remove_view(viewDispatcher, CPUStartViewMainMenu);
        submenu_free(mainMenu);
    }
    if(appsMenu) {
        view_dispatcher_remove_view(viewDispatcher, CPUStartViewAppsMenu);
        submenu_free(appsMenu);
    }
    if(bootProfilesMenu) {
        view_dispatcher_remove_view(viewDispatcher, CPUStartViewBootProfiles);
        submenu_free(bootProfilesMenu);
    }
    if(gamesMenu) {
        view_dispatcher_remove_view(viewDispatcher, CPUStartViewGamesMenu);
        submenu_free(gamesMenu);
    }
    if(filesView) {
        view_dispatcher_remove_view(viewDispatcher, CPUStartViewFiles);
        view_free(filesView);
    }
    if(networkView) {
        view_dispatcher_remove_view(viewDispatcher, CPUStartViewNetwork);
        view_free(networkView);
    }
    if(settingsView) {
        view_dispatcher_remove_view(viewDispatcher, CPUStartViewSettings);
        view_free(settingsView);
    }
    if(terminalInput) {
        view_dispatcher_remove_view(viewDispatcher, CPUStartViewTerminalInput);
        text_input_free(terminalInput);
    }

    if(viewDispatcher) {
        view_dispatcher_free(viewDispatcher);
    }
    if(gui) {
        furi_record_close(RECORD_GUI);
    }
}

void CPUStartApp::runDispatcher() {
    view_dispatcher_run(viewDispatcher);
}

uint32_t CPUStartApp::callbackExitApp(void* context) {
    UNUSED(context);
    return VIEW_NONE;
}

uint32_t CPUStartApp::callbackReturnToMain(void* context) {
    UNUSED(context);
    return CPUStartViewMainMenu;
}

uint32_t CPUStartApp::callbackReturnToApps(void* context) {
    UNUSED(context);
    return CPUStartViewAppsMenu;
}

uint32_t CPUStartApp::callbackReturnFromFakeApp(void* context) {
    CPUStartApp* app = static_cast<CPUStartApp*>(context);
    return app->fakeAppPrevView;
}

// ===================== Main menu =====================
void CPUStartApp::callbackSubmenuMain(void* context, uint32_t index) {
    CPUStartApp* app = static_cast<CPUStartApp*>(context);
    switch(index) {
    case CPUStartMainDesktop:
        app->desktopStartTick = furi_get_tick();
        app->desktopSelected = 0;
        view_dispatcher_switch_to_view(app->viewDispatcher, CPUStartViewDesktop);
        break;
    case CPUStartMainBootMenu:
        app->ubootSelected = 0;
        view_dispatcher_switch_to_view(app->viewDispatcher, CPUStartViewUBoot);
        break;
    case CPUStartMainApps:
        view_dispatcher_switch_to_view(app->viewDispatcher, CPUStartViewAppsMenu);
        break;
    case CPUStartMainFiles:
        view_dispatcher_switch_to_view(app->viewDispatcher, CPUStartViewFiles);
        break;
    case CPUStartMainNetwork:
        view_dispatcher_switch_to_view(app->viewDispatcher, CPUStartViewNetwork);
        break;
    case CPUStartMainSettings:
        view_dispatcher_switch_to_view(app->viewDispatcher, CPUStartViewSettings);
        break;
    case CPUStartMainBootProfiles:
        view_dispatcher_switch_to_view(app->viewDispatcher, CPUStartViewBootProfiles);
        break;
    default:
        break;
    }
}

// ===================== Apps menu =====================
void CPUStartApp::callbackSubmenuApps(void* context, uint32_t index) {
    CPUStartApp* app = static_cast<CPUStartApp*>(context);
    switch(index) {
    case CPUStartAppsInternetRadio:
        app->fakeAppIndex = CPUStartFakeRadio;
        app->fakeAppSelected = 0;
        app->fakeAppState = 0;
        app->fakeAppPrevView = CPUStartViewAppsMenu;
        view_dispatcher_switch_to_view(app->viewDispatcher, CPUStartViewFakeApp);
        break;
    case CPUStartAppsVoiceRecorder:
        app->fakeAppIndex = CPUStartFakeRecorder;
        app->fakeAppState = 0;
        app->fakeAppPrevView = CPUStartViewAppsMenu;
        view_dispatcher_switch_to_view(app->viewDispatcher, CPUStartViewFakeApp);
        break;
    case CPUStartAppsWalkieTalkie:
        app->fakeAppIndex = CPUStartFakeWalkie;
        app->fakeAppSelected = 0;
        app->fakeAppState = 0;
        app->fakeAppPrevView = CPUStartViewAppsMenu;
        view_dispatcher_switch_to_view(app->viewDispatcher, CPUStartViewFakeApp);
        break;
    case CPUStartAppsTVMediaBox:
        app->fakeAppIndex = CPUStartFakeTV;
        app->fakeAppSelected = 0;
        app->fakeAppState = 1;
        app->fakeAppPrevView = CPUStartViewAppsMenu;
        view_dispatcher_switch_to_view(app->viewDispatcher, CPUStartViewFakeApp);
        break;
    case CPUStartAppsTerminal:
        view_dispatcher_switch_to_view(app->viewDispatcher, CPUStartViewTerminal);
        break;
    case CPUStartAppsGames:
        view_dispatcher_switch_to_view(app->viewDispatcher, CPUStartViewGamesMenu);
        break;
    default:
        break;
    }
}

// ===================== Games menu =====================
void CPUStartApp::callbackSubmenuGames(void* context, uint32_t index) {
    CPUStartApp* app = static_cast<CPUStartApp*>(context);
    app->fakeAppPrevView = CPUStartViewGamesMenu;
    switch(index) {
    case CPUStartGamesSnake:
        app->fakeAppIndex = CPUStartFakeSnake;
        snakeInit(app);
        view_dispatcher_switch_to_view(app->viewDispatcher, CPUStartViewFakeApp);
        break;
    case CPUStartGamesTetris:
        app->fakeAppIndex = CPUStartFakeTetris;
        tetrisInit(app);
        view_dispatcher_switch_to_view(app->viewDispatcher, CPUStartViewFakeApp);
        break;
    case CPUStartGamesPong:
        app->fakeAppIndex = CPUStartFakePong;
        pongInit(app);
        view_dispatcher_switch_to_view(app->viewDispatcher, CPUStartViewFakeApp);
        break;
    default:
        break;
    }
}

// ===================== Boot profiles =====================
void CPUStartApp::callbackSubmenuBootProfiles(void* context, uint32_t index) {
    CPUStartApp* app = static_cast<CPUStartApp*>(context);
    switch(index) {
    case CPUStartBootProfilesRouter:
        app->fakeAppIndex = CPUStartFakeRouter;
        app->fakeAppSelected = 0;
        app->fakeAppState = 0;
        app->fakeAppPrevView = CPUStartViewAppsMenu;
        app->fakeAppStartTick = furi_get_tick();
        app->startBoot("Router", CPUStartViewFakeApp);
        break;
    case CPUStartBootProfilesTVMedia:
        app->fakeAppIndex = CPUStartFakeTV;
        app->fakeAppSelected = 0;
        app->fakeAppState = 1;
        app->fakeAppPrevView = CPUStartViewAppsMenu;
        app->startBoot("TV Media Box", CPUStartViewFakeApp);
        break;
    case CPUStartBootProfilesDesktop:
        app->startBoot("Desktop", CPUStartViewDesktop);
        break;
    case CPUStartBootProfilesMinimal:
        app->startBoot("Minimal System", CPUStartViewMainMenu);
        break;
    case CPUStartBootProfilesSDCard:
        app->startBoot("SD Card", CPUStartViewMainMenu);
        break;
    default:
        break;
    }
}

// ===================== Boot sequence =====================
void CPUStartApp::startBoot(const char* name, uint32_t target) {
    bootName = name;
    bootTargetView = target;
    bootRevealed = 0;
    view_dispatcher_switch_to_view(viewDispatcher, CPUStartViewBoot);
}

bool CPUStartApp::callbackBootInput(InputEvent* event, void* context) {
    UNUSED(event);
    UNUSED(context);
    return false;
}

void CPUStartApp::callbackBootDraw(Canvas* canvas, void* context) {
    CPUStartApp* app = *(CPUStartApp**)context;

    canvas_clear(canvas);
    canvas_set_font(canvas, FontSecondary);

    char line[48];
    uint8_t start = 0;
    uint8_t count = app->bootRevealed;
    if(count > BOOT_LOG_VISIBLE) {
        start = count - BOOT_LOG_VISIBLE;
        count = BOOT_LOG_VISIBLE;
    }
    for(uint8_t i = 0; i < count; i++) {
        uint8_t src = start + i;
        if(src >= BOOT_LOG_LINES_COUNT) break;
        const char* text = bootLogLines[src];
        if(src == 7) {
            snprintf(line, sizeof(line), "Booting \"%s\"...", app->bootName);
            text = line;
        }
        canvas_draw_str(canvas, 2, 8 + i * 8, text);
    }

    // Progress bar
    uint32_t total = BOOT_LOG_LINES_COUNT + BOOT_DONE_HOLD_TICKS;
    uint8_t pct = (uint8_t)((app->bootRevealed * 100u) / total);
    if(pct > 100) pct = 100;
    canvas_draw_frame(canvas, 2, 58, 124, 5);
    canvas_draw_box(canvas, 3, 59, (uint8_t)((pct * 122u) / 100u), 3);
}

bool CPUStartApp::callbackBootCustomEvent(uint32_t event, void* context) {
    CPUStartApp* app = static_cast<CPUStartApp*>(context);
    if(event == CPUStartCustomEventTick) {
        app->bootRevealed++;
        if(app->bootRevealed >= BOOT_LOG_LINES_COUNT + BOOT_DONE_HOLD_TICKS) {
            uint32_t target = app->bootTargetView;
            view_dispatcher_switch_to_view(app->viewDispatcher, target);
            return true;
        }
        view_commit_model(app->bootView, true);
    }
    return true;
}

// ===================== Fake apps =====================
bool CPUStartApp::callbackFakeAppInput(InputEvent* event, void* context) {
    CPUStartApp* app = static_cast<CPUStartApp*>(context);
    switch(app->fakeAppIndex) {
    case CPUStartFakeRadio:
        if(event->type == InputTypeShort || event->type == InputTypeRepeat) {
            if(event->key == InputKeyUp) {
                app->fakeAppSelected =
                    (app->fakeAppSelected + RADIO_STATIONS_COUNT - 1) % RADIO_STATIONS_COUNT;
            } else if(event->key == InputKeyDown) {
                app->fakeAppSelected =
                    (app->fakeAppSelected + 1) % RADIO_STATIONS_COUNT;
            } else if(event->key == InputKeyOk) {
                app->fakeAppState = !app->fakeAppState;
                app->fakeAppStartTick = furi_get_tick();
            }
        }
        break;
    case CPUStartFakeRecorder:
        if(event->type == InputTypeShort && event->key == InputKeyOk) {
            app->fakeAppState = !app->fakeAppState;
            app->fakeAppStartTick = furi_get_tick();
        }
        break;
    case CPUStartFakeWalkie:
        if(event->type == InputTypeShort || event->type == InputTypeRepeat) {
            if(event->key == InputKeyUp) {
                app->fakeAppSelected =
                    (app->fakeAppSelected + WALKIE_CHANNELS_COUNT - 1) % WALKIE_CHANNELS_COUNT;
            } else if(event->key == InputKeyDown) {
                app->fakeAppSelected =
                    (app->fakeAppSelected + 1) % WALKIE_CHANNELS_COUNT;
            } else if(event->key == InputKeyOk) {
                app->fakeAppState = !app->fakeAppState;
                app->fakeAppStartTick = furi_get_tick();
            }
        }
        break;
    case CPUStartFakeTV:
        if(event->type == InputTypeShort || event->type == InputTypeRepeat) {
            if(event->key == InputKeyUp) {
                app->fakeAppSelected =
                    (app->fakeAppSelected + TV_CHANNELS_COUNT - 1) % TV_CHANNELS_COUNT;
            } else if(event->key == InputKeyDown) {
                app->fakeAppSelected =
                    (app->fakeAppSelected + 1) % TV_CHANNELS_COUNT;
            } else if(event->key == InputKeyOk) {
                app->fakeAppState = !app->fakeAppState;
            }
        }
        break;
    case CPUStartFakeRouter:
        if(event->type == InputTypeShort && event->key == InputKeyOk) {
            app->fakeAppState = 1;
            app->fakeAppStartTick = furi_get_tick();
        }
        break;
    case CPUStartFakeSnake:
        if(event->type == InputTypeShort || event->type == InputTypeRepeat) {
            if(event->key == InputKeyUp && app->snakeDir != 2) {
                app->snakeDir = 0;
            } else if(event->key == InputKeyDown && app->snakeDir != 0) {
                app->snakeDir = 2;
            } else if(event->key == InputKeyLeft && app->snakeDir != 1) {
                app->snakeDir = 3;
            } else if(event->key == InputKeyRight && app->snakeDir != 3) {
                app->snakeDir = 1;
            } else if(event->key == InputKeyOk) {
                if(app->snakeOver) {
                    snakeInit(app);
                } else if(!app->snakeStarted) {
                    app->snakeStarted = true;
                } else {
                    app->snakePaused = !app->snakePaused;
                }
            }
        }
        break;
    case CPUStartFakeTetris:
        if(event->type == InputTypeShort || event->type == InputTypeRepeat) {
            if(event->key == InputKeyLeft) {
                tetrisMove(app, -1);
            } else if(event->key == InputKeyRight) {
                tetrisMove(app, 1);
            } else if(event->key == InputKeyUp) {
                tetrisRotate(app);
            } else if(event->key == InputKeyDown) {
                if(!app->tetrisOver) {
                    app->tetrisSoftDrop = true;
                    if(tetrisStepDown(app)) {
                        app->tetrisScore += 1;
                    }
                }
            } else if(event->key == InputKeyOk) {
                if(app->tetrisOver) {
                    tetrisInit(app);
                } else {
                    tetrisHardDrop(app);
                }
            }
        } else if(event->type == InputTypeRelease) {
            if(event->key == InputKeyDown) {
                app->tetrisSoftDrop = false;
            }
        }
        break;
    case CPUStartFakePong:
        if(event->type == InputTypeShort || event->type == InputTypeRepeat) {
            if(event->key == InputKeyUp) {
                if(app->pongPaddleL > PLAY_TOP) app->pongPaddleL -= 4;
            } else if(event->key == InputKeyDown) {
                if(app->pongPaddleL < PLAY_BOTTOM - PADDLE_H) app->pongPaddleL += 4;
            } else if(event->key == InputKeyOk) {
                if(app->pongOver) {
                    pongInit(app);
                } else if(!app->pongStarted) {
                    app->pongStarted = true;
                }
            }
        }
        break;
    default:
        break;
    }
    return false;
}

void CPUStartApp::drawRadio(Canvas* canvas, CPUStartApp* app) {
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 2, 10, "Internet Radio");

    canvas_set_font(canvas, FontSecondary);
    for(uint8_t i = 0; i < RADIO_STATIONS_COUNT; i++) {
        uint8_t y = 20 + i * 7;
        if(i == app->fakeAppSelected) {
            canvas_draw_box(canvas, 2, y - 6, 124, 7);
            canvas_set_color(canvas, ColorWhite);
            canvas_draw_str(canvas, 4, y, radioStations[i]);
            canvas_set_color(canvas, ColorBlack);
        } else {
            canvas_draw_str(canvas, 4, y, radioStations[i]);
        }
        canvas_draw_str(canvas, 80, y, radioFreq[i]);
    }

    // Now playing bar
    canvas_draw_line(canvas, 2, 56, 125, 56);
    canvas_set_font(canvas, FontSecondary);
    if(app->fakeAppState) {
        uint32_t sec = (furi_get_tick() - app->fakeAppStartTick) / 1000;
        char now[24];
        snprintf(
            now,
            sizeof(now),
            "NOW: %s  %02lu:%02lu",
            radioStations[app->fakeAppSelected],
            (unsigned long)(sec / 60),
            (unsigned long)(sec % 60));
        canvas_draw_str(canvas, 4, 64, now);

        // Equalizer bars
        uint32_t tick = furi_get_tick();
        for(uint8_t b = 0; b < 5; b++) {
            uint8_t h = 3 + ((tick / 80 + b * 3) % 6);
            canvas_draw_line(canvas, 88 + b * 8, 55, 88 + b * 8, 55 - h);
        }
    } else {
        canvas_draw_str(canvas, 4, 64, "Press OK to play");
    }
}

void CPUStartApp::drawRecorder(Canvas* canvas, CPUStartApp* app) {
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 2, 10, "Voice Recorder");

    // Level meter
    uint32_t tick = furi_get_tick();
    for(uint8_t b = 0; b < 10; b++) {
        uint8_t h = app->fakeAppState ? (2 + ((tick / 50 + b * 5) % 10)) : 2;
        canvas_draw_line(canvas, 14 + b * 10, 26, 14 + b * 10, 26 - h);
    }

    // Timer
    canvas_set_font(canvas, FontBigNumbers);
    uint32_t sec = app->fakeAppState ? (furi_get_tick() - app->fakeAppStartTick) / 1000 : 0;
    char time[16];
    snprintf(
        time,
        sizeof(time),
        "%02lu:%02lu",
        (unsigned long)(sec / 60),
        (unsigned long)(sec % 60));
    canvas_draw_str(canvas, 28, 46, time);

    // REC indicator
    canvas_set_font(canvas, FontPrimary);
    if(app->fakeAppState && ((tick / 400) % 2 == 0)) {
        canvas_set_color(canvas, ColorWhite);
        canvas_draw_box(canvas, 6, 38, 22, 8);
        canvas_set_color(canvas, ColorBlack);
        canvas_draw_str(canvas, 8, 45, "REC");
        canvas_set_color(canvas, ColorBlack);
    } else if(!app->fakeAppState) {
        canvas_draw_str(canvas, 6, 45, "REC");
    }

    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 2, 62, app->fakeAppState ? "Press OK to stop" : "Press OK to record");
}

void CPUStartApp::drawWalkie(Canvas* canvas, CPUStartApp* app) {
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 2, 10, "Walkie Talkie");

    // Channel + frequency
    canvas_set_font(canvas, FontSecondary);
    char ch[16];
    snprintf(ch, sizeof(ch), "Channel %02lu", (unsigned long)(app->fakeAppSelected + 1));
    canvas_draw_str(canvas, 4, 24, ch);
    canvas_draw_str(canvas, 4, 34, walkieChannels[app->fakeAppSelected]);

    // Signal bars
    uint32_t tick = furi_get_tick();
    uint8_t level = 1 + ((tick / 200) % 5);
    for(uint8_t b = 0; b < 5; b++) {
        uint8_t h = (b < level) ? (uint8_t)(4 + b * 2) : 1;
        canvas_draw_line(canvas, 76 + b * 9, 26, 76 + b * 9, 26 - h);
    }

    // TX / RX indicator
    canvas_draw_frame(canvas, 76, 34, 50, 10);
    if(app->fakeAppState && ((tick / 200) % 2 == 0)) {
        canvas_draw_box(canvas, 77, 35, 48, 8);
        canvas_set_color(canvas, ColorWhite);
        canvas_draw_str(canvas, 84, 42, "TX");
        canvas_set_color(canvas, ColorBlack);
    } else {
        canvas_draw_str(canvas, 84, 42, "RX");
    }

    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(
        canvas, 2, 62, app->fakeAppState ? "Releasing channel..." : "OK = Push to talk");
}

void CPUStartApp::drawTV(Canvas* canvas, CPUStartApp* app) {
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 2, 10, "TV Media Box");

    canvas_set_font(canvas, FontSecondary);
    char ch[16];
    snprintf(ch, sizeof(ch), "CH %02lu", (unsigned long)(app->fakeAppSelected + 1));
    canvas_draw_str(canvas, 4, 24, ch);

    // Now playing
    canvas_draw_str(canvas, 4, 34, tvChannels[app->fakeAppSelected]);
    canvas_draw_str(canvas, 4, 44, app->fakeAppState ? "ON AIR" : "MUTED");

    // Signal / volume bars
    uint32_t tick = furi_get_tick();
    uint8_t level = app->fakeAppState ? (3 + ((tick / 150) % 3)) : 1;
    for(uint8_t b = 0; b < 6; b++) {
        uint8_t h = (b < level) ? (uint8_t)(4 + b * 2) : 1;
        canvas_draw_line(canvas, 76 + b * 8, 26, 76 + b * 8, 26 - h);
    }

    // Animated noise when on air
    if(app->fakeAppState && ((tick / 120) % 2 == 0)) {
        canvas_draw_line(canvas, 4, 50, 123, 50);
    }

    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 2, 62, app->fakeAppState ? "OK = mute" : "OK = unmute");
}

void CPUStartApp::drawRouter(Canvas* canvas, CPUStartApp* app) {
    uint32_t tick = furi_get_tick();

    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 2, 10, "Router Admin");

    // Status LED (blinks when online)
    if(app->fakeAppState == 0) {
        if((tick / 300) % 2 == 0) {
            canvas_draw_disc(canvas, 118, 8, 3);
        } else {
            canvas_draw_frame(canvas, 115, 5, 6, 6);
        }
    } else {
        canvas_draw_frame(canvas, 115, 5, 6, 6);
    }

    canvas_set_font(canvas, FontSecondary);

    if(app->fakeAppState == 0) {
        char line[32];

        canvas_draw_str(canvas, 4, 24, "WAN   203.0.113.7");
        canvas_draw_str(canvas, 4, 34, "LAN   192.168.1.1");
        canvas_draw_str(canvas, 4, 44, "Clients  4");

        uint32_t uptime = (tick - app->fakeAppStartTick) / 1000;
        snprintf(
            line,
            sizeof(line),
            "Uptime %02lu:%02lu",
            (unsigned long)(uptime / 60) % 60,
            (unsigned long)uptime % 60);
        canvas_draw_str(canvas, 4, 54, line);

        // Animated traffic
        uint8_t speed = 2 + ((tick / 60) % 9);
        char tr[16];
        snprintf(tr, sizeof(tr), "%lu KB/s", (unsigned long)speed);
        canvas_draw_str(canvas, 4, 64, tr);
        for(uint8_t b = 0; b < 8; b++) {
            uint8_t h = 2 + ((tick / 40 + b * 3) % 5);
            canvas_draw_line(canvas, 40 + b * 11, 63, 40 + b * 11, 63 - h);
        }
    } else {
        canvas_draw_str(canvas, 4, 24, "Rebooting...");
        uint8_t dots = ((tick / 250) % 3) + 1;
        char dotsBuf[8];
        for(uint8_t i = 0; i < dots; i++) dotsBuf[i] = '.';
        dotsBuf[dots] = '\0';
        canvas_draw_str(canvas, 4, 36, dotsBuf);
        canvas_draw_str(canvas, 4, 54, "Flushing connections...");
    }
}

void CPUStartApp::drawSnake(Canvas* canvas, CPUStartApp* app) {
    canvas_set_font(canvas, FontSecondary);
    char buf[24];
    snprintf(buf, sizeof(buf), "Snake   Score %lu", (unsigned long)app->snakeScore);
    canvas_draw_str(canvas, 2, 8, buf);

    // Board frame (32 cols x 13 rows, 4px cells)
    canvas_draw_frame(canvas, 0, 10, 128, 54);

    // Food
    canvas_draw_box(canvas, app->snakeFoodX * 4, 11 + app->snakeFoodY * 4, 4, 4);

    // Body
    for(uint8_t i = 0; i < app->snakeLen; i++) {
        canvas_draw_box(
            canvas, app->snakeBody[i][0] * 4, 11 + app->snakeBody[i][1] * 4, 4, 4);
    }

    if(!app->snakeStarted) {
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str(canvas, 16, 40, "Press OK to start");
    } else if(app->snakePaused) {
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str(canvas, 34, 40, "PAUSED");
    } else if(app->snakeOver) {
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str(canvas, 28, 40, "GAME OVER");
    }
}

void CPUStartApp::drawTetris(Canvas* canvas, CPUStartApp* app) {
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 2, 8, "Tetris");

    // Board frame (10 cols x 12 rows, 4px cells)
    canvas_draw_frame(canvas, 2, 2, 44, 52);

    // Locked cells
    for(uint8_t x = 0; x < TETRIS_COLS; x++) {
        for(uint8_t y = 0; y < TETRIS_ROWS; y++) {
            if(app->tetrisBoard[x][y] != 0) {
                canvas_draw_box(canvas, 4 + x * 4, 4 + y * 4, 4, 4);
            }
        }
    }

    // Current piece
    for(uint8_t i = 0; i < 4; i++) {
        int8_t bx = app->tetrisOX + app->tetrisCells[i][0];
        int8_t by = app->tetrisOY + app->tetrisCells[i][1];
        if(bx >= 0 && bx < TETRIS_COLS && by >= 0 && by < TETRIS_ROWS) {
            canvas_draw_box(canvas, 4 + bx * 4, 4 + by * 4, 4, 4);
        }
    }

    // Right panel: next piece + score
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 52, 12, "Next");
    for(uint8_t i = 0; i < 4; i++) {
        uint8_t cx = tetrisShapes[app->tetrisNext][i][0];
        uint8_t cy = tetrisShapes[app->tetrisNext][i][1];
        canvas_draw_box(canvas, 54 + cx * 5, 18 + cy * 5, 5, 5);
    }

    canvas_draw_str(canvas, 52, 46, "Score");
    char buf[16];
    snprintf(buf, sizeof(buf), "%lu", (unsigned long)app->tetrisScore);
    canvas_set_font(canvas, FontBigNumbers);
    canvas_draw_str(canvas, 52, 58, buf);

    if(app->tetrisOver) {
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str(canvas, 10, 34, "GAME OVER");
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str(canvas, 10, 44, "OK = restart");
    }
}

void CPUStartApp::drawPong(Canvas* canvas, CPUStartApp* app) {
    canvas_set_font(canvas, FontBigNumbers);
    char buf[8];
    snprintf(buf, sizeof(buf), "%lu", (unsigned long)app->pongScoreL);
    canvas_draw_str(canvas, 40, 14, buf);
    snprintf(buf, sizeof(buf), "%lu", (unsigned long)app->pongScoreR);
    canvas_draw_str(canvas, 78, 14, buf);

    // Center line
    for(uint8_t y = 18; y < 63; y += 6) {
        canvas_draw_line(canvas, 64, y, 64, y + 3);
    }

    // Paddles + ball
    canvas_draw_box(canvas, 6, app->pongPaddleL, 4, PADDLE_H);
    canvas_draw_box(canvas, 118, app->pongPaddleR, 4, PADDLE_H);
    canvas_draw_box(canvas, app->pongBallX, app->pongBallY, 4, 4);

    if(!app->pongStarted) {
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str(canvas, 24, 34, "Press OK to start");
    } else if(app->pongOver) {
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str(canvas, 24, 34, "GAME OVER");
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str(canvas, 28, 44, "OK = play again");
    }
}

void CPUStartApp::callbackFakeAppDraw(Canvas* canvas, void* context) {
    CPUStartApp* app = *(CPUStartApp**)context;
    canvas_clear(canvas);
    switch(app->fakeAppIndex) {
    case CPUStartFakeRadio:
        drawRadio(canvas, app);
        break;
    case CPUStartFakeRecorder:
        drawRecorder(canvas, app);
        break;
    case CPUStartFakeWalkie:
        drawWalkie(canvas, app);
        break;
    case CPUStartFakeTV:
        drawTV(canvas, app);
        break;
    case CPUStartFakeRouter:
        drawRouter(canvas, app);
        break;
    case CPUStartFakeSnake:
        drawSnake(canvas, app);
        break;
    case CPUStartFakeTetris:
        drawTetris(canvas, app);
        break;
    case CPUStartFakePong:
        drawPong(canvas, app);
        break;
    default:
        break;
    }
}

bool CPUStartApp::callbackFakeAppCustomEvent(uint32_t event, void* context) {
    CPUStartApp* app = static_cast<CPUStartApp*>(context);
    if(event == CPUStartCustomEventTick) {
        if(app->fakeAppIndex == CPUStartFakeRouter && app->fakeAppState == 1) {
            if(furi_get_tick() - app->fakeAppStartTick > 3000) {
                app->fakeAppState = 0;
                app->fakeAppStartTick = furi_get_tick();
            }
        } else if(app->fakeAppIndex == CPUStartFakeSnake) {
            snakeUpdate(app);
        } else if(app->fakeAppIndex == CPUStartFakeTetris) {
            tetrisUpdate(app);
        } else if(app->fakeAppIndex == CPUStartFakePong) {
            pongUpdate(app);
        }
        view_commit_model(app->fakeAppView, true);
    }
    return true;
}

// ===================== Games =====================
// ---- Snake ----
void CPUStartApp::snakeInit(CPUStartApp* app) {
    app->snakeLen = 3;
    app->snakeBody[0][0] = 16;
    app->snakeBody[0][1] = 6;
    app->snakeBody[1][0] = 15;
    app->snakeBody[1][1] = 6;
    app->snakeBody[2][0] = 14;
    app->snakeBody[2][1] = 6;
    app->snakeDir = 1;
    app->snakeScore = 0;
    app->snakeStarted = false;
    app->snakePaused = false;
    app->snakeOver = false;
    snakePlaceFood(app);
}

bool CPUStartApp::snakeOccupies(CPUStartApp* app, uint8_t x, uint8_t y) {
    for(uint8_t i = 0; i < app->snakeLen; i++) {
        if(app->snakeBody[i][0] == x && app->snakeBody[i][1] == y) return true;
    }
    return false;
}

void CPUStartApp::snakePlaceFood(CPUStartApp* app) {
    do {
        app->snakeFoodX = furi_hal_random_get() % SNAKE_COLS;
        app->snakeFoodY = furi_hal_random_get() % SNAKE_ROWS;
    } while(snakeOccupies(app, app->snakeFoodX, app->snakeFoodY));
}

void CPUStartApp::snakeUpdate(CPUStartApp* app) {
    if(!app->snakeStarted || app->snakePaused || app->snakeOver) return;

    int8_t dx = 0, dy = 0;
    switch(app->snakeDir) {
    case 0: dy = -1; break;
    case 1: dx = 1; break;
    case 2: dy = 1; break;
    case 3: dx = -1; break;
    }

    int16_t nx = app->snakeBody[0][0] + dx;
    int16_t ny = app->snakeBody[0][1] + dy;

    if(nx < 0 || nx >= SNAKE_COLS || ny < 0 || ny >= SNAKE_ROWS) {
        app->snakeOver = true;
        return;
    }
    if(snakeOccupies(app, nx, ny)) {
        app->snakeOver = true;
        return;
    }

    bool eating = (nx == app->snakeFoodX && ny == app->snakeFoodY);
    if(eating) {
        if(app->snakeLen < SNAKE_MAX) {
            for(uint8_t i = app->snakeLen; i > 0; i--) {
                app->snakeBody[i][0] = app->snakeBody[i - 1][0];
                app->snakeBody[i][1] = app->snakeBody[i - 1][1];
            }
            app->snakeLen++;
        }
        app->snakeScore += 10;
        snakePlaceFood(app);
    } else {
        for(uint8_t i = app->snakeLen - 1; i > 0; i--) {
            app->snakeBody[i][0] = app->snakeBody[i - 1][0];
            app->snakeBody[i][1] = app->snakeBody[i - 1][1];
        }
    }
    app->snakeBody[0][0] = nx;
    app->snakeBody[0][1] = ny;
}

// ---- Tetris ----
void CPUStartApp::tetrisInit(CPUStartApp* app) {
    memset(app->tetrisBoard, 0, sizeof(app->tetrisBoard));
    app->tetrisScore = 0;
    app->tetrisGrav = 0;
    app->tetrisSoftDrop = false;
    app->tetrisOver = false;
    app->tetrisPiece = furi_hal_random_get() % 7;
    app->tetrisNext = furi_hal_random_get() % 7;
    tetrisSpawn(app);
}

void CPUStartApp::tetrisSpawn(CPUStartApp* app) {
    app->tetrisOX = 3;
    app->tetrisOY = -1;
    for(uint8_t i = 0; i < 4; i++) {
        app->tetrisCells[i][0] = tetrisShapes[app->tetrisPiece][i][0];
        app->tetrisCells[i][1] = tetrisShapes[app->tetrisPiece][i][1];
    }
}

bool CPUStartApp::tetrisCollides(
    CPUStartApp* app, int8_t ox, int8_t oy, const uint8_t cells[4][2]) {
    for(uint8_t i = 0; i < 4; i++) {
        int8_t bx = ox + cells[i][0];
        int8_t by = oy + cells[i][1];
        if(bx < 0 || bx >= TETRIS_COLS) return true;
        if(by >= TETRIS_ROWS) return true;
        if(by >= 0 && app->tetrisBoard[bx][by] != 0) return true;
    }
    return false;
}

bool CPUStartApp::tetrisStepDown(CPUStartApp* app) {
    int8_t ny = app->tetrisOY + 1;
    if(!tetrisCollides(app, app->tetrisOX, ny, app->tetrisCells)) {
        app->tetrisOY = ny;
        return true;
    }
    tetrisLock(app);
    return false;
}

void CPUStartApp::tetrisMove(CPUStartApp* app, int8_t dx) {
    if(app->tetrisOver) return;
    int8_t nx = app->tetrisOX + dx;
    if(!tetrisCollides(app, nx, app->tetrisOY, app->tetrisCells)) {
        app->tetrisOX = nx;
    }
}

void CPUStartApp::tetrisRotate(CPUStartApp* app) {
    if(app->tetrisOver) return;
    uint8_t rcells[4][2];
    for(uint8_t i = 0; i < 4; i++) {
        rcells[i][0] = 3 - app->tetrisCells[i][1];
        rcells[i][1] = app->tetrisCells[i][0];
    }
    if(!tetrisCollides(app, app->tetrisOX, app->tetrisOY, rcells)) {
        memcpy(app->tetrisCells, rcells, sizeof(rcells));
    } else if(!tetrisCollides(app, app->tetrisOX - 1, app->tetrisOY, rcells)) {
        app->tetrisOX--;
        memcpy(app->tetrisCells, rcells, sizeof(rcells));
    } else if(!tetrisCollides(app, app->tetrisOX + 1, app->tetrisOY, rcells)) {
        app->tetrisOX++;
        memcpy(app->tetrisCells, rcells, sizeof(rcells));
    } else if(!tetrisCollides(app, app->tetrisOX, app->tetrisOY - 1, rcells)) {
        app->tetrisOY--;
        memcpy(app->tetrisCells, rcells, sizeof(rcells));
    }
}

void CPUStartApp::tetrisHardDrop(CPUStartApp* app) {
    if(app->tetrisOver) return;
    while(tetrisStepDown(app)) {
        app->tetrisScore += 2;
    }
    app->tetrisSoftDrop = false;
}

void CPUStartApp::tetrisLock(CPUStartApp* app) {
    for(uint8_t i = 0; i < 4; i++) {
        int8_t bx = app->tetrisOX + app->tetrisCells[i][0];
        int8_t by = app->tetrisOY + app->tetrisCells[i][1];
        if(by < 0) {
            app->tetrisOver = true;
            return;
        }
        app->tetrisBoard[bx][by] = app->tetrisPiece + 1;
    }
    tetrisClearLines(app);
    app->tetrisPiece = app->tetrisNext;
    app->tetrisNext = furi_hal_random_get() % 7;
    tetrisSpawn(app);
    if(tetrisCollides(app, app->tetrisOX, app->tetrisOY, app->tetrisCells)) {
        app->tetrisOver = true;
    }
}

void CPUStartApp::tetrisClearLines(CPUStartApp* app) {
    uint8_t cleared = 0;
    for(int8_t y = TETRIS_ROWS - 1; y >= 0; y--) {
        bool full = true;
        for(uint8_t x = 0; x < TETRIS_COLS; x++) {
            if(app->tetrisBoard[x][y] == 0) {
                full = false;
                break;
            }
        }
        if(full) {
            for(int8_t yy = y; yy > 0; yy--) {
                for(uint8_t x = 0; x < TETRIS_COLS; x++) {
                    app->tetrisBoard[x][yy] = app->tetrisBoard[x][yy - 1];
                }
            }
            for(uint8_t x = 0; x < TETRIS_COLS; x++) {
                app->tetrisBoard[x][0] = 0;
            }
            y++;
            cleared++;
        }
    }
    if(cleared) {
        app->tetrisScore += cleared * 100;
    }
}

void CPUStartApp::tetrisUpdate(CPUStartApp* app) {
    if(app->tetrisOver) return;
    if(app->tetrisSoftDrop) {
        if(tetrisStepDown(app)) {
            app->tetrisScore += 1;
        } else {
            app->tetrisSoftDrop = false;
        }
    } else {
        app->tetrisGrav++;
        if(app->tetrisGrav >= 2) {
            app->tetrisGrav = 0;
            tetrisStepDown(app);
        }
    }
}

// ---- Pong ----
void CPUStartApp::pongInit(CPUStartApp* app) {
    app->pongPaddleL = (PLAY_TOP + PLAY_BOTTOM - PADDLE_H) / 2;
    app->pongPaddleR = (PLAY_TOP + PLAY_BOTTOM - PADDLE_H) / 2;
    app->pongScoreL = 0;
    app->pongScoreR = 0;
    app->pongSpeed = 3;
    app->pongStarted = false;
    app->pongOver = false;
    pongResetBall(app);
}

void CPUStartApp::pongResetBall(CPUStartApp* app) {
    app->pongBallX = 62;
    app->pongBallY = 40;
    app->pongBallVX = (app->pongScoreL > app->pongScoreR) ? -app->pongSpeed : app->pongSpeed;
    app->pongBallVY = (furi_hal_random_get() % 2) ? 1 : -1;
}

void CPUStartApp::pongUpdate(CPUStartApp* app) {
    if(!app->pongStarted || app->pongOver) return;

    // AI paddle chases the ball
    if(app->pongBallY < app->pongPaddleR + PADDLE_H / 2 - 2 && app->pongPaddleR > PLAY_TOP) {
        app->pongPaddleR -= 2;
    } else if(
        app->pongBallY > app->pongPaddleR + PADDLE_H / 2 + 2 &&
        app->pongPaddleR < PLAY_BOTTOM - PADDLE_H) {
        app->pongPaddleR += 2;
    }

    app->pongBallX += app->pongBallVX;
    app->pongBallY += app->pongBallVY;

    if(app->pongBallY <= PLAY_TOP + 1) {
        app->pongBallY = PLAY_TOP + 1;
        app->pongBallVY = -app->pongBallVY;
    }
    if(app->pongBallY >= PLAY_BOTTOM - 1) {
        app->pongBallY = PLAY_BOTTOM - 1;
        app->pongBallVY = -app->pongBallVY;
    }

    // Left paddle (player)
    if(app->pongBallX <= 10 && app->pongBallX >= 4 && app->pongBallVX < 0 &&
       app->pongBallY >= app->pongPaddleL - 2 &&
       app->pongBallY <= app->pongPaddleL + PADDLE_H) {
        app->pongBallX = 10;
        app->pongBallVX = app->pongSpeed;
        int8_t hit = (app->pongBallY - app->pongPaddleL) - PADDLE_H / 2;
        app->pongBallVY = hit / 3;
        if(app->pongBallVY == 0) app->pongBallVY = (hit >= 0) ? 1 : -1;
    }

    // Right paddle (AI)
    if(app->pongBallX >= 112 && app->pongBallX <= 120 && app->pongBallVX > 0 &&
       app->pongBallY >= app->pongPaddleR - 2 &&
       app->pongBallY <= app->pongPaddleR + PADDLE_H) {
        app->pongBallX = 112;
        app->pongBallVX = -app->pongSpeed;
        int8_t hit = (app->pongBallY - app->pongPaddleR) - PADDLE_H / 2;
        app->pongBallVY = hit / 3;
        if(app->pongBallVY == 0) app->pongBallVY = (hit >= 0) ? 1 : -1;
    }

    if(app->pongBallX < 0) {
        app->pongScoreR++;
        pongResetBall(app);
        if(app->pongScoreR >= 10) app->pongOver = true;
    }
    if(app->pongBallX > 128) {
        app->pongScoreL++;
        pongResetBall(app);
        if(app->pongScoreL >= 10) app->pongOver = true;
    }
}

// ===================== Terminal =====================
bool CPUStartApp::callbackTerminalInput(InputEvent* event, void* context) {
    CPUStartApp* app = static_cast<CPUStartApp*>(context);
    if(event->type == InputTypeShort) {
        if(event->key == InputKeyOk) {
            app->termInput[0] = '\0';
            view_dispatcher_switch_to_view(app->viewDispatcher, CPUStartViewTerminalInput);
            return true;
        }
    }
    return false;
}

void CPUStartApp::callbackTerminalInputResult(void* context) {
    CPUStartApp* app = static_cast<CPUStartApp*>(context);
    char line[80];
    snprintf(line, sizeof(line), "mmc@2a31:~$ %s", app->termInput);

    char output[96];
    if(app->termInput[0] == '\0') {
        snprintf(output, sizeof(output), "%s", "");
    } else if(strncmp(app->termInput, "help", 4) == 0) {
        snprintf(output, sizeof(output), "Available: help ls cat echo uname");
    } else if(strncmp(app->termInput, "ls", 2) == 0) {
        snprintf(output, sizeof(output), "boot  dev  etc  home  proc  root");
    } else if(strncmp(app->termInput, "cat /etc/os-release", 20) == 0) {
        snprintf(output, sizeof(output), "FlipperOS RISC-V @ 64MHz");
    } else if(strncmp(app->termInput, "uname", 5) == 0) {
        snprintf(output, sizeof(output), "Linux flipper 6.1.0-flipper #1");
    } else if(strncmp(app->termInput, "echo", 4) == 0) {
        const char* rest = app->termInput + 5;
        snprintf(output, sizeof(output), "%s", rest);
    } else if(strncmp(app->termInput, "clear", 5) == 0) {
        app->termLineCount = 0;
        for(uint8_t i = 0; i < 6; i++) {
            app->termLines[i][0] = '\0';
        }
        view_dispatcher_switch_to_view(app->viewDispatcher, CPUStartViewTerminal);
        return;
    } else {
        snprintf(output, sizeof(output), "command not found: %s", app->termInput);
    }

    uint8_t count = (output[0] != '\0') ? 2 : 1;
    uint8_t newCount = app->termLineCount + count;
    if(newCount > 6) {
        uint8_t shift = newCount - 6;
        for(uint8_t i = 0; i + shift < 6; i++) {
            memcpy(app->termLines[i], app->termLines[i + shift], sizeof(app->termLines[i]));
        }
        newCount = 6;
    }
    app->termLineCount = newCount;
    snprintf(
        app->termLines[app->termLineCount - count],
        sizeof(app->termLines[app->termLineCount - count]),
        "%.40s",
        line);
    snprintf(
        app->termLines[app->termLineCount - 1],
        sizeof(app->termLines[app->termLineCount - 1]),
        "%.40s",
        output);

    view_dispatcher_switch_to_view(app->viewDispatcher, CPUStartViewTerminal);
}

uint32_t CPUStartApp::callbackReturnToTerminal(void* context) {
    UNUSED(context);
    return CPUStartViewTerminal;
}

void CPUStartApp::callbackTerminalDraw(Canvas* canvas, void* context) {
    CPUStartApp* app = *(CPUStartApp**)context;
    canvas_clear(canvas);
    canvas_set_font(canvas, FontKeyboard);

    uint8_t start = app->termLineCount >= 6 ? 0 : (6 - app->termLineCount);
    for(uint8_t i = 0; i < app->termLineCount; i++) {
        canvas_draw_str(canvas, 2, 10 + (start + i) * 8, app->termLines[i]);
    }

    // Blinking cursor at the prompt
    uint32_t tick = furi_get_tick();
    if((tick / 400) % 2 == 0) {
        canvas_draw_str(canvas, 2, 58, "mmc@2a31:~$");
    }
    if((tick / 400) % 2 == 1) {
        canvas_draw_str(canvas, 2, 58, "mmc@2a31:~$ ");
        canvas_draw_box(canvas, 94, 54, 4, 5);
    }
}

bool CPUStartApp::callbackTerminalCustomEvent(uint32_t event, void* context) {
    CPUStartApp* app = static_cast<CPUStartApp*>(context);
    if(event == CPUStartCustomEventTick) {
        view_commit_model(app->terminalView, true);
    }
    return true;
}

// ===================== Files =====================
struct FakeFile {
    const char* name;
    const char* size;
    bool isDir;
};

static const FakeFile filesRoot[] = {
    {"home", "", true},
    {"etc", "", true},
    {"var", "", true},
    {"boot.log", "2.1 KB", false},
    {"config.txt", "1.2 KB", false},
    {"readme.md", "3.0 KB", false},
};
static const FakeFile filesHome[] = {
    {"user", "", true},
    {"photos", "", true},
    {"notes.txt", "500 B", false},
    {".bashrc", "4.2 KB", false},
};
static const FakeFile filesEtc[] = {
    {"hosts", "220 B", false},
    {"passwd", "1.5 KB", false},
    {"os-release", "300 B", false},
};
static const FakeFile filesVar[] = {
    {"log", "", true},
    {"cache", "", true},
    {"tmp", "", false},
};

const FakeFile* CPUStartApp::filesCurrent(uint8_t* count) {
    if(fileLevel == 0) {
        *count = COUNT_OF(filesRoot);
        return filesRoot;
    }
    if(fileDir == 0) {
        *count = COUNT_OF(filesHome);
        return filesHome;
    }
    if(fileDir == 1) {
        *count = COUNT_OF(filesEtc);
        return filesEtc;
    }
    *count = COUNT_OF(filesVar);
    return filesVar;
}

bool CPUStartApp::callbackFilesInput(InputEvent* event, void* context) {
    CPUStartApp* app = static_cast<CPUStartApp*>(context);
    if(event->type != InputTypeShort) return false;
    uint8_t count = 0;
    const FakeFile* list = app->filesCurrent(&count);

    if(event->key == InputKeyUp) {
        if(app->fileIndex > 0) app->fileIndex--;
        return true;
    }
    if(event->key == InputKeyDown) {
        if(app->fileIndex < count - 1) app->fileIndex++;
        return true;
    }
    if(event->key == InputKeyOk) {
        if(list[app->fileIndex].isDir) {
            app->fileDir = app->fileIndex;
            app->fileLevel = 1;
            app->fileIndex = 0;
        }
        return true;
    }
    if(event->key == InputKeyBack) {
        if(app->fileLevel == 1) {
            app->fileLevel = 0;
            app->fileIndex = app->fileDir;
            return true;
        }
    }
    return false;
}

void CPUStartApp::callbackFilesDraw(Canvas* canvas, void* context) {
    CPUStartApp* app = *(CPUStartApp**)context;
    canvas_clear(canvas);
    canvas_set_font(canvas, FontSecondary);

    canvas_draw_str(canvas, 4, 8, app->fileLevel == 0 ? "Files  /" : "Files  /");

    canvas_draw_line(canvas, 0, 10, 128, 10);

    uint8_t count = 0;
    const FakeFile* list = app->filesCurrent(&count);

    for(uint8_t i = 0; i < count && i < 5; i++) {
        uint8_t y = 20 + i * 10;
        if(i == app->fileIndex) {
            canvas_draw_box(canvas, 0, y - 6, 128, 10);
            canvas_invert_color(canvas);
        }
        if(list[i].isDir) {
            canvas_draw_str(canvas, 4, y, list[i].name);
            canvas_draw_str(canvas, 118, y, ">");
        } else {
            canvas_draw_str(canvas, 8, y, list[i].name);
            canvas_draw_str(canvas, 100, y, list[i].size);
        }
        if(i == app->fileIndex) {
            canvas_invert_color(canvas);
        }
    }
    if(app->fileLevel == 1) {
        canvas_set_font(canvas, FontKeyboard);
        canvas_draw_str(canvas, 2, 63, "[OK] open   [BACK] up");
    }
}

bool CPUStartApp::callbackFilesCustomEvent(uint32_t event, void* context) {
    CPUStartApp* app = static_cast<CPUStartApp*>(context);
    if(event == CPUStartCustomEventTick) {
        view_commit_model(app->filesView, true);
    }
    return true;
}

// ===================== Network =====================
static const char* const netSSIDs[] = {
    "FlipperNet",
    "CoffeeShop",
    "NeighborWiFi",
    "FreePublic",
    "Hotel_WiFi",
};
#define NET_SSID_COUNT (sizeof(netSSIDs) / sizeof(netSSIDs[0]))

bool CPUStartApp::callbackNetworkInput(InputEvent* event, void* context) {
    CPUStartApp* app = static_cast<CPUStartApp*>(context);
    if(event->type != InputTypeShort) return false;

    if(app->netState == 1) {
        if(event->key == InputKeyUp) {
            if(app->netIndex > 0) app->netIndex--;
            return true;
        }
        if(event->key == InputKeyDown) {
            if(app->netIndex < NET_SSID_COUNT - 1) app->netIndex++;
            return true;
        }
        if(event->key == InputKeyOk) {
            app->netState = 2;
            app->netStartTick = furi_get_tick();
            return true;
        }
    }
    if(app->netState == 3) {
        if(event->key == InputKeyOk) {
            app->netState = 0;
            app->netStartTick = furi_get_tick();
            return true;
        }
    }
    return false;
}

void CPUStartApp::callbackNetworkDraw(Canvas* canvas, void* context) {
    CPUStartApp* app = *(CPUStartApp**)context;
    canvas_clear(canvas);
    canvas_set_font(canvas, FontSecondary);
    uint32_t tick = furi_get_tick();

    if(app->netState == 0) {
        canvas_draw_str(canvas, 4, 8, "WiFi");
        canvas_draw_line(canvas, 0, 10, 128, 10);
        canvas_draw_str(canvas, 20, 32, "Scanning for networks...");
        uint8_t dots = ((tick / 300) % 3) + 1;
        char buf[8];
        for(uint8_t i = 0; i < dots; i++) buf[i] = '.';
        buf[dots] = '\0';
        canvas_draw_str(canvas, 20, 42, buf);
    } else if(app->netState == 1) {
        canvas_draw_str(canvas, 4, 8, "WiFi Networks");
        canvas_draw_line(canvas, 0, 10, 128, 10);
        for(uint8_t i = 0; i < NET_SSID_COUNT; i++) {
            uint8_t y = 20 + i * 9;
            if(i == app->netIndex) {
                canvas_draw_box(canvas, 0, y - 6, 128, 9);
                canvas_invert_color(canvas);
            }
            canvas_draw_str(canvas, 4, y, netSSIDs[i]);
            uint8_t bars = ((tick / 600 + i) % 4) + 1;
            for(uint8_t b = 0; b < bars; b++) {
                canvas_draw_line(
                    canvas, 100 + b * 5, y - 1 - b * 2, 100 + b * 5, y - 1);
            }
            if(i == app->netIndex) {
                canvas_invert_color(canvas);
            }
        }
    } else if(app->netState == 2) {
        canvas_draw_str(canvas, 4, 8, "Connecting");
        canvas_draw_line(canvas, 0, 10, 128, 10);
        char buf[48];
        snprintf(buf, sizeof(buf), "Connecting to %s...", netSSIDs[app->netIndex]);
        canvas_draw_str(canvas, 4, 24, buf);
        uint8_t dots = ((tick / 250) % 3) + 1;
        char dotsBuf[8];
        for(uint8_t i = 0; i < dots; i++) dotsBuf[i] = '.';
        dotsBuf[dots] = '\0';
        canvas_draw_str(canvas, 4, 36, dotsBuf);
    } else if(app->netState == 3) {
        canvas_draw_str(canvas, 4, 8, "Connected");
        canvas_draw_line(canvas, 0, 10, 128, 10);
        char buf[48];
        snprintf(buf, sizeof(buf), "Connected to %s", netSSIDs[app->netIndex]);
        canvas_draw_str(canvas, 4, 24, buf);
        canvas_draw_str(canvas, 4, 36, "IP: 192.168.1.42");
        canvas_set_font(canvas, FontKeyboard);
        canvas_draw_str(canvas, 2, 63, "[OK] rescan");
    }
}

bool CPUStartApp::callbackNetworkCustomEvent(uint32_t event, void* context) {
    CPUStartApp* app = static_cast<CPUStartApp*>(context);
    if(event == CPUStartCustomEventTick) {
        uint32_t elapsed = furi_get_tick() - app->netStartTick;
        if(app->netState == 0 && elapsed > 1500) {
            app->netState = 1;
        }
        if(app->netState == 2 && elapsed > 2500) {
            app->netState = 3;
        }
        view_commit_model(app->networkView, true);
    }
    return true;
}

// ===================== Settings =====================
static const char* const setLabels[] = {
    "Brightness",
    "Theme",
    "Wi-Fi",
    "Bluetooth",
    "Volume",
};

void CPUStartApp::settingsValueText(uint8_t i, char* buf, uint8_t len) {
    switch(i) {
    case 0:
        snprintf(buf, len, "%u/5", setValues[i]);
        break;
    case 1:
        snprintf(buf, len, "%s", setValues[i] ? "Dark" : "Light");
        break;
    case 2:
    case 3:
        snprintf(buf, len, "%s", setValues[i] ? "ON" : "OFF");
        break;
    case 4:
        snprintf(buf, len, "%u%%", setValues[i]);
        break;
    default:
        buf[0] = '\0';
    }
}

bool CPUStartApp::callbackSettingsInput(InputEvent* event, void* context) {
    CPUStartApp* app = static_cast<CPUStartApp*>(context);
    if(event->type != InputTypeShort) return false;

    if(event->key == InputKeyUp) {
        if(app->setIndex > 0) app->setIndex--;
        return true;
    }
    if(event->key == InputKeyDown) {
        if(app->setIndex < 4) app->setIndex++;
        return true;
    }
    if(event->key == InputKeyLeft || event->key == InputKeyRight) {
        int8_t dir = (event->key == InputKeyRight) ? 1 : -1;
        switch(app->setIndex) {
        case 0:
            app->setValues[0] = CLAMP(app->setValues[0] + dir, 1, 5);
            break;
        case 1:
        case 2:
        case 3:
            app->setValues[app->setIndex] = app->setValues[app->setIndex] ? 0 : 1;
            break;
        case 4:
            app->setValues[4] = CLAMP(app->setValues[4] + (int8_t)(dir * 10), 0, 100);
            break;
        }
        return true;
    }
    return false;
}

void CPUStartApp::callbackSettingsDraw(Canvas* canvas, void* context) {
    CPUStartApp* app = *(CPUStartApp**)context;
    canvas_clear(canvas);
    canvas_set_font(canvas, FontSecondary);

    canvas_draw_str(canvas, 4, 8, "Settings");
    canvas_draw_line(canvas, 0, 10, 128, 10);

    char val[16];
    for(uint8_t i = 0; i < 5; i++) {
        uint8_t y = 20 + i * 10;
        if(i == app->setIndex) {
            canvas_draw_box(canvas, 0, y - 6, 128, 10);
            canvas_invert_color(canvas);
        }
        canvas_draw_str(canvas, 4, y, setLabels[i]);
        app->settingsValueText(i, val, sizeof(val));
        canvas_draw_str(canvas, 90, y, val);
        if(i == app->setIndex) {
            canvas_invert_color(canvas);
        }
    }
    canvas_set_font(canvas, FontKeyboard);
    canvas_draw_str(canvas, 2, 63, "[<][>] change value");
}

bool CPUStartApp::callbackSettingsCustomEvent(uint32_t event, void* context) {
    CPUStartApp* app = static_cast<CPUStartApp*>(context);
    if(event == CPUStartCustomEventTick) {
        view_commit_model(app->settingsView, true);
    }
    return true;
}

// ===================== U-Boot menu =====================
void CPUStartApp::callbackUBootDraw(Canvas* canvas, void* context) {
    CPUStartApp* app = *(CPUStartApp**)context;

    const char* options[] = {
        "Desktop",
        "TV Media Box",
        "Router",
        "Minimal",
        "No graphics",
    };

    canvas_clear(canvas);

    // Framed box border
    canvas_draw_frame(canvas, 0, 0, 128, 64);

    // Title
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 4, 10, "U-Boot - Boot Menu");

    // Horizontal divider under title
    canvas_draw_line(canvas, 2, 14, 125, 14);

    // Options
    canvas_set_font(canvas, FontSecondary);
    for(uint8_t i = 0; i < 5; i++) {
        uint8_t y = 22 + i * 8;
        if(i == app->ubootSelected) {
            canvas_draw_box(canvas, 2, y - 7, 124, 8);
            canvas_set_color(canvas, ColorWhite);
            canvas_draw_str(canvas, 5, y, options[i]);
            canvas_set_color(canvas, ColorBlack);
        } else {
            canvas_draw_str(canvas, 5, y, options[i]);
        }
    }
    UNUSED(app);
}

bool CPUStartApp::callbackUBootInput(InputEvent* event, void* context) {
    CPUStartApp* app = static_cast<CPUStartApp*>(context);
    if(event->type == InputTypeShort || event->type == InputTypeRepeat) {
        if(event->key == InputKeyUp) {
            if(app->ubootSelected > 0) {
                app->ubootSelected--;
            } else {
                app->ubootSelected = 4;
            }
        } else if(event->key == InputKeyDown) {
            if(app->ubootSelected < 4) {
                app->ubootSelected++;
            } else {
                app->ubootSelected = 0;
            }
        } else if(event->key == InputKeyOk) {
            switch(app->ubootSelected) {
            case CPUStartUBootDesktop:
                app->startBoot("Desktop", CPUStartViewDesktop);
                break;
            case CPUStartUBootTVMediaBox:
                app->fakeAppIndex = CPUStartFakeTV;
                app->fakeAppSelected = 0;
                app->fakeAppState = 1;
                app->fakeAppPrevView = CPUStartViewAppsMenu;
                app->startBoot("TV Media Box", CPUStartViewFakeApp);
                break;
            case CPUStartUBootRouter:
                app->fakeAppIndex = CPUStartFakeRouter;
                app->fakeAppSelected = 0;
                app->fakeAppState = 0;
                app->fakeAppPrevView = CPUStartViewAppsMenu;
                app->fakeAppStartTick = furi_get_tick();
                app->startBoot("Router", CPUStartViewFakeApp);
                break;
            case CPUStartUBootMinimal:
                app->startBoot("Minimal", CPUStartViewMainMenu);
                break;
            case CPUStartUBootNoGraphics:
                app->startBoot("No graphics", CPUStartViewTerminal);
                break;
            default:
                break;
            }
        }
    }
    return false;
}

// ===================== Desktop =====================
void CPUStartApp::callbackDesktopDraw(Canvas* canvas, void* context) {
    CPUStartApp* app = *(CPUStartApp**)context;
    canvas_clear(canvas);
    canvas_set_font(canvas, FontSecondary);

    // Wallpaper: sun + horizon
    canvas_set_color(canvas, ColorBlack);
    canvas_draw_disc(canvas, 110, 12, 8);
    canvas_draw_line(canvas, 0, 40, 128, 40);

    // Desktop icons
    const char* icons[] = {"Computer", "Files", "Apps", "Settings", "Network"};
    for(uint8_t i = 0; i < 5; i++) {
        uint8_t x = 6 + i * 24;
        uint8_t y = 16;
        if(i == app->desktopSelected) {
            canvas_draw_box(canvas, x - 3, y - 3, 22, 22);
            canvas_set_color(canvas, ColorWhite);
            canvas_draw_str(canvas, x, y + 8, icons[i]);
            canvas_set_color(canvas, ColorBlack);
        } else {
            canvas_draw_frame(canvas, x - 3, y - 3, 22, 22);
            canvas_draw_str(canvas, x, y + 8, icons[i]);
        }
    }

    // Taskbar
    canvas_draw_box(canvas, 0, 56, 128, 8);
    canvas_set_color(canvas, ColorWhite);
    canvas_draw_str(canvas, 2, 62, "FlipperOS");

    // Clock (counts up from desktop launch)
    char clock[16];
    uint32_t seconds = (furi_get_tick() - app->desktopStartTick) / 1000;
    snprintf(
        clock,
        sizeof(clock),
        "%02lu:%02lu",
        (unsigned long)(seconds / 60) % 60,
        (unsigned long)seconds % 60);
    canvas_draw_str(canvas, 90, 62, clock);
    canvas_set_color(canvas, ColorBlack);
}

bool CPUStartApp::callbackDesktopInput(InputEvent* event, void* context) {
    CPUStartApp* app = static_cast<CPUStartApp*>(context);
    if(event->type == InputTypeShort || event->type == InputTypeRepeat) {
        if(event->key == InputKeyRight) {
            app->desktopSelected = (app->desktopSelected + 1) % 5;
        } else if(event->key == InputKeyLeft) {
            app->desktopSelected = (app->desktopSelected + 4) % 5;
        } else if(event->key == InputKeyOk) {
            switch(app->desktopSelected) {
            case 0: // Computer
                app->startBoot("Desktop", CPUStartViewDesktop);
                break;
            case 1: // Files
                view_dispatcher_switch_to_view(app->viewDispatcher, CPUStartViewFiles);
                break;
            case 2: // Apps
                view_dispatcher_switch_to_view(app->viewDispatcher, CPUStartViewAppsMenu);
                break;
            case 3: // Settings
                view_dispatcher_switch_to_view(app->viewDispatcher, CPUStartViewSettings);
                break;
            case 4: // Network
                view_dispatcher_switch_to_view(app->viewDispatcher, CPUStartViewNetwork);
                break;
            default:
                break;
            }
        }
    }
    return false;
}

bool CPUStartApp::callbackDesktopCustomEvent(uint32_t event, void* context) {
    CPUStartApp* app = static_cast<CPUStartApp*>(context);
    if(event == CPUStartCustomEventTick) {
        view_commit_model(app->desktopView, true);
    }
    return true;
}

void CPUStartApp::callbackTimer(void* context) {
    CPUStartApp* app = static_cast<CPUStartApp*>(context);
    if(app->viewDispatcher) {
        view_dispatcher_send_custom_event(app->viewDispatcher, CPUStartCustomEventTick);
    }
}

extern "C" {
int32_t cpu_start_main(void* p) {
    UNUSED(p);

    CPUStartApp app;

    app.runDispatcher();

    return 0;
}
}
