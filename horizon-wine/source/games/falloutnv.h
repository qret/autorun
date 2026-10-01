#ifndef AUTORUN_GAME_FALLOUTNV_H
#define AUTORUN_GAME_FALLOUTNV_H

static const struct game_setting_choice falloutnv_toggle[] = { { "Disabled", "0" }, { "Enabled", "1" } };
static const struct game_setting_choice falloutnv_resolution[] = {
    { "960 x 540", "960x540" }, { "1024 x 576", "1024x576" }, { "1280 x 720", "1280x720" },
    { "1600 x 900", "1600x900" }, { "1920 x 1080", "1920x1080" }
};
static const struct game_setting_choice falloutnv_aa[] = {
    { "Off", "1" }, { "2x", "2" }, { "4x", "4" }, { "8x", "8" }
};
static const struct game_setting_choice falloutnv_anisotropy[] = {
    { "Off", "1" }, { "2x", "2" }, { "4x", "4" }, { "8x", "8" }, { "15x", "15" }, { "16x", "16" }
};
static const struct game_setting_choice falloutnv_textures[] = {
    { "Low", "2" }, { "Medium", "1" }, { "High", "0" }
};
static const struct game_setting_choice falloutnv_shadows[] = {
    { "256", "256" }, { "512", "512" }, { "1024", "1024" }, { "2048", "2048" }
};
static const struct game_setting_choice falloutnv_quality[] = {
    { "Low", "0" }, { "Medium", "1" }, { "High", "2" }
};

