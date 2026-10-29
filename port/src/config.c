// The player's settings (docs/port.md, "Settings"): kururin.ini next to the executable (or
// --config FILE), read when the game runs in a window and written with the defaults when it
// is missing. The window's scale and fullscreen, the sound's volume, pausing on focus loss, and
// which keys and gamepad buttons are the GBA's. The command line's --scale, --fullscreen,
// --volume and --mute win over the file. Test runs (--headless) never read it.
#include "port.h"

#include <SDL.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// The GBA's buttons in KEYINPUT's bit order
static const char *const kButtons[PORT_NUM_BUTTONS] = { "A", "B", "Select", "Start", "Right",
                                                        "Left", "Up", "Down", "R", "L" };

static const char kDefaults[] =
    "; Kuru Kuru Kururin (PC port): settings. Written with the defaults the first time the game\n"
    "; ran; edit it with any text editor, the game reads it when it starts. The command line's\n"
    "; --scale, --fullscreen, --volume and --mute win over it.\n"
    "\n"
    "[video]\n"
    "; the window is 240x160 times this (1-16)\n"
    "scale = 3\n"
    "; start in fullscreen (F11 or Alt+Enter switch at any time): yes or no\n"
    "fullscreen = no\n"
    "\n"
    "[sound]\n"
    "; 0-100 (percent)\n"
    "volume = 100\n"
    "mute = no\n"
    "\n"
    "[game]\n"
    "; pause while the window does not have the focus (another window was clicked): yes or no\n"
    "pause_on_focus_loss = yes\n"
    "\n"
    "[keyboard]\n"
    "; the GBA's buttons: one or more SDL key names each, separated by commas\n"
    "; (https://wiki.libsdl.org/SDL2/SDL_Keycode: e.g. Z, Return, Space, Left Shift, Keypad 8)\n"
    "A = Z\n"
    "B = X\n"
    "L = A\n"
    "R = S\n"
    "Start = Return\n"
    "Select = Backspace\n"
    "Up = Up\n"
    "Down = Down\n"
    "Left = Left\n"
    "Right = Right\n"
    "\n"
    "[gamepad]\n"
    "; the GBA's buttons: one or more game controller buttons or axis directions each, separated\n"
    "; by commas (SDL's names, for an Xbox-style layout):\n"
    ";   buttons: a b x y back start guide leftshoulder rightshoulder leftstick rightstick\n"
    ";            dpup dpdown dpleft dpright\n"
    ";   axes:    leftx- leftx+ lefty- lefty+ rightx- rightx+ righty- righty+ lefttrigger righttrigger\n"
    "A = a\n"
    "B = b\n"
    "L = leftshoulder, lefttrigger\n"
    "R = rightshoulder, righttrigger\n"
    "Start = start\n"
    "Select = back\n"
    "Up = dpup, lefty-\n"
    "Down = dpdown, lefty+\n"
    "Left = dpleft, leftx-\n"
    "Right = dpright, leftx+\n"
    "; how far a stick or trigger must move to count as pressed (percent)\n"
    "deadzone = 50\n";

#define MAX_BINDINGS 8
struct PadInput {
    int button;  // SDL_GameControllerButton, or -1
    int axis;    // SDL_GameControllerAxis (with button -1)
    int sign;    // +1 or -1: which direction of the axis
};
static SDL_Keycode sKeys[PORT_NUM_BUTTONS][MAX_BINDINGS];
static int sNumKeys[PORT_NUM_BUTTONS];
static struct PadInput sPad[PORT_NUM_BUTTONS][MAX_BINDINGS];
static int sNumPad[PORT_NUM_BUTTONS];
static int sDeadzone = 50;  // percent
// the file's settings (the defaults' text first, then the file's)
static struct {
    int scale, volume;
    bool fullscreen, mute, pauseOnFocusLoss;
} sCfg;

static char *trim(char *s) {
    while (isspace((unsigned char)*s))
        s++;
    char *e = s + strlen(s);
    while (e > s && isspace((unsigned char)e[-1]))
        *--e = 0;
    return s;
}

