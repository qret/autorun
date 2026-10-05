#ifndef AUTORUN_FF7REMAKE_SETTINGS_H
#define AUTORUN_FF7REMAKE_SETTINGS_H

static const struct game_setting_choice ff7remake_api[] = {
    { "Game default", NULL }, { "DirectX 11", "11" }, { "DirectX 12", "12" }
};
static const struct game_setting_choice ff7remake_scale[] = {
    { "Game default", NULL }, { "25%", "25" }, { "33%", "33" }, { "50%", "50" },
    { "67%", "67" }, { "75%", "75" }, { "100%", "100" }
};
static const struct game_setting_choice ff7remake_aa[] = {
    { "Game default", NULL }, { "Off", "0" }, { "Low", "2" }, { "Medium", "3" },
    { "High", "4" }, { "Ultra", "6" }
};
static const struct game_setting_choice ff7remake_toggle[] = {
    { "Game default", NULL }, { "Off", "0" }, { "On", "1" }
};
static const struct game_setting_choice ff7remake_shadows[] = {
    { "Game default", NULL }, { "Off", "0" }, { "Low", "1" }, { "Medium", "3" }, { "High", "5" }
};
static const struct game_setting_choice ff7remake_motion_blur[] = {
    { "Game default", NULL }, { "Off", "0" }, { "Low", "2" }, { "High", "4" }
};
static const struct game_setting_choice ff7remake_depth_of_field[] = {
    { "Game default", NULL }, { "Off", "0" }, { "Low", "1" }, { "High", "2" }
};
static const struct game_setting_choice ff7remake_occlusion[] = {
    { "Game default", NULL }, { "Off", "0" }, { "Low", "1" }, { "Medium", "2" },
    { "High", "3" }
};
static const struct game_setting_choice ff7remake_distance[] = {
    { "Game default", NULL }, { "25%", "0.25" }, { "50%", "0.5" }, { "75%", "0.75" }, { "100%", "1" }
};
static const struct game_setting_choice ff7remake_lod[] = {
    { "Game default", NULL }, { "Full detail", "0" }, { "Reduced", "1" }, { "Minimum", "2" }
};
static const struct game_setting_choice ff7remake_filtering[] = {
    { "Game default", NULL }, { "1x", "1" }, { "2x", "2" }, { "4x", "4" }, { "8x", "8" }, { "16x", "16" }
};

#define FF7_CHOICES(array) array, sizeof(array) / sizeof(array[0])
#define FF7_SETTING(key, label, choices) \
    { "SystemSettings", key, NULL, label, { NULL }, NULL, FF7_CHOICES(choices), GAME_SETTING_NUMBER }
static const struct game_setting ff7remake_settings[] = {
    { "Autorun", "GraphicsAPI", NULL, "Graphics API", { NULL }, NULL, FF7_CHOICES(ff7remake_api), GAME_SETTING_D3D_API },
    FF7_SETTING("r.ScreenPercentage", "Render scale", ff7remake_scale),
    FF7_SETTING("r.PostProcessAAQuality", "Anti-aliasing", ff7remake_aa),
    FF7_SETTING("r.ShadowQuality", "Shadows", ff7remake_shadows),
    FF7_SETTING("r.ViewDistanceScale", "View distance", ff7remake_distance),
    FF7_SETTING("r.SkeletalMeshLODBias", "Character detail", ff7remake_lod),
    FF7_SETTING("r.Streaming.MipBias", "Texture detail", ff7remake_lod),
    FF7_SETTING("r.MaxAnisotropy", "Texture filtering", ff7remake_filtering),
    FF7_SETTING("r.MotionBlurQuality", "Motion blur", ff7remake_motion_blur),
    FF7_SETTING("r.BloomQuality", "Bloom", ff7remake_shadows),
    FF7_SETTING("r.DepthOfFieldQuality", "Depth of field", ff7remake_depth_of_field),
    FF7_SETTING("r.AmbientOcclusionLevels", "Ambient occlusion", ff7remake_occlusion),
    FF7_SETTING("r.VolumetricFog", "Volumetric fog", ff7remake_toggle),
    FF7_SETTING("r.SceneColorFringeQuality", "Chromatic aberration", ff7remake_toggle)
};
#undef FF7_SETTING
#undef FF7_CHOICES

static const char *const ff7remake_help[] = {
    "DX11 uses DXVK; DX12 uses VKD3D. Applied on launch.",
    "Fixes internal resolution at the selected percentage.",
    "Lower quality reduces GPU work; Off leaves jagged edges.",
    "Lower quality reduces GPU work. Off can affect indoor lighting.",
    "Distance at which objects remain visible.",
    "Higher LOD bias reduces character mesh detail.",
    "Higher mip bias uses lower-resolution streamed textures.",
    "Sharpness of textures viewed at an angle.",
    "Blur during camera and object movement.",
    "Glow around bright areas.",
    "Blur outside the camera's focus range.",
    "Contact shadows around objects.",
    "Volume lighting in fog.",
    "Colour fringing around screen edges."
};
_Static_assert( sizeof(ff7remake_help) / sizeof(ff7remake_help[0]) ==
                sizeof(ff7remake_settings) / sizeof(ff7remake_settings[0]), "FF7 setting help count" );

static int ff7remake_update( struct game_settings *settings, unsigned int index, const char *value )
{
    static const char *const bounds[] = { "r.DynamicRes.MinScreenPercentage", "r.DynamicRes.MaxScreenPercentage" };

    if (strcmp( settings->profile->settings[index].key, "r.ScreenPercentage" )) return 1;
    for (unsigned int i = 0; i < sizeof(bounds) / sizeof(bounds[0]); i++)
        if (value ? !ini_set( settings, "SystemSettings", bounds[i], value ) :
                    !ini_remove( settings, "SystemSettings", bounds[i] )) return 0;
    return 1;
}

static const struct game_settings_profile ff7remake_profile = {
    .executable = "ff7remake_.exe", .machine = 0x8664,
    .path = "drive_c/users/steamuser/Documents/My Games/FINAL FANTASY VII REMAKE/Saved/Config/WindowsNoEditor/Engine.ini",
    .settings = ff7remake_settings, .count = sizeof(ff7remake_settings) / sizeof(ff7remake_settings[0]),
    .format = GAME_SETTINGS_INI, .update = ff7remake_update, .help = ff7remake_help
};

#endif
