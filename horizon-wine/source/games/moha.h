#ifndef AUTORUN_GAME_MOHA_H
#define AUTORUN_GAME_MOHA_H

static const struct game_setting_choice moha_toggle[] = { { "Disabled", "False" }, { "Enabled", "True" } };
static const struct game_setting_choice moha_resolution[] = {
    { "800 x 600", "800x600" }, { "960 x 540", "960x540" }, { "1280 x 720", "1280x720" },
    { "1600 x 900", "1600x900" }, { "1920 x 1080", "1920x1080" }
};
static const struct game_setting_choice moha_decals[] = {
    { "20", "20" }, { "50", "50" }, { "Unlimited", "-1" }
};
static const struct game_setting_choice moha_particles[] = {
    { "500", "500" }, { "700", "700" }, { "Unlimited", "-1" }
};
static const struct game_setting_choice moha_textures[] = {
    { "Low", "(MinLODSize=64,MaxLODSize=1024,LODBias=2)" },
    { "Medium", "(MinLODSize=128,MaxLODSize=2048,LODBias=1)" },
    { "High", "(MinLODSize=256,MaxLODSize=4096,LODBias=0)" }
};
static const struct game_setting_choice moha_targets[] = {
    { "Low", "(MinLODSize=1,MaxLODSize=1024,LODBias=2)" },
    { "Medium", "(MinLODSize=1,MaxLODSize=2048,LODBias=1)" },
    { "High", "(MinLODSize=1,MaxLODSize=4096,LODBias=0)" }
};

#define MOHA_CHOICES(array) array, sizeof(array) / sizeof(array[0])
#define MOHA_SCALABILITY "MOHAGame.MOHAScalabilityOptions"
#define MOHA_BOOL(key, label, low, medium, high) \
    { MOHA_SCALABILITY, key, NULL, label, { low, medium, high }, NULL, MOHA_CHOICES(moha_toggle), GAME_SETTING_TEXT }
#define MOHA_NUMBER(key, label, low, medium, high) \
    { MOHA_SCALABILITY, key, NULL, label, { low, medium, high }, NULL, NULL, 0, GAME_SETTING_NUMBER }
#define MOHA_TEXTURE(key, label) \
    { "TextureLODSettings", "TEXTUREGROUP_" key, NULL, label, \
      { "(MinLODSize=64,MaxLODSize=1024,LODBias=2)", "(MinLODSize=128,MaxLODSize=2048,LODBias=1)", \
        "(MinLODSize=256,MaxLODSize=4096,LODBias=0)" }, NULL, MOHA_CHOICES(moha_textures), GAME_SETTING_TEXT }

