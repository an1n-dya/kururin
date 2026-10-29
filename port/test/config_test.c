// The settings and the gamepad mapping (port/src/config.c), without a window: make -C port
// test-config. A kururin.ini is written with some settings changed, read back, and SDL's virtual
// joystick (a game controller with every button and axis) plays the gamepad: each GBA button
// must come from the right buttons and axis directions, and only from them.
#include "../src/port.h"

#define SDL_MAIN_HANDLED  // this main is the program's (no SDL2main)
#include <SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct PortOptions gPortOptions;  // what config.c fills (frame.c's in the game)

static int sFailures;
#define CHECK(cond, ...) \
    do { \
        if (!(cond)) { \
            printf("FAIL %s:%d: ", __FILE__, __LINE__); \
            printf(__VA_ARGS__); \
            putchar('\n'); \
            sFailures++; \
        } \
    } while (0)

enum { A = 1, B = 2, SELECT = 4, START = 8, RIGHT = 16, LEFT = 32, UP = 64, DOWN = 128, R = 256, L = 512 };

static uint16_t padWith(SDL_GameController *pad, SDL_Joystick *joy, int button, int axis, int value) {
    for (int b = 0; b < SDL_CONTROLLER_BUTTON_MAX; b++)
        SDL_JoystickSetVirtualButton(joy, b, b == button);
    // a virtual axis is raw (-32768..32767): a trigger at rest is -32768 (0 to the game controller)
    for (int a = 0; a < SDL_CONTROLLER_AXIS_MAX; a++) {
        bool trigger = a == SDL_CONTROLLER_AXIS_TRIGGERLEFT || a == SDL_CONTROLLER_AXIS_TRIGGERRIGHT;
        SDL_JoystickSetVirtualAxis(joy, a, a == axis ? value : trigger ? -32768 : 0);
    }
    SDL_JoystickUpdate();
    SDL_GameControllerUpdate();
    return Port_GamepadBits(pad);
}