static bool parseBool(const char *v, bool *out) {
    if (!SDL_strcasecmp(v, "yes") || !SDL_strcasecmp(v, "true") || !strcmp(v, "1") || !SDL_strcasecmp(v, "on"))
        return *out = true, true;
    if (!SDL_strcasecmp(v, "no") || !SDL_strcasecmp(v, "false") || !strcmp(v, "0") || !SDL_strcasecmp(v, "off"))
        return *out = false, true;
    return false;
}

static int buttonIndex(const char *name) {
    for (int i = 0; i < PORT_NUM_BUTTONS; i++)
        if (!SDL_strcasecmp(name, kButtons[i]))
            return i;
    return -1;
}

static bool parsePad(const char *tok, struct PadInput *in) {
    SDL_GameControllerButton b = SDL_GameControllerGetButtonFromString(tok);
    if (b != SDL_CONTROLLER_BUTTON_INVALID) {
        *in = (struct PadInput){ b, -1, 0 };
        return true;
    }
    char name[64];
    snprintf(name, sizeof(name), "%s", tok);
    size_t n = strlen(name);
    int sign = 1;
    if (n && (name[n - 1] == '+' || name[n - 1] == '-'))
        sign = name[n - 1] == '-' ? -1 : 1, name[n - 1] = 0;
    SDL_GameControllerAxis a = SDL_GameControllerGetAxisFromString(name);
    if (a == SDL_CONTROLLER_AXIS_INVALID)
        return false;
    *in = (struct PadInput){ -1, a, sign };
    return true;
}

// One "key = value" line of section *section*; *where* names it for warnings.
static void setting(const char *where, const char *section, const char *key, char *value) {
    bool b;
    char *end;
    long v = strtol(value, &end, 10);
    bool number = end != value && !*end;
    int button = buttonIndex(key);
    if (!SDL_strcasecmp(section, "video") && !SDL_strcasecmp(key, "scale") && number && v >= 1 && v <= 16) {
        sCfg.scale = (int)v;
        return;
    } else if (!SDL_strcasecmp(section, "video") && !SDL_strcasecmp(key, "fullscreen") && parseBool(value, &b)) {
        sCfg.fullscreen = b;
        return;
    } else if (!SDL_strcasecmp(section, "sound") && !SDL_strcasecmp(key, "volume") && number && v >= 0 && v <= 100) {
        sCfg.volume = (int)v;
        return;
    } else if (!SDL_strcasecmp(section, "sound") && !SDL_strcasecmp(key, "mute") && parseBool(value, &b)) {
        sCfg.mute = b;
        return;
    } else if (!SDL_strcasecmp(section, "game") && !SDL_strcasecmp(key, "pause_on_focus_loss") && parseBool(value, &b)) {
        sCfg.pauseOnFocusLoss = b;
        return;
    } else if (!SDL_strcasecmp(section, "gamepad") && !SDL_strcasecmp(key, "deadzone") && number && v >= 1 && v <= 99) {
        sDeadzone = (int)v;
        return;
    } else if ((!SDL_strcasecmp(section, "keyboard") || !SDL_strcasecmp(section, "gamepad")) && button >= 0) {
        bool keyboard = !SDL_strcasecmp(section, "keyboard");
        int n = 0;
        for (char *tok = strtok(value, ","); tok; tok = strtok(NULL, ",")) {
            tok = trim(tok);
            if (!*tok)
                continue;
            if (n == MAX_BINDINGS) {
                fprintf(stderr, "kururin_pc: %s: at most %d for %s, the rest left out\n", where, MAX_BINDINGS, key);
                break;
            }
            if (keyboard) {
                SDL_Keycode k = SDL_GetKeyFromName(tok);
                if (k == SDLK_UNKNOWN)
                    fprintf(stderr, "kururin_pc: %s: no key named '%s'\n", where, tok);
                else
                    sKeys[button][n++] = k;
            } else if (parsePad(tok, &sPad[button][n])) {
                n++;
            } else {
                fprintf(stderr, "kururin_pc: %s: no gamepad button or axis named '%s'\n", where, tok);
            }
        }
        if (keyboard)
            sNumKeys[button] = n;
        else
            sNumPad[button] = n;
        return;
    }
    fprintf(stderr, "kururin_pc: %s: ignored ([%s] %s = %s)\n", where, section, key, value);
}