static const struct game_setting moha_settings[] = {
    { "WinDrv.WindowsClient", "StartupResolutionX", "StartupResolutionY", "Resolution", { NULL }, "1280x720",
      MOHA_CHOICES(moha_resolution), GAME_SETTING_RESOLUTION },
    { "WinDrv.WindowsClient", "StartupFullscreen", NULL, "Fullscreen", { NULL }, "True",
      MOHA_CHOICES(moha_toggle), GAME_SETTING_TEXT },
    { "Engine.Engine", "bSubtitlesEnabled", NULL, "Subtitles", { NULL }, NULL,
      MOHA_CHOICES(moha_toggle), GAME_SETTING_TEXT },
    { "ALAudio.ALAudioDevice", "MaxChannels", NULL, "Audio channels", { "20", "90", "91" }, NULL,
      NULL, 0, GAME_SETTING_INTEGER },
    { "ALAudio.ALAudioDevice", "UseEffectsProcessing", NULL, "Audio effects", { "False", "False", "True" }, NULL,
      MOHA_CHOICES(moha_toggle), GAME_SETTING_TEXT },
    MOHA_BOOL( "bTickLODAnims", "Animation LOD", "True", "False", "False" ),
    MOHA_BOOL( "bAllowDynamicDecals", "Dynamic decals", "True", "True", "True" ),
    MOHA_BOOL( "bAllowStaticDecals", "Static decals", "False", "True", "False" ),
    MOHA_BOOL( "bAllowDynamicLights", "Dynamic lights", "True", "True", "True" ),
    MOHA_BOOL( "bAllowDynamicShadows", "Dynamic shadows", "True", "True", "True" ),
    MOHA_BOOL( "bAllowLightEnvironmentShadows", "Environment shadows", "True", "True", "True" ),
    MOHA_BOOL( "bGroupShadows", "Grouped shadows", "True", "False", "False" ),
    MOHA_BOOL( "bForceFallbackShader", "Simplified shaders", "True", "False", "False" ),
    MOHA_BOOL( "bAllowBloom", "Bloom", "False", "True", "True" ),
    MOHA_BOOL( "bAllowDepthOfField", "Depth of field", "False", "True", "True" ),
    MOHA_BOOL( "bAllowMotionBlur", "Motion blur", "False", "False", "True" ),
    MOHA_BOOL( "bPostFX", "Post-processing", "False", "True", "True" ),
    MOHA_BOOL( "bSimplePostFX", "Simplified post-processing", "False", "True", "False" ),
    MOHA_BOOL( "bMinspecCull", "Low-spec object culling", "True", "False", "False" ),
    MOHA_NUMBER( "ScreenPercentage", "Render scale (%)", "100", "100", "100" ),
    MOHA_NUMBER( "SkeletalMeshLODBias", "Character LOD bias", "1", "1", "0" ),
    MOHA_NUMBER( "StaticMeshLODBias", "Object LOD bias", "2", "1", "0" ),
    MOHA_NUMBER( "CullDistanceBiasBase", "Object distance scale", "0.4", "0.6", "1" ),
    MOHA_NUMBER( "ParticleCullExemptDistance", "Particle culling distance", "4000", "4000", "0" ),
    MOHA_NUMBER( "ParticleLODBias", "Particle LOD bias", "3", "2", "0" ),
    { MOHA_SCALABILITY, "MaxNumberOfDynamicDecals", NULL, "Dynamic decal limit", { "20", "50", "-1" }, NULL,
      MOHA_CHOICES(moha_decals), GAME_SETTING_TEXT },
    { MOHA_SCALABILITY, "MaxNumberOfStaticDecals", NULL, "Static decal limit", { "0", "0", "0" }, NULL,
      NULL, 0, GAME_SETTING_INTEGER },
    { MOHA_SCALABILITY, "MaxParticlesAllowed", NULL, "Particle limit", { "500", "700", "-1" }, NULL,
      MOHA_CHOICES(moha_particles), GAME_SETTING_TEXT },
    MOHA_TEXTURE( "Character", "Character textures" ),
    MOHA_TEXTURE( "CharacterNormalMap", "Character normal maps" ),
    MOHA_TEXTURE( "Effects", "Effects textures" ),
    MOHA_TEXTURE( "LightAndShadowMap", "Light and shadow maps" ),
    MOHA_TEXTURE( "Skybox", "Sky textures" ),
    MOHA_TEXTURE( "Vehicle", "Vehicle textures" ),
    MOHA_TEXTURE( "VehicleNormalMap", "Vehicle normal maps" ),
    MOHA_TEXTURE( "Weapon", "Weapon textures" ),
    MOHA_TEXTURE( "WeaponNormalMap", "Weapon normal maps" ),
    MOHA_TEXTURE( "World", "World textures" ),
    MOHA_TEXTURE( "WorldNormalMap", "World normal maps" ),
    { "TextureLODSettings", "TEXTUREGROUP_RenderTarget", NULL, "Render target quality",
      { "(MinLODSize=1,MaxLODSize=1024,LODBias=2)", "(MinLODSize=1,MaxLODSize=2048,LODBias=1)",
        "(MinLODSize=1,MaxLODSize=4096,LODBias=0)" }, NULL, MOHA_CHOICES(moha_targets), GAME_SETTING_TEXT }
};
#undef MOHA_TEXTURE
#undef MOHA_NUMBER
#undef MOHA_BOOL
#undef MOHA_SCALABILITY
#undef MOHA_CHOICES

static const char *const moha_presets[] = { "Low", "Medium", "High" };
static const struct game_settings_profile moha_profile = {
    .executable = "MOHA.exe",
    .machine = 0x014c,
    .path = "drive_c/users/steamuser/Documents/EA Games/Medal of Honor Airborne(tm)/Config/MOHAAutorun.ini",
    .presets = moha_presets,
    .preset_count = sizeof(moha_presets) / sizeof(moha_presets[0]),
    .settings = moha_settings,
    .count = sizeof(moha_settings) / sizeof(moha_settings[0]),
    .format = GAME_SETTINGS_INI,
    .initial_text = "[Configuration]\r\nBasedOn=..\\MOHAGame\\Config\\DefaultEngine.ini\r\n",
    /* UE3 regenerates MOHAEngine.ini from this defaults chain when settings change. */
    .arguments = "-NO_LAUNCHER -NOWXWINDOWS -DEFENGINEINI=\"C:\\users\\steamuser\\Documents\\EA Games\\"
                 "Medal of Honor Airborne(tm)\\Config\\MOHAAutorun.ini\""
};

#endif