int main(int argc, char **argv) {
    const char *dir = argc > 1 ? argv[1] : ".";
    char path[4096];
    snprintf(path, sizeof(path), "%s/config_test.ini", dir);
    remove(path);

    // 1. No file: the defaults, and the file is written with them
    gPortOptions = (struct PortOptions){ .volume = -1, .fullscreen = -1 };
    Port_LoadConfig(path);
    CHECK(gPortOptions.scale == 3 && gPortOptions.volume == 100 && !gPortOptions.fullscreen && !gPortOptions.mute &&
              gPortOptions.pauseOnFocusLoss,
          "defaults: scale %d volume %d fullscreen %d mute %d pause %d", gPortOptions.scale, gPortOptions.volume,
          gPortOptions.fullscreen, gPortOptions.mute, gPortOptions.pauseOnFocusLoss);
    FILE *f = fopen(path, "r");
    CHECK(f != NULL, "%s was not written", path);
    if (f)
        fclose(f);
    CHECK(Port_KeyBits(SDLK_z) == A && Port_KeyBits(SDLK_x) == B && Port_KeyBits(SDLK_RETURN) == START &&
              Port_KeyBits(SDLK_BACKSPACE) == SELECT && Port_KeyBits(SDLK_a) == L && Port_KeyBits(SDLK_s) == R &&
              Port_KeyBits(SDLK_UP) == UP && Port_KeyBits(SDLK_LEFT) == LEFT && Port_KeyBits(SDLK_SPACE) == 0,
          "default keys");

    // 2. A file with changes (Windows line ends, odd spacing, an unknown name): read over the
    // defaults; the command line wins
    f = fopen(path, "wb");
    fputs("[video]\r\nscale=5\r\nfullscreen = yes\r\n[sound]\r\nvolume = 40\r\n[game]\r\npause_on_focus_loss = no\r\n"
          "[keyboard]\r\nStart = Space, Return\r\nA = K\r\nB = NoSuchKey, J\r\n"
          "[gamepad]\r\nA = x\r\nUp = dpup, righty-\r\ndeadzone = 25\r\n",
          f);
    fclose(f);
    gPortOptions = (struct PortOptions){ .volume = 70, .fullscreen = -1 };  // as if --volume 70
    Port_LoadConfig(path);
    CHECK(gPortOptions.scale == 5 && gPortOptions.volume == 70 && gPortOptions.fullscreen == 1 &&
              !gPortOptions.pauseOnFocusLoss,
          "file: scale %d volume %d fullscreen %d pause %d", gPortOptions.scale, gPortOptions.volume,
          gPortOptions.fullscreen, gPortOptions.pauseOnFocusLoss);
    CHECK(Port_KeyBits(SDLK_SPACE) == START && Port_KeyBits(SDLK_RETURN) == START && Port_KeyBits(SDLK_k) == A &&
              Port_KeyBits(SDLK_z) == 0 && Port_KeyBits(SDLK_j) == B && Port_KeyBits(SDLK_x) == 0 &&
              Port_KeyBits(SDLK_BACKSPACE) == SELECT,
          "remapped keys");

    // 3. The gamepad (the file's mapping: A on x, Up also on the right stick, deadzone 25%;
    // the rest the defaults)
    if (SDL_Init(SDL_INIT_GAMECONTROLLER)) {
        printf("FAIL SDL_Init: %s\n", SDL_GetError());
        return 1;
    }
    int index = SDL_JoystickAttachVirtual(SDL_JOYSTICK_TYPE_GAMECONTROLLER, SDL_CONTROLLER_AXIS_MAX,
                                          SDL_CONTROLLER_BUTTON_MAX, 0);
    SDL_GameController *pad = index >= 0 ? SDL_GameControllerOpen(index) : NULL;
    if (!pad) {
        printf("FAIL no virtual game controller: %s\n", SDL_GetError());
        return 1;
    }
    SDL_Joystick *joy = SDL_GameControllerGetJoystick(pad);
    struct { int button, axis, value; uint16_t want; const char *what; } cases[] = {
        { -1, -1, 0, 0, "nothing" },
        { SDL_CONTROLLER_BUTTON_X, -1, 0, A, "x (A, remapped)" },
        { SDL_CONTROLLER_BUTTON_A, -1, 0, 0, "a (no longer A)" },
        { SDL_CONTROLLER_BUTTON_B, -1, 0, B, "b" },
        { SDL_CONTROLLER_BUTTON_START, -1, 0, START, "start" },
        { SDL_CONTROLLER_BUTTON_BACK, -1, 0, SELECT, "back" },
        { SDL_CONTROLLER_BUTTON_LEFTSHOULDER, -1, 0, L, "leftshoulder" },
        { SDL_CONTROLLER_BUTTON_RIGHTSHOULDER, -1, 0, R, "rightshoulder" },
        { SDL_CONTROLLER_BUTTON_DPAD_UP, -1, 0, UP, "dpup" },
        { SDL_CONTROLLER_BUTTON_DPAD_DOWN, -1, 0, DOWN, "dpdown" },
        { SDL_CONTROLLER_BUTTON_DPAD_LEFT, -1, 0, LEFT, "dpleft" },
        { SDL_CONTROLLER_BUTTON_DPAD_RIGHT, -1, 0, RIGHT, "dpright" },
        { -1, SDL_CONTROLLER_AXIS_LEFTX, -20000, LEFT, "leftx-" },
        { -1, SDL_CONTROLLER_AXIS_LEFTX, 20000, RIGHT, "leftx+" },
        { -1, SDL_CONTROLLER_AXIS_LEFTY, -20000, 0, "lefty- (Up replaced by dpup, righty-)" },
        { -1, SDL_CONTROLLER_AXIS_LEFTY, 20000, DOWN, "lefty+ (down)" },
        { -1, SDL_CONTROLLER_AXIS_LEFTX, -6000, 0, "leftx- inside the deadzone" },
        { -1, SDL_CONTROLLER_AXIS_RIGHTY, -20000, UP, "righty- (Up, added)" },
        { -1, SDL_CONTROLLER_AXIS_TRIGGERLEFT, 0, L, "lefttrigger half way" },
        { -1, SDL_CONTROLLER_AXIS_TRIGGERRIGHT, 0, R, "righttrigger half way" },
        { -1, SDL_CONTROLLER_AXIS_TRIGGERLEFT, -25000, 0, "lefttrigger inside the deadzone" },
    };
    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        uint16_t got = padWith(pad, joy, cases[i].button, cases[i].axis, cases[i].value);
        CHECK(got == cases[i].want, "gamepad %s: 0x%03X, want 0x%03X", cases[i].what, got, cases[i].want);
    }
    CHECK(Port_GamepadButtonBits(SDL_CONTROLLER_BUTTON_X) == A && Port_GamepadButtonBits(SDL_CONTROLLER_BUTTON_A) == 0,
          "gamepad button presses");
    SDL_GameControllerClose(pad);
    SDL_Quit();
    remove(path);
    if (!sFailures)
        printf("ok   config: defaults, the file over them, the command line over it, %zu gamepad inputs\n",
               sizeof(cases) / sizeof(cases[0]));
    return sFailures != 0;
}