static void parse(char *text, const char *name) {
    char section[32] = "", line[512];
    int lineNo = 0;
    for (char *p = text; *p;) {
        size_t n = strcspn(p, "\n");
        snprintf(line, sizeof(line), "%.*s", (int)(n < sizeof(line) - 1 ? n : sizeof(line) - 1), p);
        p += n + (p[n] == '\n');
        lineNo++;
        char *s = trim(line), where[600];
        snprintf(where, sizeof(where), "%s:%d", name, lineNo);
        if (!*s || *s == ';' || *s == '#')
            continue;
        if (*s == '[') {
            char *e = strchr(s, ']');
            if (e) {
                *e = 0;
                snprintf(section, sizeof(section), "%s", trim(s + 1));
                continue;
            }
        }
        char *eq = strchr(s, '=');
        if (!eq) {
            fprintf(stderr, "kururin_pc: %s: expected 'name = value'\n", where);
            continue;
        }
        *eq = 0;
        setting(where, section, trim(s), trim(eq + 1));
    }
}

void Port_LoadConfig(const char *path) {
    char defaultPath[4096];
    if (!path) {
        char *base = SDL_GetBasePath();
        snprintf(defaultPath, sizeof(defaultPath), "%skururin.ini", base ? base : "");
        SDL_free(base);
        path = defaultPath;
    }
    // the defaults first (the same text a new file gets), then the file over them
    char *text = SDL_strdup(kDefaults);
    parse(text, "(defaults)");
    SDL_free(text);
    FILE *f = fopen(path, "rb");
    if (f) {
        static char buf[65536];
        size_t n = fread(buf, 1, sizeof(buf) - 1, f);
        buf[n] = 0;
        fclose(f);
        for (size_t i = 0; i < n; i++)
            if (buf[i] == '\r')
                buf[i] = ' ';  // a file saved with Windows line ends
        parse(buf, path);
    } else if ((f = fopen(path, "w"))) {
        fputs(kDefaults, f);
        fclose(f);
    }
    // the command line wins
    struct PortOptions *o = &gPortOptions;
    if (!o->scale)
        o->scale = sCfg.scale;
    if (o->volume < 0)
        o->volume = sCfg.volume;
    if (o->fullscreen < 0)
        o->fullscreen = sCfg.fullscreen;
    o->mute = o->mute || sCfg.mute;
    o->pauseOnFocusLoss = sCfg.pauseOnFocusLoss;
}

uint16_t Port_KeyboardBits(void) {
    const Uint8 *held = SDL_GetKeyboardState(NULL);
    bool alt = SDL_GetModState() & KMOD_ALT;  // Alt+Enter is the fullscreen switch, not a button
    uint16_t bits = 0;
    for (int b = 0; b < PORT_NUM_BUTTONS; b++)
        for (int i = 0; i < sNumKeys[b]; i++)
            if (held[SDL_GetScancodeFromKey(sKeys[b][i])] && !(alt && sKeys[b][i] == SDLK_RETURN))
                bits |= (uint16_t)(1u << b);
    return bits;
}

uint16_t Port_KeyBits(int32_t key) {
    uint16_t bits = 0;
    for (int b = 0; b < PORT_NUM_BUTTONS; b++)
        for (int i = 0; i < sNumKeys[b]; i++)
            if (sKeys[b][i] == key)
                bits |= (uint16_t)(1u << b);
    return bits;
}

uint16_t Port_GamepadButtonBits(int button) {
    uint16_t bits = 0;
    for (int b = 0; b < PORT_NUM_BUTTONS; b++)
        for (int i = 0; i < sNumPad[b]; i++)
            if (sPad[b][i].button == button)
                bits |= (uint16_t)(1u << b);
    return bits;
}

uint16_t Port_GamepadBits(void *controller) {
    SDL_GameController *c = controller;
    int dead = 32767 * sDeadzone / 100;
    uint16_t bits = 0;
    for (int b = 0; b < PORT_NUM_BUTTONS; b++)
        for (int i = 0; i < sNumPad[b]; i++) {
            const struct PadInput *in = &sPad[b][i];
            bool on = in->button >= 0 ? SDL_GameControllerGetButton(c, in->button)
                                      : SDL_GameControllerGetAxis(c, in->axis) * in->sign > dead;
            if (on)
                bits |= (uint16_t)(1u << b);
        }
    return bits;
}
