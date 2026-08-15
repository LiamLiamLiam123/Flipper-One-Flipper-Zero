#include "app.hpp"
#include "cpu_start_icons.h"

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
#define KODI_MENU_COUNT 6

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
    notification = static_cast<NotificationApp*>(furi_record_open(RECORD_NOTIFICATION));

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

    // ---- Main menu (custom icon menu) ----
    mainMenuView = view_alloc();
    if(!mainMenuView) {
        FURI_LOG_E(TAG, "Failed to allocate main menu view");
        return;
    }
    view_allocate_model(mainMenuView, ViewModelTypeLockFree, sizeof(CPUStartApp*));
    *(CPUStartApp**)view_get_model(mainMenuView) = this;
    view_set_context(mainMenuView, this);
    view_set_draw_callback(mainMenuView, callbackMainMenuDraw);
    view_set_input_callback(mainMenuView, callbackMainMenuInput);
    view_set_previous_callback(mainMenuView, callbackExitApp);
    view_dispatcher_add_view(viewDispatcher, CPUStartViewMainMenu, mainMenuView);

    // ---- Apps menu (custom icon menu) ----
    appsView = view_alloc();
    if(!appsView) {
        FURI_LOG_E(TAG, "Failed to allocate apps view");
        return;
    }
    view_allocate_model(appsView, ViewModelTypeLockFree, sizeof(CPUStartApp*));
    *(CPUStartApp**)view_get_model(appsView) = this;
    view_set_context(appsView, this);
    view_set_draw_callback(appsView, callbackAppsDraw);
    view_set_input_callback(appsView, callbackAppsInput);
    view_set_previous_callback(appsView, callbackReturnToMain);
    view_dispatcher_add_view(viewDispatcher, CPUStartViewAppsMenu, appsView);

    // ---- Games menu (custom icon menu) ----
    gamesView = view_alloc();
    if(!gamesView) {
        FURI_LOG_E(TAG, "Failed to allocate games view");
        return;
    }
    view_allocate_model(gamesView, ViewModelTypeLockFree, sizeof(CPUStartApp*));
    *(CPUStartApp**)view_get_model(gamesView) = this;
    view_set_context(gamesView, this);
    view_set_draw_callback(gamesView, callbackGamesDraw);
    view_set_input_callback(gamesView, callbackGamesInput);
    view_set_previous_callback(gamesView, callbackReturnToApps);
    view_dispatcher_add_view(viewDispatcher, CPUStartViewGamesMenu, gamesView);

    // ---- Boot profiles (custom icon menu) ----
    bootProfilesView = view_alloc();
    if(!bootProfilesView) {
        FURI_LOG_E(TAG, "Failed to allocate boot profiles view");
        return;
    }
    view_allocate_model(bootProfilesView, ViewModelTypeLockFree, sizeof(CPUStartApp*));
    *(CPUStartApp**)view_get_model(bootProfilesView) = this;
    view_set_context(bootProfilesView, this);
    view_set_draw_callback(bootProfilesView, callbackBootProfilesDraw);
    view_set_input_callback(bootProfilesView, callbackBootProfilesInput);
    view_set_previous_callback(bootProfilesView, callbackReturnToMain);
    view_dispatcher_add_view(viewDispatcher, CPUStartViewBootProfiles, bootProfilesView);

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
    view_set_previous_callback(terminalView, callbackReturnFromTerminal);
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

    // ---- Testing view (custom) ----
    testingView = view_alloc();
    if(!testingView) {
        FURI_LOG_E(TAG, "Failed to allocate testing view");
        return;
    }
    view_allocate_model(testingView, ViewModelTypeLockFree, sizeof(CPUStartApp*));
    *(CPUStartApp**)view_get_model(testingView) = this;
    view_set_context(testingView, this);
    view_set_draw_callback(testingView, callbackTestingDraw);
    view_set_input_callback(testingView, callbackTestingInput);
    view_set_custom_callback(testingView, callbackTestingCustomEvent);
    view_set_exit_callback(testingView, callbackTestingExit);
    view_set_previous_callback(testingView, callbackReturnToMain);
    view_dispatcher_add_view(viewDispatcher, CPUStartViewTesting, testingView);

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
    if(mainMenuView) {
        view_dispatcher_remove_view(viewDispatcher, CPUStartViewMainMenu);
        view_free(mainMenuView);
    }
    if(appsView) {
        view_dispatcher_remove_view(viewDispatcher, CPUStartViewAppsMenu);
        view_free(appsView);
    }
    if(bootProfilesView) {
        view_dispatcher_remove_view(viewDispatcher, CPUStartViewBootProfiles);
        view_free(bootProfilesView);
    }
    if(gamesView) {
        view_dispatcher_remove_view(viewDispatcher, CPUStartViewGamesMenu);
        view_free(gamesView);
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
    if(testingView) {
        view_dispatcher_remove_view(viewDispatcher, CPUStartViewTesting);
        view_free(testingView);
    }

    if(viewDispatcher) {
        view_dispatcher_free(viewDispatcher);
    }
    if(notification) {
        hapticStop(this);
        furi_record_close(RECORD_NOTIFICATION);
        notification = nullptr;
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

uint32_t CPUStartApp::callbackReturnFromTerminal(void* context) {
    CPUStartApp* app = static_cast<CPUStartApp*>(context);
    return app->termPrevView;
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
        app->netState = 0;
        app->netIndex = 0;
        view_dispatcher_switch_to_view(app->viewDispatcher, CPUStartViewNetwork);
        break;
    case CPUStartMainTesting:
        app->testScreen = TEST_MENU;
        app->testIndex = 0;
        app->testKeyCounts[0] = 0;
        app->testKeyCounts[1] = 0;
        app->testKeyCounts[2] = 0;
        app->testKeyCounts[3] = 0;
        app->testKeyCounts[4] = 0;
        app->testKeyCounts[5] = 0;
        app->testFwdCount = 0;
        view_dispatcher_switch_to_view(app->viewDispatcher, CPUStartViewTesting);
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

// ===================== Main menu view (icons) =====================
#define MAIN_COUNT 8
#define MAIN_VISIBLE 4

static const char* const mainLabels[] = {
    "Desktop Computer",
    "Boot menu",
    "Apps",
    "Files",
    "Network",
    "Testing",
    "Settings",
    "Boot profiles",
};

const Icon* CPUStartApp::mainIcon(uint8_t index) {
    static const Icon* icons[] = {
        &I_main_desktop_12x12,
        &I_main_boot_12x12,
        &I_main_apps_12x12,
        &I_main_files_12x12,
        &I_main_network_12x12,
        &I_main_testing_12x12,
        &I_main_settings_12x12,
        &I_main_profiles_12x12,
    };
    return icons[index];
}

bool CPUStartApp::callbackMainMenuInput(InputEvent* event, void* context) {
    CPUStartApp* app = static_cast<CPUStartApp*>(context);
    if(event->type != InputTypeShort) return false;
    if(event->key == InputKeyUp) {
        if(app->mainIndex > 0) app->mainIndex--;
        return true;
    }
    if(event->key == InputKeyDown) {
        if(app->mainIndex < MAIN_COUNT - 1) app->mainIndex++;
        return true;
    }
    if(event->key == InputKeyOk) {
        callbackSubmenuMain(app, app->mainIndex);
        return true;
    }
    return false;
}

void CPUStartApp::drawMainMenu(Canvas* canvas, CPUStartApp* app) {
    drawStatusBar(canvas, app, NULL);

    // Menu items (light background)
    uint8_t scroll = 0;
    if(app->mainIndex >= MAIN_VISIBLE) scroll = app->mainIndex - MAIN_VISIBLE + 1;
    for(uint8_t i = 0; i < MAIN_VISIBLE; i++) {
        uint8_t idx = scroll + i;
        if(idx >= MAIN_COUNT) break;
        uint8_t y = 26 + i * 12;
        bool sel = (idx == app->mainIndex);
        if(sel) {
            canvas_draw_box(canvas, 0, y - 9, 128, 12);
            canvas_invert_color(canvas);
        }
        canvas_draw_icon(canvas, 4, y - 8, mainIcon(idx));
        canvas_draw_str(canvas, 20, y, mainLabels[idx]);
        if(sel) canvas_invert_color(canvas);
    }

    canvas_set_color(canvas, ColorWhite);
    if(scroll > 0) {
        canvas_draw_triangle(canvas, 124, 10, 4, 3, CanvasDirectionBottomToTop);
    }
    canvas_set_color(canvas, ColorBlack);
    if((uint8_t)(scroll + MAIN_VISIBLE) < (uint8_t)MAIN_COUNT) {
        canvas_draw_triangle(canvas, 124, 60, 4, 3, CanvasDirectionTopToBottom);
    }
}

void CPUStartApp::drawStatusBar(Canvas* canvas, CPUStartApp* app, const char* title) {
    UNUSED(app);
    uint32_t tick = furi_get_tick();

    // Dark status bar
    canvas_set_color(canvas, ColorBlack);
    canvas_draw_box(canvas, 0, 0, 128, 14);

    // WiFi icon (top-left)
    canvas_set_color(canvas, ColorWhite);
    canvas_draw_icon(canvas, 2, 1, &I_net_wifi_12x12);

    // Fake battery (top-right)
    uint32_t sec = tick / 1000;
    uint8_t battery = 100 - (sec % 90);
    char bat[8];
    snprintf(bat, sizeof(bat), "%u%%", (unsigned)battery);
    canvas_set_font(canvas, FontSecondary);
    uint8_t bw = canvas_string_width(canvas, bat);
    canvas_draw_str(canvas, 104 - 3 - bw, 9, bat);
    canvas_draw_frame(canvas, 104, 2, 14, 7);
    canvas_draw_box(canvas, 119, 4, 2, 3);
    uint8_t fill = (battery * 12) / 100;
    if(fill > 0) canvas_draw_box(canvas, 105, 3, fill, 5);

    // Centered title, truncated to fit between WiFi icon and battery
    if(title && title[0] != '\0') {
        canvas_set_font(canvas, FontSecondary);
        uint8_t maxW = 101 - 18;
        if(canvas_string_width(canvas, title) <= maxW) {
            uint8_t tw = canvas_string_width(canvas, title);
            canvas_draw_str(canvas, 60 - tw / 2, 10, title);
        } else {
            char buf[24];
            strncpy(buf, title, sizeof(buf) - 3);
            buf[sizeof(buf) - 3] = '\0';
            while(canvas_string_width(canvas, buf) > maxW - 10) {
                uint8_t len = strlen(buf);
                if(len == 0) break;
                buf[len - 1] = '\0';
            }
            char out[64];
            snprintf(out, sizeof(out), "%s..", buf);
            uint8_t ow = canvas_string_width(canvas, out);
            canvas_draw_str(canvas, 60 - ow / 2, 10, out);
        }
    }

    canvas_draw_line(canvas, 0, 14, 128, 14);
    canvas_set_color(canvas, ColorBlack);
}

void CPUStartApp::callbackMainMenuDraw(Canvas* canvas, void* context) {
    CPUStartApp* app = *(CPUStartApp**)context;
    canvas_clear(canvas);
    drawMainMenu(canvas, app);
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
        app->termPrompt = "mmc@2a31:~$";
        app->termPrevView = CPUStartViewAppsMenu;
        app->termLineCount = 0;
        for(uint8_t i = 0; i < 6; i++) {
            app->termLines[i][0] = '\0';
        }
        view_dispatcher_switch_to_view(app->viewDispatcher, CPUStartViewTerminal);
        break;
    case CPUStartAppsGames:
        view_dispatcher_switch_to_view(app->viewDispatcher, CPUStartViewGamesMenu);
        break;
    default:
        break;
    }
}

// ===================== Apps view (icons) =====================
#define APPS_COUNT 6
#define APPS_VISIBLE 4

static const char* const appsLabels[] = {
    "Internet radio",
    "Voice recorder",
    "Walkie talkie",
    "TV media box",
    "Terminal",
    "Games",
};

const Icon* CPUStartApp::appsIcon(uint8_t index) {
    static const Icon* icons[] = {
        &I_app_radio_12x12,
        &I_app_mic_12x12,
        &I_app_walkie_12x12,
        &I_app_tv_12x12,
        &I_app_terminal_12x12,
        &I_app_games_12x12,
    };
    return icons[index];
}

bool CPUStartApp::callbackAppsInput(InputEvent* event, void* context) {
    CPUStartApp* app = static_cast<CPUStartApp*>(context);
    if(event->type != InputTypeShort) return false;
    if(event->key == InputKeyUp) {
        if(app->appsIndex > 0) app->appsIndex--;
        return true;
    }
    if(event->key == InputKeyDown) {
        if(app->appsIndex < APPS_COUNT - 1) app->appsIndex++;
        return true;
    }
    if(event->key == InputKeyOk) {
        callbackSubmenuApps(app, app->appsIndex);
        return true;
    }
    return false;
}

void CPUStartApp::drawAppsMenu(Canvas* canvas, CPUStartApp* app) {
    drawStatusBar(canvas, app, "Apps");
    canvas_set_font(canvas, FontSecondary);
    uint8_t scroll = 0;
    if(app->appsIndex >= APPS_VISIBLE) scroll = app->appsIndex - APPS_VISIBLE + 1;
    for(uint8_t i = 0; i < APPS_VISIBLE; i++) {
        uint8_t idx = scroll + i;
        if(idx >= APPS_COUNT) break;
        uint8_t y = 24 + i * 12;
        bool sel = (idx == app->appsIndex);
        if(sel) {
            canvas_draw_box(canvas, 0, y - 9, 128, 12);
            canvas_invert_color(canvas);
        }
        canvas_draw_icon(canvas, 4, y - 8, appsIcon(idx));
        canvas_draw_str(canvas, 20, y, appsLabels[idx]);
        if(sel) canvas_invert_color(canvas);
    }
    if(scroll > 0) {
        canvas_draw_triangle(canvas, 124, 3, 4, 3, CanvasDirectionBottomToTop);
    }
    if((uint8_t)(scroll + APPS_VISIBLE) < (uint8_t)APPS_COUNT) {
        canvas_draw_triangle(canvas, 124, 60, 4, 3, CanvasDirectionTopToBottom);
    }
}

void CPUStartApp::callbackAppsDraw(Canvas* canvas, void* context) {
    CPUStartApp* app = *(CPUStartApp**)context;
    canvas_clear(canvas);
    drawAppsMenu(canvas, app);
}

// ===================== Games menu =====================
#define GAMES_COUNT 3
#define GAMES_VISIBLE 3

static const char* const gamesLabels[] = {
    "Snake",
    "Tetris",
    "Pong",
};

const Icon* CPUStartApp::gamesIcon(uint8_t index) {
    static const Icon* icons[] = {
        &I_game_snake_12x12,
        &I_game_tetris_12x12,
        &I_game_pong_12x12,
    };
    return icons[index];
}

bool CPUStartApp::callbackGamesInput(InputEvent* event, void* context) {
    CPUStartApp* app = static_cast<CPUStartApp*>(context);
    if(event->type != InputTypeShort) return false;
    if(event->key == InputKeyUp) {
        if(app->gamesIndex > 0) app->gamesIndex--;
        return true;
    }
    if(event->key == InputKeyDown) {
        if(app->gamesIndex < GAMES_COUNT - 1) app->gamesIndex++;
        return true;
    }
    if(event->key == InputKeyOk) {
        callbackSubmenuGames(app, app->gamesIndex);
        return true;
    }
    return false;
}

void CPUStartApp::drawGamesMenu(Canvas* canvas, CPUStartApp* app) {
    drawStatusBar(canvas, app, "Games");
    canvas_set_font(canvas, FontSecondary);
    for(uint8_t i = 0; i < GAMES_COUNT; i++) {
        uint8_t y = 26 + i * 12;
        bool sel = (i == app->gamesIndex);
        if(sel) {
            canvas_draw_box(canvas, 0, y - 9, 128, 12);
            canvas_invert_color(canvas);
        }
        canvas_draw_icon(canvas, 4, y - 8, gamesIcon(i));
        canvas_draw_str(canvas, 20, y, gamesLabels[i]);
        if(sel) canvas_invert_color(canvas);
    }
    canvas_set_font(canvas, FontKeyboard);
    canvas_draw_str(canvas, 2, 63, "[OK] play  [UP][DN] select");
}

void CPUStartApp::callbackGamesDraw(Canvas* canvas, void* context) {
    CPUStartApp* app = *(CPUStartApp**)context;
    canvas_clear(canvas);
    drawGamesMenu(canvas, app);
}

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

#define BOOT_PROFILE_COUNT 5
#define BOOT_PROFILE_VISIBLE 4

static const char* const bootProfileLabels[] = {
    "Router",
    "TV Media Box",
    "Desktop",
    "Minimal System",
    "Boot from sd card",
};

const Icon* CPUStartApp::bootProfileIcon(uint8_t index) {
    static const Icon* icons[] = {
        &I_boot_router_12x12,
        &I_boot_tv_12x12,
        &I_main_desktop_12x12,
        &I_boot_minimal_12x12,
        &I_boot_sd_12x12,
    };
    return icons[index];
}

bool CPUStartApp::callbackBootProfilesInput(InputEvent* event, void* context) {
    CPUStartApp* app = static_cast<CPUStartApp*>(context);
    if(event->type != InputTypeShort) return false;
    if(event->key == InputKeyUp) {
        if(app->bootIndex > 0) app->bootIndex--;
        return true;
    }
    if(event->key == InputKeyDown) {
        if(app->bootIndex < BOOT_PROFILE_COUNT - 1) app->bootIndex++;
        return true;
    }
    if(event->key == InputKeyOk) {
        callbackSubmenuBootProfiles(app, app->bootIndex);
        return true;
    }
    return false;
}

void CPUStartApp::drawBootProfilesMenu(Canvas* canvas, CPUStartApp* app) {
    drawStatusBar(canvas, app, "Boot profiles");
    canvas_set_font(canvas, FontSecondary);
    uint8_t scroll = 0;
    if(app->bootIndex >= BOOT_PROFILE_VISIBLE) {
        scroll = app->bootIndex - BOOT_PROFILE_VISIBLE + 1;
    }
    for(uint8_t i = 0; i < BOOT_PROFILE_VISIBLE; i++) {
        uint8_t idx = scroll + i;
        if(idx >= BOOT_PROFILE_COUNT) break;
        uint8_t y = 24 + i * 12;
        bool sel = (idx == app->bootIndex);
        if(sel) {
            canvas_draw_box(canvas, 0, y - 9, 128, 12);
            canvas_invert_color(canvas);
        }
        canvas_draw_icon(canvas, 4, y - 8, bootProfileIcon(idx));
        canvas_draw_str(canvas, 20, y, bootProfileLabels[idx]);
        if(sel) canvas_invert_color(canvas);
    }
    if(scroll > 0) {
        canvas_set_color(canvas, ColorWhite);
        canvas_draw_triangle(canvas, 124, 10, 4, 3, CanvasDirectionBottomToTop);
        canvas_set_color(canvas, ColorBlack);
    }
    if((uint8_t)(scroll + BOOT_PROFILE_VISIBLE) < (uint8_t)BOOT_PROFILE_COUNT) {
        canvas_draw_triangle(canvas, 124, 60, 4, 3, CanvasDirectionTopToBottom);
    }
}

void CPUStartApp::callbackBootProfilesDraw(Canvas* canvas, void* context) {
    CPUStartApp* app = *(CPUStartApp**)context;
    canvas_clear(canvas);
    drawBootProfilesMenu(canvas, app);
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
            } else if(event->key == InputKeyLeft) {
                if(app->radioVolume > 0) app->radioVolume--;
            } else if(event->key == InputKeyRight) {
                if(app->radioVolume < 10) app->radioVolume++;
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
        if(event->key == InputKeyUp && event->type == InputTypeShort) {
            app->fakeAppSelected =
                (app->fakeAppSelected + WALKIE_CHANNELS_COUNT - 1) % WALKIE_CHANNELS_COUNT;
        } else if(event->key == InputKeyDown && event->type == InputTypeShort) {
            app->fakeAppSelected = (app->fakeAppSelected + 1) % WALKIE_CHANNELS_COUNT;
        } else if(
            event->key == InputKeyLeft &&
            (event->type == InputTypeShort || event->type == InputTypeRepeat)) {
            if(app->walkieVolume > 0) app->walkieVolume--;
        } else if(
            event->key == InputKeyRight &&
            (event->type == InputTypeShort || event->type == InputTypeRepeat)) {
            if(app->walkieVolume < 10) app->walkieVolume++;
        } else if(event->key == InputKeyOk && event->type == InputTypePress) {
            app->fakeAppState = 1;
            app->fakeAppStartTick = furi_get_tick();
        } else if(event->key == InputKeyOk && event->type == InputTypeRelease) {
            app->fakeAppState = 0;
        }
        break;
    case CPUStartFakeTV:
        if(event->type == InputTypeShort || event->type == InputTypeRepeat) {
            if(event->key == InputKeyUp) {
                app->fakeAppSelected =
                    (app->fakeAppSelected + KODI_MENU_COUNT - 1) % KODI_MENU_COUNT;
            } else if(event->key == InputKeyDown) {
                app->fakeAppSelected = (app->fakeAppSelected + 1) % KODI_MENU_COUNT;
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
    uint32_t tick = furi_get_tick();
    drawStatusBar(canvas, app, "Internet Radio");

    // Station name (centered)
    canvas_set_font(canvas, FontSecondary);
    const char* name = radioStations[app->fakeAppSelected];
    uint8_t nw = canvas_string_width(canvas, name);
    canvas_draw_str(canvas, 64 - nw / 2, 24, name);

    // Frequency (big digits)
    canvas_set_font(canvas, FontBigNumbers);
    const char* freq = radioFreq[app->fakeAppSelected];
    uint8_t fw = canvas_string_width(canvas, freq);
    canvas_draw_str(canvas, 64 - fw / 2, 44, freq);

    // Volume bar
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 4, 57, "VOL");
    canvas_draw_frame(canvas, 30, 51, 60, 5);
    uint8_t fill = (app->radioVolume * 58) / 10;
    if(fill > 0) canvas_draw_box(canvas, 31, 52, fill, 3);
    char vol[4];
    snprintf(vol, sizeof(vol), "%u", (unsigned)app->radioVolume);
    canvas_draw_str(canvas, 96, 57, vol);

    // Equalizer bars (right side) while playing
    if(app->fakeAppState) {
        for(uint8_t b = 0; b < 4; b++) {
            uint8_t h = 3 + ((tick / 80 + b * 3) % 6);
            canvas_draw_line(canvas, 108 + b * 5, 55, 108 + b * 5, 55 - h);
        }
    }

    // Bottom status line
    canvas_set_font(canvas, FontKeyboard);
    if(app->fakeAppState) {
        uint32_t sec = (tick - app->fakeAppStartTick) / 1000;
        char now[20];
        snprintf(
            now,
            sizeof(now),
            "NOW %02lu:%02lu",
            (unsigned long)(sec / 60),
            (unsigned long)(sec % 60));
        canvas_draw_str(canvas, 2, 63, now);
    } else {
        canvas_draw_str(canvas, 2, 63, "[OK] play  [<][>] vol");
    }
}

void CPUStartApp::drawRecorder(Canvas* canvas, CPUStartApp* app) {
    drawStatusBar(canvas, app, "Voice Recorder");

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
    uint32_t tick = furi_get_tick();
    bool tx = (app->fakeAppState == 1);

    drawStatusBar(canvas, app, "Walkie Talkie");

    // Channel + frequency
    canvas_set_font(canvas, FontSecondary);
    char ch[16];
    snprintf(ch, sizeof(ch), "CH %02lu", (unsigned long)(app->fakeAppSelected + 1));
    canvas_draw_str(canvas, 4, 24, ch);
    const char* freq = walkieChannels[app->fakeAppSelected];
    uint8_t fw = canvas_string_width(canvas, freq);
    canvas_draw_str(canvas, 126 - fw, 24, freq);

    // Signal bars (animated)
    canvas_draw_str(canvas, 4, 38, "SIG");
    uint8_t level = 1 + ((tick / 200) % 5);
    for(uint8_t b = 0; b < 5; b++) {
        uint8_t h = (b < level) ? (uint8_t)(3 + b * 2) : 1;
        canvas_draw_line(canvas, 30 + b * 9, 36, 30 + b * 9, 36 - h);
    }

    // Battery
    canvas_draw_str(canvas, 90, 38, "BAT");
    canvas_draw_frame(canvas, 112, 29, 14, 7);
    canvas_draw_box(canvas, 126, 31, 2, 3);
    canvas_draw_box(canvas, 113, 30, 8, 5);

    // TX / RX status
    canvas_set_font(canvas, FontSecondary);
    if(tx) {
        canvas_draw_box(canvas, 4, 42, 120, 8);
        canvas_invert_color(canvas);
        canvas_draw_str(canvas, 6, 49, "TRANSMITTING");
        canvas_invert_color(canvas);
    } else {
        canvas_draw_str(canvas, 6, 49, "Receiving");
    }

    // PTT button
    canvas_draw_frame(canvas, 8, 53, 112, 9);
    if(tx) {
        canvas_draw_box(canvas, 9, 54, 110, 7);
        canvas_invert_color(canvas);
        canvas_draw_str(canvas, 12, 61, "PTT  TX ON");
        canvas_invert_color(canvas);
    } else {
        canvas_draw_str(canvas, 12, 61, "PTT  HOLD OK");
    }
}

void CPUStartApp::drawTV(Canvas* canvas, CPUStartApp* app) {
    uint32_t tick = furi_get_tick();

    // Dark media-center background
    canvas_set_color(canvas, ColorBlack);
    canvas_draw_box(canvas, 0, 0, 128, 64);
    canvas_set_color(canvas, ColorWhite);

    // Faded backdrop art on the right (moon over mountains)
    canvas_draw_disc(canvas, 110, 18, 5);
    canvas_draw_line(canvas, 86, 48, 116, 26);
    canvas_draw_line(canvas, 116, 26, 126, 48);
    canvas_draw_line(canvas, 82, 48, 128, 48);

    drawStatusBar(canvas, app, "KODI");
    canvas_set_color(canvas, ColorWhite);

    // Vertical home menu
    canvas_set_font(canvas, FontSecondary);
    static const char* kodiItems[] = {
        "TV Shows", "Movies", "Music", "Pictures", "Add-ons", "Settings"};
    for(uint8_t i = 0; i < KODI_MENU_COUNT; i++) {
        uint8_t y = 20 + i * 6;
        if(i == app->fakeAppSelected) {
            canvas_draw_box(canvas, 0, y - 5, 76, 6);
            canvas_set_color(canvas, ColorBlack);
            canvas_draw_str(canvas, 4, y, kodiItems[i]);
            canvas_set_color(canvas, ColorWhite);
        } else {
            canvas_draw_str(canvas, 4, y, kodiItems[i]);
        }
    }

    // Bottom now-playing bar
    canvas_draw_line(canvas, 0, 57, 128, 57);
    canvas_set_font(canvas, FontKeyboard);
    const char* chan =
        tvChannels[(app->fakeAppState ? (tick / 4000) : 0) % TV_CHANNELS_COUNT];
    char np[40];
    snprintf(np, sizeof(np), "Now playing: %s", chan);
    canvas_draw_str(canvas, 2, 63, np);
}

void CPUStartApp::drawRouter(Canvas* canvas, CPUStartApp* app) {
    uint32_t tick = furi_get_tick();

    drawStatusBar(canvas, app, "Router Admin");

    // Status LED (blinks when online)
    if(app->fakeAppState == 0) {
        if((tick / 300) % 2 == 0) {
            canvas_draw_disc(canvas, 118, 22, 3);
        } else {
            canvas_draw_frame(canvas, 115, 19, 6, 6);
        }
    } else {
        canvas_draw_frame(canvas, 115, 19, 6, 6);
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

// ===================== Testing =====================
static const char* const testLabels[] = {
    "Screen",
    "Screen (photo)",
    "UI demos",
    "Input (buttons)",
    "Touchpad",
    "Touchpad ABS",
    "Screen keyboard",
    "Network LEDs",
    "Sound",
    "UI input forwarding",
    "Haptic (vibrations)",
    "UI PNG viewer",
    "GPIO",
    "Switch to flipctl",
};
#define TEST_COUNT (sizeof(testLabels) / sizeof(testLabels[0]))
#define TEST_VISIBLE 4

static const char* testKeyName(InputKey key) {
    switch(key) {
    case InputKeyUp:
        return "UP";
    case InputKeyDown:
        return "DN";
    case InputKeyLeft:
        return "LT";
    case InputKeyRight:
        return "RT";
    case InputKeyOk:
        return "OK";
    case InputKeyBack:
        return "BK";
    default:
        return "??";
    }
}

const Icon* CPUStartApp::testIcon(uint8_t index) {
    static const Icon* icons[] = {
        &I_test_screen_12x12,
        &I_test_photo_12x12,
        &I_test_ui_12x12,
        &I_test_input_12x12,
        &I_test_touchpad_12x12,
        &I_test_touchabs_12x12,
        &I_test_kbd_12x12,
        &I_test_netled_12x12,
        &I_test_sound_12x12,
        &I_test_forward_12x12,
        &I_test_haptic_12x12,
        &I_test_png_12x12,
        &I_test_gpio_12x12,
        &I_test_flipctl_12x12,
    };
    return icons[index];
}

void CPUStartApp::testSoundStart(CPUStartApp* app) {
    if(!furi_hal_speaker_is_mine()) {
        if(!furi_hal_speaker_acquire(1000)) return;
    }
    furi_hal_speaker_start(app->testFreq, 1.0f);
}

void CPUStartApp::testSoundStop(CPUStartApp* app) {
    UNUSED(app);
    if(furi_hal_speaker_is_mine()) {
        furi_hal_speaker_stop();
        furi_hal_speaker_release();
    }
}

void CPUStartApp::hapticStart(CPUStartApp* app) {
    if(!furi_hal_speaker_is_mine()) {
        if(!furi_hal_speaker_acquire(1000)) return;
    }
    furi_hal_speaker_start(140.0f, 1.0f);
    if(app->notification && !app->vibroActive) {
        notification_message(app->notification, &sequence_set_vibro_on);
        app->vibroActive = true;
    }
}

void CPUStartApp::hapticStop(CPUStartApp* app) {
    if(furi_hal_speaker_is_mine()) {
        furi_hal_speaker_stop();
        furi_hal_speaker_release();
    }
    if(app->notification && app->vibroActive) {
        notification_message(app->notification, &sequence_reset_vibro);
        app->vibroActive = false;
    }
}

void CPUStartApp::hapticUpdate(CPUStartApp* app) {
    uint32_t phase = furi_get_tick() % 500;
    if(phase < 350) {
        if(!furi_hal_speaker_is_mine()) {
            if(!furi_hal_speaker_acquire(10)) return;
        }
        float freq = 120.0f + (float)((furi_get_tick() / 40) % 60);
        furi_hal_speaker_start(freq, 1.0f);
        if(app->notification && !app->vibroActive) {
            notification_message(app->notification, &sequence_set_vibro_on);
            app->vibroActive = true;
        }
    } else {
        if(furi_hal_speaker_is_mine()) {
            furi_hal_speaker_stop();
            furi_hal_speaker_release();
        }
        if(app->notification && app->vibroActive) {
            notification_message(app->notification, &sequence_reset_vibro);
            app->vibroActive = false;
        }
    }
}

void CPUStartApp::callbackTestingExit(void* context) {
    CPUStartApp* app = static_cast<CPUStartApp*>(context);
    testSoundStop(app);
    hapticStop(app);
}

bool CPUStartApp::callbackTestingCustomEvent(uint32_t event, void* context) {
    CPUStartApp* app = static_cast<CPUStartApp*>(context);
    if(event == CPUStartCustomEventTick) {
        if(app->testScreen == CPUStartTestHaptic) {
            if(app->testVibrate) {
                hapticUpdate(app);
            } else {
                hapticStop(app);
            }
        }
        view_commit_model(app->testingView, true);
    }
    return true;
}

bool CPUStartApp::callbackTestingInput(InputEvent* event, void* context) {
    CPUStartApp* app = static_cast<CPUStartApp*>(context);
    if(event->type != InputTypeShort) return false;

    if(app->testScreen == TEST_MENU) {
        if(event->key == InputKeyUp) {
            if(app->testIndex > 0) app->testIndex--;
            return true;
        }
        if(event->key == InputKeyDown) {
            if(app->testIndex < TEST_COUNT - 1) app->testIndex++;
            return true;
        }
        if(event->key == InputKeyOk) {
            if(app->testIndex == CPUStartTestFlipCtl) {
                app->termPrompt = "flipctl #";
                app->termPrevView = CPUStartViewTesting;
                app->termLineCount = 0;
                view_dispatcher_switch_to_view(app->viewDispatcher, CPUStartViewTerminal);
                return true;
            }
            app->testScreen = app->testIndex;
            if(app->testScreen == CPUStartTestInput) {
                for(uint8_t i = 0; i < 6; i++) app->testKeyCounts[i] = 0;
            }
            if(app->testScreen == CPUStartTestForward) {
                app->testFwdCount = 0;
                app->testFwdIdx = 0;
            }
            if(app->testScreen == CPUStartTestKbd) {
                app->testKbdX = 0;
                app->testKbdY = 0;
                app->testKbdLen = 0;
                app->testKbdBuf[0] = '\0';
            }
            if(app->testScreen == CPUStartTestSound) {
                app->testSoundOn = false;
                app->testFreq = 440.0f;
            }
            if(app->testScreen == CPUStartTestHaptic) {
                app->testVibrate = false;
            }
            return true;
        }
        return false;
    }

    // Active test screen: Back returns to the menu
    if(event->key == InputKeyBack) {
        if(app->testScreen == CPUStartTestInput) app->testKeyCounts[5]++;
        app->testScreen = TEST_MENU;
        testSoundStop(app);
        hapticStop(app);
        app->testVibrate = false;
        return true;
    }

    // Forwarding test logs every event
    if(app->testScreen == CPUStartTestForward) {
        char line[24];
        snprintf(line, sizeof(line), "FWD %s", testKeyName(event->key));
        memcpy(app->testFwd[app->testFwdIdx], line, sizeof(app->testFwd[0]));
        app->testFwdIdx = (app->testFwdIdx + 1) % 4;
        if(app->testFwdCount < 4) app->testFwdCount++;
    }

    switch(app->testScreen) {
    case CPUStartTestPhoto:
        if(event->key == InputKeyOk) app->testPhotoBlack = !app->testPhotoBlack;
        break;
    case CPUStartTestInput: {
        uint8_t idx = 0xFF;
        switch(event->key) {
        case InputKeyUp:
            idx = 0;
            break;
        case InputKeyDown:
            idx = 1;
            break;
        case InputKeyLeft:
            idx = 2;
            break;
        case InputKeyRight:
            idx = 3;
            break;
        case InputKeyOk:
            idx = 4;
            break;
        default:
            break;
        }
        if(idx != 0xFF) app->testKeyCounts[idx]++;
        break;
    }
    case CPUStartTestKbd:
        if(event->key == InputKeyUp && app->testKbdY > 0) app->testKbdY--;
        else if(event->key == InputKeyDown && app->testKbdY < 2) app->testKbdY++;
        else if(event->key == InputKeyLeft && app->testKbdX > 0) app->testKbdX--;
        else if(event->key == InputKeyRight && app->testKbdX < 9) app->testKbdX++;
        else if(event->key == InputKeyOk && app->testKbdLen < 19) {
            app->testKbdBuf[app->testKbdLen++] = 'a' + app->testKbdY * 10 + app->testKbdX;
            app->testKbdBuf[app->testKbdLen] = '\0';
        }
        break;
    case CPUStartTestSound:
        if(event->key == InputKeyUp) app->testFreq += 50.0f;
        else if(event->key == InputKeyDown) {
            app->testFreq -= 50.0f;
            if(app->testFreq < 100.0f) app->testFreq = 100.0f;
        } else if(event->key == InputKeyOk) {
            app->testSoundOn = !app->testSoundOn;
            if(app->testSoundOn) {
                testSoundStart(app);
            } else {
                testSoundStop(app);
            }
        }
        if(app->testSoundOn) testSoundStart(app);
        break;
    case CPUStartTestHaptic:
        if(event->key == InputKeyOk) app->testVibrate = !app->testVibrate;
        break;
    case CPUStartTestGPIO:
        if(event->key == InputKeyUp && app->testGpioSel > 0) app->testGpioSel--;
        else if(event->key == InputKeyDown && app->testGpioSel < 5) app->testGpioSel++;
        else if(event->key == InputKeyOk) {
            app->testGpioStates[app->testGpioSel] = !app->testGpioStates[app->testGpioSel];
        }
        break;
    default:
        break;
    }
    return true;
}

void CPUStartApp::drawTestingMenu(Canvas* canvas, CPUStartApp* app) {
    drawStatusBar(canvas, app, "Testing");
    canvas_set_font(canvas, FontSecondary);

    uint8_t scroll = 0;
    if(app->testIndex >= TEST_VISIBLE) scroll = app->testIndex - TEST_VISIBLE + 1;
    for(uint8_t i = 0; i < TEST_VISIBLE; i++) {
        uint8_t idx = scroll + i;
        if(idx >= TEST_COUNT) break;
        uint8_t y = 24 + i * 12;
        bool sel = (idx == app->testIndex);
        if(sel) {
            canvas_draw_box(canvas, 0, y - 9, 128, 12);
            canvas_invert_color(canvas);
        }
        canvas_draw_icon(canvas, 4, y - 8, testIcon(idx));
        canvas_draw_str(canvas, 20, y, testLabels[idx]);
        if(sel) canvas_invert_color(canvas);
    }
    if(scroll > 0) {
        canvas_set_color(canvas, ColorWhite);
        canvas_draw_triangle(canvas, 124, 10, 4, 3, CanvasDirectionBottomToTop);
        canvas_set_color(canvas, ColorBlack);
    }
    if((uint8_t)(scroll + TEST_VISIBLE) < (uint8_t)TEST_COUNT) {
        canvas_draw_triangle(canvas, 124, 60, 4, 3, CanvasDirectionTopToBottom);
    }
}

void CPUStartApp::drawTestScreen(Canvas* canvas, CPUStartApp* app) {
    UNUSED(app);
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 2, 8, "Screen test");
    for(uint8_t x = 0; x < 8; x++) {
        for(uint8_t y = 0; y < 4; y++) {
            if((x + y) % 2 == 0) {
                canvas_draw_box(canvas, 2 + x * 15, 14 + y * 10, 15, 10);
            }
        }
    }
    canvas_draw_frame(canvas, 0, 12, 128, 44);
    canvas_draw_disc(canvas, 64, 34, 4);
    canvas_draw_str(canvas, 2, 63, "Press BACK to return");
}

void CPUStartApp::drawTestPhoto(Canvas* canvas, CPUStartApp* app) {
    if(app->testPhotoBlack) {
        canvas_draw_box(canvas, 0, 0, 128, 64);
        canvas_set_color(canvas, ColorWhite);
    }
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(
        canvas, 4, 58, app->testPhotoBlack ? "BLACK  [OK] white" : "WHITE  [OK] black");
}

void CPUStartApp::drawTestUIDemos(Canvas* canvas, CPUStartApp* app) {
    UNUSED(app);
    uint32_t tick = furi_get_tick();
    drawStatusBar(canvas, app, "UI Demos");
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_frame(canvas, 4, 16, 40, 14);
    canvas_draw_str(canvas, 12, 24, "Button");
    canvas_draw_str(canvas, 60, 20, "Slider");
    canvas_draw_frame(canvas, 60, 24, 60, 6);
    uint8_t pos = 2 + ((tick / 80) % 57);
    canvas_draw_box(canvas, 60, 24, pos, 6);
    canvas_draw_frame(canvas, 4, 34, 8, 8);
    canvas_draw_line(canvas, 5, 38, 8, 41);
    canvas_draw_line(canvas, 8, 41, 11, 35);
    canvas_draw_str(canvas, 16, 42, "Checkbox");
    canvas_draw_disc(canvas, 7, 50, 3);
    canvas_draw_str(canvas, 16, 53, "Radio");
    canvas_draw_disc(canvas, 50, 50, 3);
    canvas_draw_str(canvas, 59, 53, "Radio 2");
}

void CPUStartApp::drawTestInput(Canvas* canvas, CPUStartApp* app) {
    drawStatusBar(canvas, app, "Input test");
    canvas_set_font(canvas, FontSecondary);
    static const char* keyNames[6] = {"UP", "DN", "LT", "RT", "OK", "BK"};
    for(uint8_t i = 0; i < 6; i++) {
        uint8_t x = 4 + (i % 3) * 42;
        uint8_t y = 24 + (i / 3) * 20;
        char buf[16];
        snprintf(buf, sizeof(buf), "%s %u", keyNames[i], app->testKeyCounts[i]);
        if(app->testKeyCounts[i] > 0) {
            canvas_draw_box(canvas, x, y - 8, 40, 16);
            canvas_invert_color(canvas);
            canvas_draw_str(canvas, x + 3, y + 4, buf);
            canvas_invert_color(canvas);
        } else {
            canvas_draw_frame(canvas, x, y - 8, 40, 16);
            canvas_draw_str(canvas, x + 3, y + 4, buf);
        }
    }
    canvas_set_font(canvas, FontKeyboard);
    canvas_draw_str(canvas, 2, 62, "[BACK] return to menu");
}

void CPUStartApp::drawTestTouchpad(Canvas* canvas, CPUStartApp* app) {
    UNUSED(app);
    drawStatusBar(canvas, app, "Touchpad");
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_frame(canvas, 20, 18, 88, 30);
    canvas_draw_str(canvas, 16, 62, "No touchpad present");
}

void CPUStartApp::drawTestTouchpadABS(Canvas* canvas, CPUStartApp* app) {
    UNUSED(app);
    drawStatusBar(canvas, app, "Touchpad ABS");
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_frame(canvas, 20, 18, 88, 30);
    canvas_draw_line(canvas, 64, 18, 64, 48);
    canvas_draw_line(canvas, 20, 33, 108, 33);
    canvas_draw_str(canvas, 16, 62, "Absolute mode: no device");
}

void CPUStartApp::drawTestKbd(Canvas* canvas, CPUStartApp* app) {
    drawStatusBar(canvas, app, "Screen keyboard");
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 2, 20, app->testKbdBuf);
    for(uint8_t y = 0; y < 3; y++) {
        for(uint8_t x = 0; x < 10; x++) {
            uint8_t kx = 2 + x * 12;
            uint8_t ky = 26 + y * 12;
            char c = 'a' + y * 10 + x;
            char cs[2] = {c, 0};
            if(app->testKbdX == x && app->testKbdY == y) {
                canvas_draw_box(canvas, kx, ky, 11, 10);
                canvas_invert_color(canvas);
                canvas_draw_str(canvas, kx + 3, ky + 8, cs);
                canvas_invert_color(canvas);
            } else {
                canvas_draw_frame(canvas, kx, ky, 11, 10);
                canvas_draw_str(canvas, kx + 3, ky + 8, cs);
            }
        }
    }
    canvas_draw_str(canvas, 2, 63, "[OK] type  [BACK] exit");
}

void CPUStartApp::drawTestNetLEDs(Canvas* canvas, CPUStartApp* app) {
    UNUSED(app);
    uint32_t tick = furi_get_tick();
    drawStatusBar(canvas, app, "Network LEDs");
    canvas_set_font(canvas, FontSecondary);
    static const char* ledNames[5] = {"LINK", "ACT", "WAN", "LAN", "PWR"};
    for(uint8_t i = 0; i < 5; i++) {
        uint8_t y = 22 + i * 9;
        bool on = (tick / (150 + i * 120)) % 2 == 0;
        canvas_draw_str(canvas, 10, y, ledNames[i]);
        if(on) {
            canvas_draw_box(canvas, 52, y - 6, 60, 6);
            canvas_set_color(canvas, ColorWhite);
            canvas_draw_str(canvas, 70, y, "ON");
            canvas_set_color(canvas, ColorBlack);
        } else {
            canvas_draw_frame(canvas, 52, y - 6, 60, 6);
            canvas_draw_str(canvas, 68, y, "off");
        }
    }
}

void CPUStartApp::drawTestSound(Canvas* canvas, CPUStartApp* app) {
    uint32_t tick = furi_get_tick();
    drawStatusBar(canvas, app, "Sound test");
    canvas_set_font(canvas, FontSecondary);
    char buf[32];
    snprintf(buf, sizeof(buf), "Freq: %.0f Hz", (double)app->testFreq);
    canvas_draw_str(canvas, 4, 24, buf);
    canvas_draw_str(canvas, 4, 36, app->testSoundOn ? "Playing" : "Stopped");
    if(app->testSoundOn) {
        for(uint8_t b = 0; b < 6; b++) {
            uint8_t h = 3 + ((tick / 60 + b * 4) % 6);
            canvas_draw_line(canvas, 60 + b * 10, 40, 60 + b * 10, 40 - h);
        }
    }
    canvas_draw_str(canvas, 4, 62, "[OK] play/stop  [UP][DN] freq");
}

void CPUStartApp::drawTestForward(Canvas* canvas, CPUStartApp* app) {
    drawStatusBar(canvas, app, "Input forwarding");
    canvas_set_font(canvas, FontSecondary);
    for(uint8_t i = 0; i < 4; i++) {
        if(i < app->testFwdCount) {
            uint8_t idx = (app->testFwdIdx + i) % 4;
            canvas_draw_str(canvas, 4, 22 + i * 9, app->testFwd[idx]);
        }
    }
    canvas_draw_str(canvas, 2, 63, "Events shown as forwarded");
}

void CPUStartApp::drawTestHaptic(Canvas* canvas, CPUStartApp* app) {
    uint32_t tick = furi_get_tick();
    drawStatusBar(canvas, app, "Haptic test");
    canvas_set_font(canvas, FontSecondary);
    if(app->testVibrate) {
        uint8_t amp = 2 + ((tick / 80) % 4);
        for(uint8_t b = 0; b < 12; b++) {
            uint8_t h = (b % 2 == 0) ? amp : (uint8_t)(amp / 2);
            canvas_draw_line(canvas, 10 + b * 9, 36, 10 + b * 9, 36 - h);
        }
        canvas_draw_str(canvas, 2, 54, "VIBRATING...");
    } else {
        canvas_draw_str(canvas, 2, 36, "Motor idle");
        canvas_draw_str(canvas, 2, 54, "[OK] vibrate");
    }
    canvas_set_font(canvas, FontKeyboard);
    canvas_draw_str(canvas, 2, 63, "[OK] toggle  [BACK] menu");
}

void CPUStartApp::drawTestPNG(Canvas* canvas, CPUStartApp* app) {
    UNUSED(app);
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_frame(canvas, 4, 4, 120, 56);
    canvas_draw_line(canvas, 4, 12, 124, 12);
    canvas_draw_str(canvas, 8, 10, "photo.png");
    canvas_draw_str(canvas, 100, 10, "PNG");
    canvas_draw_disc(canvas, 100, 22, 5);
    canvas_draw_line(canvas, 20, 48, 44, 26);
    canvas_draw_line(canvas, 44, 26, 66, 48);
    canvas_draw_line(canvas, 30, 48, 60, 30);
    canvas_draw_line(canvas, 60, 30, 88, 48);
    for(uint8_t i = 0; i < 8; i++) {
        if(i % 2 == 0) canvas_draw_box(canvas, 8 + i * 14, 48, 14, 9);
    }
}

void CPUStartApp::drawTestGPIO(Canvas* canvas, CPUStartApp* app) {
    drawStatusBar(canvas, app, "GPIO test");
    canvas_set_font(canvas, FontSecondary);
    static const char* pins[6] = {"C0", "C1", "C3", "C4", "C5", "C6"};
    for(uint8_t i = 0; i < 6; i++) {
        uint8_t x = 4 + (i % 3) * 42;
        uint8_t y = 24 + (i / 3) * 22;
        char buf[24];
        snprintf(buf, sizeof(buf), "%s  %s", pins[i], app->testGpioStates[i] ? "HIGH" : "LOW");
        if(i == app->testGpioSel) {
            canvas_draw_box(canvas, x, y - 8, 40, 18);
            canvas_invert_color(canvas);
            canvas_draw_str(canvas, x + 4, y, buf);
            canvas_invert_color(canvas);
        } else {
            canvas_draw_frame(canvas, x, y - 8, 40, 18);
            canvas_draw_str(canvas, x + 4, y, buf);
        }
    }
    canvas_set_font(canvas, FontKeyboard);
    canvas_draw_str(canvas, 2, 63, "[OK] toggle  [UP][DN] select");
}

void CPUStartApp::callbackTestingDraw(Canvas* canvas, void* context) {
    CPUStartApp* app = *(CPUStartApp**)context;
    canvas_clear(canvas);
    if(app->testScreen == TEST_MENU) {
        drawTestingMenu(canvas, app);
        return;
    }
    switch(app->testScreen) {
    case CPUStartTestScreen:
        drawTestScreen(canvas, app);
        break;
    case CPUStartTestPhoto:
        drawTestPhoto(canvas, app);
        break;
    case CPUStartTestUIDemos:
        drawTestUIDemos(canvas, app);
        break;
    case CPUStartTestInput:
        drawTestInput(canvas, app);
        break;
    case CPUStartTestTouchpad:
        drawTestTouchpad(canvas, app);
        break;
    case CPUStartTestTouchpadABS:
        drawTestTouchpadABS(canvas, app);
        break;
    case CPUStartTestKbd:
        drawTestKbd(canvas, app);
        break;
    case CPUStartTestNetLEDs:
        drawTestNetLEDs(canvas, app);
        break;
    case CPUStartTestSound:
        drawTestSound(canvas, app);
        break;
    case CPUStartTestForward:
        drawTestForward(canvas, app);
        break;
    case CPUStartTestHaptic:
        drawTestHaptic(canvas, app);
        break;
    case CPUStartTestPNG:
        drawTestPNG(canvas, app);
        break;
    case CPUStartTestGPIO:
        drawTestGPIO(canvas, app);
        break;
    default:
        break;
    }
    if(app->testScreen != CPUStartTestPhoto) {
        canvas_draw_icon(canvas, 114, 1, testIcon(app->testScreen));
    }
}

// ===================== Terminal =====================
static bool cmdIs(const char* input, const char* name) {
    size_t len = strlen(name);
    return strncmp(input, name, len) == 0 && (input[len] == '\0' || input[len] == ' ');
}

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
    snprintf(line, sizeof(line), "%s %s", app->termPrompt, app->termInput);

    char output[96];
    if(app->termInput[0] == '\0') {
        output[0] = '\0';
    } else if(cmdIs(app->termInput, "help")) {
        snprintf(output, sizeof(output), "help ls cat echo uname clear exit");
    } else if(cmdIs(app->termInput, "ls")) {
        snprintf(output, sizeof(output), "boot  dev  etc  home  proc  root");
    } else if(cmdIs(app->termInput, "cat")) {
        if(strstr(app->termInput, "/etc/os-release") != NULL) {
            snprintf(output, sizeof(output), "FlipperOS RISC-V @ 64MHz");
        } else {
            snprintf(output, sizeof(output), "cat: No such file");
        }
    } else if(cmdIs(app->termInput, "uname")) {
        snprintf(output, sizeof(output), "Linux flipper 6.1.0-flipper #1");
    } else if(cmdIs(app->termInput, "echo")) {
        const char* rest = (app->termInput[4] == ' ') ? app->termInput + 5 : "";
        snprintf(output, sizeof(output), "%s", rest);
    } else if(cmdIs(app->termInput, "clear")) {
        app->termLineCount = 0;
        for(uint8_t i = 0; i < 6; i++) {
            app->termLines[i][0] = '\0';
        }
        view_dispatcher_switch_to_view(app->viewDispatcher, CPUStartViewTerminal);
        return;
    } else if(cmdIs(app->termInput, "exit")) {
        view_dispatcher_switch_to_view(app->viewDispatcher, app->termPrevView);
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
        "%.20s",
        line);
    snprintf(
        app->termLines[app->termLineCount - 1],
        sizeof(app->termLines[app->termLineCount - 1]),
        "%.20s",
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
    canvas_set_font(canvas, FontKeyboard);
    if((tick / 400) % 2 == 0) {
        canvas_draw_str(canvas, 2, 58, app->termPrompt);
    }
    if((tick / 400) % 2 == 1) {
        canvas_draw_str(canvas, 2, 58, app->termPrompt);
        uint8_t pw = canvas_string_width(canvas, app->termPrompt);
        canvas_draw_box(canvas, 2 + pw + 1, 54, 4, 5);
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
    drawStatusBar(canvas, app, "Files");

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

    // Back returns to the menu from any sub-screen
    if(app->netState != 0 && event->key == InputKeyBack) {
        app->netState = 0;
        return true;
    }

    switch(app->netState) {
    case 0: // menu
        if(event->key == InputKeyUp) {
            if(app->netIndex > 0) app->netIndex--;
        } else if(event->key == InputKeyDown) {
            if(app->netIndex < 4) app->netIndex++;
        } else if(event->key == InputKeyOk) {
            switch(app->netIndex) {
            case 0: // airplane mode
                app->netState = 5;
                break;
            case 1: // routing info
                app->netState = 6;
                break;
            case 2: // 5G modem
                app->netState = 7;
                break;
            case 3: // wifi
                if(!app->netAirplane) {
                    app->netIndex = 0;
                    app->netState = 1;
                    app->netStartTick = furi_get_tick();
                }
                break;
            case 4: // ethernet
                app->netState = 8;
                break;
            default:
                break;
            }
        }
        return true;
    case 2: // wifi list
        if(event->key == InputKeyUp) {
            if(app->netIndex > 0) app->netIndex--;
        } else if(event->key == InputKeyDown) {
            if(app->netIndex < NET_SSID_COUNT - 1) app->netIndex++;
        } else if(event->key == InputKeyOk) {
            app->netState = 3;
            app->netStartTick = furi_get_tick();
        }
        return true;
    case 4: // wifi connected
        if(event->key == InputKeyOk) {
            app->netState = 1;
            app->netIndex = 0;
            app->netStartTick = furi_get_tick();
        }
        return true;
    case 5: // airplane mode
        if(event->key == InputKeyOk) {
            app->netAirplane = !app->netAirplane;
            if(app->netAirplane) app->netWifiOn = false;
        }
        return true;
    case 7: // 5G modem
        if(event->key == InputKeyOk) app->netModemOn = !app->netModemOn;
        return true;
    case 8: // ethernet
        if(event->key == InputKeyOk) app->netEthLink = !app->netEthLink;
        return true;
    default:
        return true;
    }
}

void CPUStartApp::drawNetworkMenu(Canvas* canvas, CPUStartApp* app) {
    drawStatusBar(canvas, app, "Network");
    canvas_set_font(canvas, FontSecondary);
    static const char* names[5] = {
        "Airplane mode", "Routing info", "5G Modem", "WiFi", "Ethernet"};
    uint8_t scroll = 0;
    if(app->netIndex >= 4) scroll = app->netIndex - 3;
    for(uint8_t i = 0; i < 4; i++) {
        uint8_t idx = scroll + i;
        if(idx >= 5) break;
        uint8_t y = 24 + i * 12;
        bool sel = (idx == app->netIndex);
        if(sel) {
            canvas_draw_box(canvas, 0, y - 9, 128, 12);
            canvas_invert_color(canvas);
        }
        canvas_draw_icon(canvas, 4, y - 8, netIcon(idx));
        canvas_draw_str(canvas, 20, y, names[idx]);
        const char* status = "";
        switch(idx) {
        case 0:
            status = app->netAirplane ? "ON" : "OFF";
            break;
        case 2:
            status = app->netModemOn ? "On" : "Off";
            break;
        case 3:
            status = app->netAirplane ? "Disabled" :
                    app->netWifiOn  ? "On" :
                                      "Off";
            break;
        case 4:
            status = app->netEthLink ? "Link" : "No link";
            break;
        default:
            break;
        }
        canvas_draw_str(canvas, 84, y, status);
        if(sel) canvas_invert_color(canvas);
    }
    if(scroll > 0) {
        canvas_set_color(canvas, ColorWhite);
        canvas_draw_triangle(canvas, 124, 10, 4, 3, CanvasDirectionBottomToTop);
        canvas_set_color(canvas, ColorBlack);
    }
    if((uint8_t)(scroll + 4) < 5) {
        canvas_draw_triangle(canvas, 124, 60, 4, 3, CanvasDirectionTopToBottom);
    }
}

void CPUStartApp::drawNetworkAirplane(Canvas* canvas, CPUStartApp* app) {
    drawStatusBar(canvas, app, "Airplane mode");
    canvas_draw_icon(canvas, 58, 20, &I_net_airplane_12x12);
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(
        canvas, 4, 44, app->netAirplane ? "Airplane mode: ON" : "Airplane mode: OFF");
    canvas_draw_str(canvas, 4, 54, app->netAirplane ? "All radios off" : "Radios enabled");
    canvas_set_font(canvas, FontKeyboard);
    canvas_draw_str(canvas, 2, 63, "[OK] toggle  [BACK] back");
}

void CPUStartApp::drawNetworkRouting(Canvas* canvas, CPUStartApp* app) {
    UNUSED(app);
    drawStatusBar(canvas, app, "Routing info");
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 4, 24, "Kernel IP routing table");
    canvas_draw_str(canvas, 4, 34, "default via 192.168.1.1");
    canvas_draw_str(canvas, 4, 44, "192.168.1.0/24  eth0");
    canvas_draw_str(canvas, 4, 54, "10.0.0.0/8      rmnet0");
    canvas_set_font(canvas, FontKeyboard);
    canvas_draw_str(canvas, 2, 63, "[BACK] back");
}

void CPUStartApp::drawNetworkModem(Canvas* canvas, CPUStartApp* app) {
    drawStatusBar(canvas, app, "5G Modem");
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 4, 24, app->netModemOn ? "NR: ACTIVE" : "NR: OFF");
    canvas_draw_str(canvas, 4, 34, "SIM: OK");
    canvas_draw_str(canvas, 4, 44, "APN: internet");
    canvas_draw_str(canvas, 4, 54, "IP: 10.0.0.7");
    canvas_set_font(canvas, FontKeyboard);
    canvas_draw_str(canvas, 2, 63, "[OK] toggle  [BACK] back");
}

void CPUStartApp::drawNetworkEthernet(Canvas* canvas, CPUStartApp* app) {
    drawStatusBar(canvas, app, "Ethernet");
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 4, 24, app->netEthLink ? "Link: 1000BASE-T" : "Link: down");
    canvas_draw_str(canvas, 4, 34, "MAC: AA:BB:CC:00:11:22");
    canvas_draw_str(canvas, 4, 44, "IP: 192.168.1.50");
    canvas_draw_str(canvas, 4, 54, "DHCP: enabled");
    canvas_set_font(canvas, FontKeyboard);
    canvas_draw_str(canvas, 2, 63, "[OK] toggle  [BACK] back");
}

const Icon* CPUStartApp::netIcon(uint8_t index) {
    static const Icon* icons[] = {
        &I_net_airplane_12x12,
        &I_net_routing_12x12,
        &I_net_modem_12x12,
        &I_net_wifi_12x12,
        &I_net_eth_12x12,
    };
    return icons[index];
}

void CPUStartApp::callbackNetworkDraw(Canvas* canvas, void* context) {
    CPUStartApp* app = *(CPUStartApp**)context;
    canvas_clear(canvas);
    uint32_t tick = furi_get_tick();

    switch(app->netState) {
    case 0: // menu
        drawNetworkMenu(canvas, app);
        break;
    case 1: // wifi scanning
        drawStatusBar(canvas, app, "WiFi");
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str(canvas, 20, 32, "Scanning for networks...");
        {
            uint8_t dots = ((tick / 300) % 3) + 1;
            char buf[8];
            for(uint8_t i = 0; i < dots; i++) buf[i] = '.';
            buf[dots] = '\0';
            canvas_draw_str(canvas, 20, 42, buf);
        }
        break;
    case 2: // wifi list
        drawStatusBar(canvas, app, "WiFi Networks");
        canvas_set_font(canvas, FontSecondary);
        for(uint8_t i = 0; i < NET_SSID_COUNT; i++) {
            uint8_t y = 20 + i * 9;
            if(i == app->netIndex) {
                canvas_draw_box(canvas, 0, y - 6, 128, 9);
                canvas_invert_color(canvas);
            }
            canvas_draw_str(canvas, 4, y, netSSIDs[i]);
            uint8_t bars = ((tick / 600 + i) % 4) + 1;
            for(uint8_t b = 0; b < bars; b++) {
                canvas_draw_line(canvas, 100 + b * 5, y - 1 - b * 2, 100 + b * 5, y - 1);
            }
            if(i == app->netIndex) {
                canvas_invert_color(canvas);
            }
        }
        break;
    case 3: // wifi connecting
        drawStatusBar(canvas, app, "Connecting");
        canvas_set_font(canvas, FontSecondary);
        {
            char buf[48];
            snprintf(buf, sizeof(buf), "Connecting to %s...", netSSIDs[app->netIndex]);
            canvas_draw_str(canvas, 4, 24, buf);
            uint8_t dots = ((tick / 250) % 3) + 1;
            char dotsBuf[8];
            for(uint8_t i = 0; i < dots; i++) dotsBuf[i] = '.';
            dotsBuf[dots] = '\0';
            canvas_draw_str(canvas, 4, 36, dotsBuf);
        }
        break;
    case 4: // wifi connected
        drawStatusBar(canvas, app, "Connected");
        canvas_set_font(canvas, FontSecondary);
        {
            char buf[48];
            snprintf(buf, sizeof(buf), "Connected to %s", netSSIDs[app->netIndex]);
            canvas_draw_str(canvas, 4, 24, buf);
            canvas_draw_str(canvas, 4, 36, "IP: 192.168.1.42");
        }
        canvas_set_font(canvas, FontKeyboard);
        canvas_draw_str(canvas, 2, 63, "[OK] rescan  [BACK] menu");
        break;
    case 5:
        drawNetworkAirplane(canvas, app);
        break;
    case 6:
        drawNetworkRouting(canvas, app);
        break;
    case 7:
        drawNetworkModem(canvas, app);
        break;
    case 8:
        drawNetworkEthernet(canvas, app);
        break;
    default:
        break;
    }
}

bool CPUStartApp::callbackNetworkCustomEvent(uint32_t event, void* context) {
    CPUStartApp* app = static_cast<CPUStartApp*>(context);
    if(event == CPUStartCustomEventTick) {
        uint32_t elapsed = furi_get_tick() - app->netStartTick;
        if(app->netState == 1 && elapsed > 1500) {
            app->netState = 2;
        }
        if(app->netState == 3 && elapsed > 2500) {
            app->netState = 4;
            app->netWifiOn = true;
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
    drawStatusBar(canvas, app, "Settings");
    canvas_set_font(canvas, FontSecondary);

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
                app->termPrompt = "mmc@2a31:~$";
                app->termPrevView = CPUStartViewMainMenu;
                app->termLineCount = 0;
                for(uint8_t i = 0; i < 6; i++) {
                    app->termLines[i][0] = '\0';
                }
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

    // ---- Wallpaper: sun, birds, rolling hills ----
    canvas_draw_disc(canvas, 118, 10, 6);
    canvas_draw_line(canvas, 40, 6, 44, 4);
    canvas_draw_line(canvas, 44, 4, 48, 6);
    canvas_draw_line(canvas, 52, 8, 55, 6);
    canvas_draw_line(canvas, 55, 6, 58, 8);
    canvas_draw_line(canvas, 0, 44, 22, 34);
    canvas_draw_line(canvas, 22, 34, 44, 44);
    canvas_draw_line(canvas, 30, 44, 56, 30);
    canvas_draw_line(canvas, 56, 30, 82, 44);
    canvas_draw_line(canvas, 70, 44, 100, 36);
    canvas_draw_line(canvas, 100, 36, 128, 44);

    // ---- Desktop icons: 2 columns x 3 rows ----
    static const char* icons[] = {
        "Computer", "Files", "Apps", "Settings", "Network", "Testing"};
    static const Icon* ic[] = {
        &I_main_desktop_12x12,
        &I_main_files_12x12,
        &I_main_apps_12x12,
        &I_main_settings_12x12,
        &I_main_network_12x12,
        &I_main_testing_12x12,
    };
    canvas_set_font(canvas, FontSecondary);
    const uint8_t cols[2] = {8, 68};
    for(uint8_t i = 0; i < 6; i++) {
        uint8_t cx = cols[i % 2];
        uint8_t by = 18 + (i / 2) * 14; // label baselines: 18, 32, 46
        bool sel = (i == app->desktopSelected);
        if(sel) {
            canvas_draw_box(canvas, cx - 4, by - 15, 58, 17);
            canvas_draw_icon(canvas, cx, by - 14, ic[i]);
            canvas_draw_str(canvas, cx, by, icons[i]);
        } else {
            canvas_draw_icon(canvas, cx, by - 14, ic[i]);
            canvas_draw_str(canvas, cx, by, icons[i]);
        }
    }

    // ---- Taskbar with Start button + clock ----
    canvas_draw_box(canvas, 0, 56, 128, 8);
    canvas_set_color(canvas, ColorWhite);
    canvas_draw_box(canvas, 2, 57, 26, 6);
    canvas_set_color(canvas, ColorBlack);
    canvas_draw_str(canvas, 4, 62, "Start");
    canvas_set_color(canvas, ColorWhite);
    canvas_draw_str(canvas, 37, 62, "FlipperOS");
    char clock[16];
    uint32_t seconds = (furi_get_tick() - app->desktopStartTick) / 1000;
    snprintf(
        clock,
        sizeof(clock),
        "%02lu:%02lu",
        (unsigned long)(seconds / 60) % 60,
        (unsigned long)seconds % 60);
    canvas_draw_str(canvas, 96, 62, clock);
    canvas_set_color(canvas, ColorBlack);
}

bool CPUStartApp::callbackDesktopInput(InputEvent* event, void* context) {
    CPUStartApp* app = static_cast<CPUStartApp*>(context);
    if(event->type == InputTypeShort || event->type == InputTypeRepeat) {
        if(event->key == InputKeyRight) {
            app->desktopSelected = (app->desktopSelected + 1) % 6;
        } else if(event->key == InputKeyLeft) {
            app->desktopSelected = (app->desktopSelected + 5) % 6;
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
                app->netState = 0;
                app->netIndex = 0;
                view_dispatcher_switch_to_view(app->viewDispatcher, CPUStartViewNetwork);
                break;
            case 5: // Testing
                app->testScreen = TEST_MENU;
                app->testIndex = 0;
                view_dispatcher_switch_to_view(app->viewDispatcher, CPUStartViewTesting);
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