#define FNV_CHOICES(array) array, sizeof(array) / sizeof(array[0])
static const struct game_setting falloutnv_settings[] = {
    { "Display", "iSize W", "iSize H", "Resolution", { NULL }, "1280x720",
      FNV_CHOICES(falloutnv_resolution), GAME_SETTING_RESOLUTION },
    { "Display", "bFull Screen", NULL, "Fullscreen", { NULL }, "0",
      FNV_CHOICES(falloutnv_toggle), GAME_SETTING_INTEGER },
    { "Display", "iPresentInterval", NULL, "VSync", { NULL }, "1",
      FNV_CHOICES(falloutnv_toggle), GAME_SETTING_INTEGER },
    { "Display", "iMultiSample", NULL, "Anti-aliasing", { "1", "1", "4", "4" }, NULL,
      FNV_CHOICES(falloutnv_aa), GAME_SETTING_INTEGER },
    { "Display", "iWaterMultisamples", NULL, "Water anti-aliasing", { "1", "1", "4", "4" }, NULL,
      FNV_CHOICES(falloutnv_aa), GAME_SETTING_INTEGER },
    { "Display", "bTransparencyMultisampling", NULL, "Transparency multisampling", { "0", "0", "1", "1" }, NULL,
      FNV_CHOICES(falloutnv_toggle), GAME_SETTING_INTEGER },
    { "Display", "iMaxAnisotropy", NULL, "Anisotropic filtering", { "1", "1", "8", "15" }, NULL,
      FNV_CHOICES(falloutnv_anisotropy), GAME_SETTING_INTEGER },
    { "Display", "iTexMipMapSkip", NULL, "Texture quality", { "1", "0", "0", "0" }, NULL,
      FNV_CHOICES(falloutnv_textures), GAME_SETTING_INTEGER },
    { "Display", "iTexMipMapMinimum", NULL, "Minimum texture mip level", { "4", "4", "4", "4" }, NULL,
      NULL, 0, GAME_SETTING_INTEGER },
    { "Display", "iMaxDecalsPerFrame", NULL, "Decals per frame", { "2", "5", "10", "10" }, NULL,
      NULL, 0, GAME_SETTING_INTEGER },
    { "Display", "fDecalLOD1", NULL, "Decal distance 1", { "500", "500", "500", "500" }, NULL,
      NULL, 0, GAME_SETTING_NUMBER },
    { "Display", "fDecalLOD2", NULL, "Decal distance 2", { "1000", "1000", "1000", "1000" }, NULL,
      NULL, 0, GAME_SETTING_NUMBER },
    { "Display", "bDrawShadows", NULL, "Shadows", { "0", "1", "1", "1" }, NULL,
      FNV_CHOICES(falloutnv_toggle), GAME_SETTING_INTEGER },
    { "Display", "iActorShadowCountInt", NULL, "Interior actor shadows", { "2", "4", "4", "6" }, NULL,
      NULL, 0, GAME_SETTING_INTEGER },
    { "Display", "iActorShadowCountExt", NULL, "Exterior actor shadows", { "2", "2", "2", "6" }, NULL,
      NULL, 0, GAME_SETTING_INTEGER },
    { "Display", "iShadowFilter", NULL, "Shadow filtering", { "0", "1", "2", "2" }, NULL,
      FNV_CHOICES(falloutnv_quality), GAME_SETTING_INTEGER },
    { "Display", "iShadowMapResolution", NULL, "Shadow resolution", { "256", "256", "512", "1024" }, NULL,
      FNV_CHOICES(falloutnv_shadows), GAME_SETTING_INTEGER },
    { "Display", "fSpecularLODStartFade", NULL, "Specular fade distance", { "200", "500", "1000", "2000" }, NULL,
      NULL, 0, GAME_SETTING_NUMBER },
    { "Display", "fShadowLODStartFade", NULL, "Shadow fade distance", { "100", "200", "460", "1000" }, NULL,
      NULL, 0, GAME_SETTING_NUMBER },
    { "Display", "fLightLODStartFade", NULL, "Light fade distance", { "200", "1000", "2500", "3500" }, NULL,
      NULL, 0, GAME_SETTING_NUMBER },
    { "LOD", "fLODFadeOutMultActors", NULL, "Actor fade multiplier", { "2.5", "2.5", "6", "15" }, NULL,
      NULL, 0, GAME_SETTING_NUMBER },
    { "LOD", "fLODFadeOutMultObjects", NULL, "Object fade multiplier", { "2.5", "2.5", "7.5", "15" }, NULL,
      NULL, 0, GAME_SETTING_NUMBER },
    { "LOD", "fLODFadeOutMultItems", NULL, "Item fade multiplier", { "3", "3.5", "4.5", "15" }, NULL,
      NULL, 0, GAME_SETTING_NUMBER },
    { "TerrainManager", "fTreeLoadDistance", NULL, "Tree distance", { "0", "25000", "26000", "40000" }, NULL,
      NULL, 0, GAME_SETTING_NUMBER },
    { "TerrainManager", "fBlockLoadDistanceLow", NULL, "Terrain distance", { "25000", "50000", "50000", "50000" }, NULL,
      NULL, 0, GAME_SETTING_NUMBER },
    { "TerrainManager", "fBlockLoadDistance", NULL, "Distant terrain", { "125000", "125000", "125000", "125000" }, NULL,
      NULL, 0, GAME_SETTING_NUMBER },
    { "TerrainManager", "fSplitDistanceMult", NULL, "Terrain split multiplier", { "0.75", "1", "1", "1.5" }, NULL,
      NULL, 0, GAME_SETTING_NUMBER },
    { "TerrainManager", "fHighBlockLoadDistanceLow", NULL, "Detailed terrain distance", { "20000", "20000", "20000", "20000" }, NULL,
      NULL, 0, GAME_SETTING_NUMBER },
    { "Grass", "b30GrassVS", NULL, "High-quality grass", { "0", "0", "1", "1" }, NULL,
      FNV_CHOICES(falloutnv_toggle), GAME_SETTING_INTEGER },
    { "Grass", "fGrassStartFadeDistance", NULL, "Grass fade distance", { "0", "3000", "5000", "7000" }, NULL,
      NULL, 0, GAME_SETTING_NUMBER },
    { "Imagespace", "bDoDepthOfField", NULL, "Depth of field", { "0", "0", "1", "1" }, NULL,
      FNV_CHOICES(falloutnv_toggle), GAME_SETTING_INTEGER },
    { "Imagespace", "iRadialBlurLevel", NULL, "Radial blur quality", { "0", "0", "1", "2" }, NULL,
      FNV_CHOICES(falloutnv_quality), GAME_SETTING_INTEGER },
    { "Water", "bUseWaterRefractions", NULL, "Water refractions", { "0", "1", "1", "1" }, NULL,
      FNV_CHOICES(falloutnv_toggle), GAME_SETTING_INTEGER },
    { "Water", "bUseWaterReflections", NULL, "Water reflections", { "0", "1", "1", "1" }, NULL,
      FNV_CHOICES(falloutnv_toggle), GAME_SETTING_INTEGER },
    { "Water", "bUseWaterReflectionBlur", NULL, "Water reflection blur", { "0", "0", "1", "1" }, NULL,
      FNV_CHOICES(falloutnv_toggle), GAME_SETTING_INTEGER },
    { "Water", "bUseWaterDisplacements", NULL, "Water displacements", { "0", "1", "1", "1" }, NULL,
      FNV_CHOICES(falloutnv_toggle), GAME_SETTING_INTEGER },
    { "Water", "bUseWaterDepth", NULL, "Water depth", { "0", "1", "1", "1" }, NULL,
      FNV_CHOICES(falloutnv_toggle), GAME_SETTING_INTEGER },
    { "Water", "iWaterReflectWidth", NULL, "Water reflection width", { "256", "512", "512", "1024" }, NULL,
      FNV_CHOICES(falloutnv_shadows), GAME_SETTING_INTEGER },
    { "Water", "iWaterReflectHeight", NULL, "Water reflection height", { "256", "512", "512", "1024" }, NULL,
      FNV_CHOICES(falloutnv_shadows), GAME_SETTING_INTEGER },
    { "Water", "bReflectExplosions", NULL, "Reflect explosions", { "1", "1", "1", "1" }, NULL,
      FNV_CHOICES(falloutnv_toggle), GAME_SETTING_INTEGER },
    { "Water", "iWaterBlurAmount", NULL, "Water blur amount", { "0", "0", "2", "4" }, NULL,
      NULL, 0, GAME_SETTING_INTEGER },
    { "Water", "bAutoWaterSilhouetteReflections", NULL, "Water silhouette reflections", { "1", "1", "1", "0" }, NULL,
      FNV_CHOICES(falloutnv_toggle), GAME_SETTING_INTEGER },
    { "Water", "bForceHighDetailReflections", NULL, "High-detail reflections", { "0", "0", "0", "1" }, NULL,
      FNV_CHOICES(falloutnv_toggle), GAME_SETTING_INTEGER },
    { "BlurShaderHDR", "bDoHighDynamicRange", NULL, "HDR", { "0", "1", "1", "1" }, NULL,
      FNV_CHOICES(falloutnv_toggle), GAME_SETTING_INTEGER },
};
#undef FNV_CHOICES

static const char *const falloutnv_presets[] = { "Low", "Medium", "High", "Ultra" };
static const struct game_settings_profile falloutnv_profile = {
    .executable = "FalloutNV.exe",
    .machine = 0x014c,
    .path = "drive_c/users/steamuser/Documents/My Games/FalloutNV/FalloutPrefs.ini",
    .presets = falloutnv_presets,
    .preset_count = sizeof(falloutnv_presets) / sizeof(falloutnv_presets[0]),
    .settings = falloutnv_settings,
    .count = sizeof(falloutnv_settings) / sizeof(falloutnv_settings[0]),
    .format = GAME_SETTINGS_INI,
    .initial_text = "[Display]\r\niAdapter=0\r\n"
};

#endif
