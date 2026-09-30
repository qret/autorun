#ifndef AUTORUN_GAME_SPIDERMAN_WOS_H
#define AUTORUN_GAME_SPIDERMAN_WOS_H

static const struct game_setting_choice spiderman_wos_toggle[] = { { "Disabled", "0" }, { "Enabled", "1" } };
static const struct game_setting_choice spiderman_wos_resolution[] = {
    { "800 x 600", "800x600" }, { "1024 x 768", "1024x768" }, { "1280 x 720", "1280x720" },
    { "1280 x 800", "1280x800" }, { "1280 x 1024", "1280x1024" },
    { "1600 x 900", "1600x900" }, { "1920 x 1080", "1920x1080" }
};
static const struct game_setting_choice spiderman_wos_display[] = { { "Fullscreen", "0" }, { "Windowed", "1" } };
static const struct game_setting_choice spiderman_wos_keys[] = {
    { "Unbound", "0" }, { "Mouse left", "524" }, { "Mouse right", "525" }, { "Mouse middle", "526" },
    { "Mouse 4", "527" }, { "Mouse 5", "528" },
    { "A", "286" }, { "B", "304" }, { "C", "302" }, { "D", "288" }, { "E", "274" },
    { "F", "289" }, { "G", "290" }, { "H", "291" }, { "I", "279" }, { "J", "292" },
    { "K", "293" }, { "L", "294" }, { "M", "306" }, { "N", "305" }, { "O", "280" },
    { "P", "281" }, { "Q", "272" }, { "R", "275" }, { "S", "287" }, { "T", "276" },
    { "U", "278" }, { "V", "303" }, { "W", "273" }, { "X", "301" }, { "Y", "277" }, { "Z", "300" },
    { "0", "267" }, { "1", "258" }, { "2", "259" }, { "3", "260" }, { "4", "261" },
    { "5", "262" }, { "6", "263" }, { "7", "264" }, { "8", "265" }, { "9", "266" },
    { "Space", "313" }, { "Tab", "271" }, { "Enter", "284" }, { "Escape", "257" },
    { "Backspace", "270" }, { "Caps Lock", "314" }, { "Left Shift", "298" }, { "Right Shift", "310" },
    { "Left Ctrl", "285" }, { "Right Ctrl", "413" }, { "Left Alt", "312" }, { "Right Alt", "440" },
    { "Arrow left", "459" }, { "Arrow right", "461" }, { "Arrow up", "456" }, { "Arrow down", "464" },
    { "Insert", "466" }, { "Delete", "467" }, { "Home", "455" }, { "End", "463" },
    { "Page Up", "457" }, { "Page Down", "465" },
    { "F1", "315" }, { "F2", "316" }, { "F3", "317" }, { "F4", "318" }, { "F5", "319" },
    { "F6", "320" }, { "F7", "321" }, { "F8", "322" }, { "F9", "323" }, { "F10", "324" },
    { "F11", "343" }, { "F12", "344" },
    { "Minus", "268" }, { "Equals", "269" }, { "Left bracket", "282" }, { "Right bracket", "283" },
    { "Semicolon", "295" }, { "Apostrophe", "296" }, { "Grave", "297" }, { "Backslash", "299" },
    { "Comma", "307" }, { "Period", "308" }, { "Slash", "309" },
    { "Num Lock", "325" }, { "Numpad 0", "338" }, { "Numpad 1", "335" }, { "Numpad 2", "336" },
    { "Numpad 3", "337" }, { "Numpad 4", "331" }, { "Numpad 5", "332" }, { "Numpad 6", "333" },
    { "Numpad 7", "327" }, { "Numpad 8", "328" }, { "Numpad 9", "329" },
    { "Numpad add", "334" }, { "Numpad subtract", "330" }, { "Numpad multiply", "311" },
    { "Numpad divide", "437" }, { "Numpad decimal", "339" }, { "Numpad enter", "412" }
};

#define WOS_CHOICES(array) array, sizeof(array) / sizeof(array[0])
#define WOS_BINDING(key, label, value) \
    { NULL, "Action" key, NULL, label, { NULL }, value, WOS_CHOICES(spiderman_wos_keys), GAME_SETTING_INTEGER }
static const struct game_setting spiderman_wos_settings[] = {
    { NULL, "VideoW", "VideoH", "Resolution", { NULL }, "1280x720",
      WOS_CHOICES(spiderman_wos_resolution), GAME_SETTING_RESOLUTION },
    { NULL, "Windowed", NULL, "Display mode", { NULL }, "0",
      WOS_CHOICES(spiderman_wos_display), GAME_SETTING_INTEGER },
    { NULL, "VideoVSync", NULL, "VSync", { NULL }, "0",
      WOS_CHOICES(spiderman_wos_toggle), GAME_SETTING_INTEGER },
    { NULL, "VideoParticles", NULL, "Particles", { NULL }, "1",
      WOS_CHOICES(spiderman_wos_toggle), GAME_SETTING_INTEGER },
    { NULL, "VideoShadows", NULL, "Shadows", { NULL }, "1",
      WOS_CHOICES(spiderman_wos_toggle), GAME_SETTING_INTEGER },
    { NULL, "VideoPostProcFx", NULL, "Post-processing", { NULL }, "1",
      WOS_CHOICES(spiderman_wos_toggle), GAME_SETTING_INTEGER },
    { NULL, "MouseSens", NULL, "Mouse sensitivity", { NULL }, "50", NULL, 0, GAME_SETTING_NUMBER },
    WOS_BINDING("Left", "Move left", "286"),
    WOS_BINDING("Right", "Move right", "288"),
    WOS_BINDING("Forward", "Move forward", "273"),
    WOS_BINDING("Backward", "Move backward", "287"),
    WOS_BINDING("Suit", "Switch suit", "314"),
    WOS_BINDING("WallCrawl", "Wall crawl", "272"),
    WOS_BINDING("WallSprint", "Wall sprint", "274"),
    WOS_BINDING("Target", "Lock target", "271"),
    WOS_BINDING("Swing", "Web swing", "525"),
    WOS_BINDING("Jump", "Jump", "313"),
    WOS_BINDING("Shoot", "Shoot web", "300"),
    WOS_BINDING("Attack", "Attack", "524"),
    WOS_BINDING("WebStrike", "Web strike", "302"),
    WOS_BINDING("Upgrades", "Upgrades", "289"),
    WOS_BINDING("AllyPrev", "Previous ally", "459"),
    WOS_BINDING("AllyNext", "Next ally", "461"),
    WOS_BINDING("AllySummon", "Summon ally", "464"),
    WOS_BINDING("AllyDismiss", "Dismiss ally", "456"),
    WOS_BINDING("CenterCamera", "Center camera", "526")
};
#undef WOS_BINDING
#undef WOS_CHOICES

static const struct game_settings_profile spiderman_wos_profile = {
    "Spider-Man Web of Shadows.exe", 0x014c,
    "drive_c/users/steamuser/AppData/Local/Activision/Spider-Man Web of Shadows/Config.xml",
    NULL, 0, spiderman_wos_settings, sizeof(spiderman_wos_settings) / sizeof(spiderman_wos_settings[0]),
    GAME_SETTINGS_XML, "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n<r>\n  <s id=\"Version\">7</s>\n</r>\n",
    { "VideoDesiredW", "VideoDesiredH" }
};

#endif
