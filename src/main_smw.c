/*
 * Super Mario World — Retro-Go SD GWHB homebrew.
 *
 * Port of Core/Src/porting/smw/main_smw.c from game-and-watch-retro-go-sd.
 * Engine: external/smw (HEADLESS). Assets stay on SD as
 * /homebrews/smw_assets.dat (not packed into the GWHB).
 *
 * Entry (run_gwhb_homebrew):
 *   void app_main(uint8_t load_state, uint8_t start_paused, int8_t save_slot)
 */

#include <odroid_system.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>
#include <stdio.h>
#include <stdbool.h>

#include "gw_lcd.h"
#include "gw_malloc.h"
#include "common.h"
#include "rom_manager.h"
#include "appid.h"
#include "odroid_overlay.h"

#include "smw_borders.h"

#include "smw/assets/smw_assets.h"
#include "smw/src/config.h"
#include "smw/src/snes/ppu.h"
#include "smw/src/types.h"
#include "smw/src/smw_rtl.h"
#include "smw/src/common_cpu_infra.h"
#include "smw/src/smw_spc_player.h"

#ifndef HOST_BUILD
#include "gw_core_bridge.h"
#include "gw_core_i18n.h"
#else
#include "host_compat.h"
#include "gw_core_i18n.h"
#endif

/* Firmware OFW probe — remapped to core_get_ofw_is_mario via redefine-syms. */
bool get_ofw_is_mario(void);

#pragma GCC optimize("Ofast")

static int g_ppu_render_flags = kPpuRenderFlags_NewRenderer;

static uint8 g_gamepad_buttons;
static int g_input1_state;

bool g_new_ppu = true;
bool g_other_image = true;
bool g_debug_flag = false;
uint32 g_wanted_features;
struct SpcPlayer *g_spc_player;

static uint32 frameCtr = 0;
static uint32 renderedFrameCtr = 0;

#define SMW_AUDIO_SAMPLE_RATE   (16000)
#if LIMIT_30FPS != 0
#define FRAMERATE 30
#else
#define FRAMERATE 60
#endif /* LIMIT_30FPS */
#define SMW_AUDIO_BUFFER_LENGTH 534  /* two frames when limited to 30 fps */

int16_t audiobuffer_smw[SMW_AUDIO_BUFFER_LENGTH];

const uint8 *g_asset_ptrs[kNumberOfAssets];
uint32 g_asset_sizes[kNumberOfAssets];

/* keys inputs (hw & sw) */
static odroid_gamepad_state_t joystick;

/* Button-binding profile (Mario vs Zelda OFW face buttons). */
static uint32_t g_button_profile;

void NORETURN Die(const char *error) {
  printf("Error: %s\n", error);
  abort();
}

static void LoadAssetsChunk(size_t length, const uint8* data) {
  uint32 offset = 88 + kNumberOfAssets * 4 + *(uint32 *)(data + 84);
  for (size_t i = 0; i < kNumberOfAssets; i++) {
    uint32 size = *(uint32 *)(data + 88 + i * 4);
    offset = (offset + 3) & ~3;
    if ((uint64)offset + size > length)
      assert(!"Assets file corruption");
    g_asset_sizes[i] = size;
    g_asset_ptrs[i] = data + offset;
    offset += size;
  }
}

static bool VerifyAssetsFile(const uint8 *data, size_t length) {
  static const char kAssetsSig[] = { kAssets_Sig };
  if (length < 16 + 32 + 32 + 8 + kNumberOfAssets * 4 ||
    memcmp(data, kAssetsSig, 48) != 0 ||
    *(uint32 *)(data + 80) != kNumberOfAssets)
    return false;
  return true;
}

static void LoadAssets(void) {
  const uint8_t *smw_assets;
  uint32_t smw_assets_length = 0;
  smw_assets = (const uint8_t *)odroid_overlay_cache_file_in_flash(
      "/homebrews/smw_assets.dat", &smw_assets_length, false);

  if (smw_assets == NULL)
    Die("Missing /homebrews/smw_assets.dat file");

  if (!VerifyAssetsFile(smw_assets, smw_assets_length))
    Die("Mismatching /homebrews/smw_assets.dat file");

  LoadAssetsChunk(smw_assets_length, smw_assets);

  for (size_t i = 0; i < kNumberOfAssets; i++) {
    if (g_asset_ptrs[i] == 0) {
      assert(!"Missing asset");
    }
  }
}

MemBlk FindInAssetArray(int asset, int idx) {
  return FindIndexInMemblk((MemBlk) { g_asset_ptrs[asset], g_asset_sizes[asset] }, idx);
}

const uint8 *FindPtrInAsset(int asset, uint32 addr) {
  return FindAddrInMemblk((MemBlk){g_asset_ptrs[asset], g_asset_sizes[asset]}, addr);
}

void RtlDrawPpuFrame(uint8 *pixel_buffer, size_t pitch, uint32 render_flags) {
  (void)pixel_buffer;
  (void)pitch;
  (void)render_flags;
  g_rtl_game_info->draw_ppu_frame();
}

#define OVERLAY_COLOR_565 0xFFFF
#define BORDER_COLOR_565 0x1082  /* Dark Dark Gray */

#define BORDER_HEIGHT 240
#define BORDER_WIDTH 32

#define BORDER_Y_OFFSET (((GW_LCD_HEIGHT) - (BORDER_HEIGHT)) / 2)

void draw_border_smw(pixel_t * fb){
    uint32_t start, bit_index;
    start = 0;
    bit_index = 0;
    for(uint16_t i=0; i < BORDER_HEIGHT; i++){
        uint32_t offset = start + i * GW_LCD_WIDTH;
        for(uint8_t j=0; j < BORDER_WIDTH; j++){
            fb[offset + j] =
                (IMG_BORDER_LEFT_SMW[bit_index >> 3] << (bit_index & 0x07)) & 0x80 ? BORDER_COLOR_565 : 0x0000;
            bit_index++;
        }
    }
    start = 32 + 256;
    bit_index = 0;
    for(uint16_t i=0; i < BORDER_HEIGHT; i++){
        uint32_t offset = start + i * GW_LCD_WIDTH;
        for(uint8_t j=0; j < BORDER_WIDTH; j++){
            fb[offset + j] =
                (IMG_BORDER_RIGHT_SMW[bit_index >> 3] << (bit_index & 0x07)) & 0x80 ? BORDER_COLOR_565 : 0x0000;
            bit_index++;
        }
    }
}

static void DrawPpuFrame(uint16_t* framebuffer) {
  wdog_refresh();
  uint8 *pixel_buffer = (uint8 *)(framebuffer + 320*8 + 32);    /* 8 rows from top, 32 px left */
  int pitch = 320 * 2;

  PpuBeginDrawing(g_my_ppu, pixel_buffer, pitch, g_ppu_render_flags);
  RtlDrawPpuFrame(pixel_buffer, pitch, g_ppu_render_flags);

  draw_border_smw(framebuffer);
}

void RtlApuLock(void) {
}

void RtlApuUnlock(void) {
}

static void HandleCommand(uint32 j, bool pressed) {
  if (j <= kKeys_Controls_Last) {
    static const uint8 kKbdRemap[] = { 0, 4, 5, 6, 7, 2, 3, 8, 0, 9, 1, 10, 11 };
    if (pressed)
      g_input1_state |= 1 << kKbdRemap[j];
    else
      g_input1_state &= ~(1 << kKbdRemap[j]);
    return;
  }
}

static FILE *savestate_file;
static char savestate_path[255];

void writeSaveStateInitImpl(void) {
  savestate_file = fopen(savestate_path, "wb");
}

void writeSaveStateImpl(uint8_t* data, size_t size) {
  if (savestate_file)
    fwrite(data, 1, size, savestate_file);
}

void writeSaveStateFinalizeImpl(void) {
  if (savestate_file) {
    fclose(savestate_file);
    savestate_file = NULL;
  }
}

void readSaveStateInitImpl(void) {
  savestate_file = fopen(savestate_path, "rb");
}
void readSaveStateImpl(uint8_t* data, size_t size) {
 if (savestate_file != NULL) {
    wdog_refresh();
    fread(data, 1, size, savestate_file);
  } else {
    memset(data, 0, size);
  }
}
void readSaveStateFinalizeImpl(void) {
  if (savestate_file) {
    fclose(savestate_file);
    savestate_file = NULL;
  }
}

static bool smw_system_SaveState(char *savePathName) {
  printf("Saving state...\n");
  odroid_audio_mute(true);

  strcpy(savestate_path, savePathName);
  RtlSaveLoad(kSaveLoad_Save, 0);

  odroid_audio_mute(false);
  return true;
}

static bool smw_system_LoadState(char *savePathName) {
  odroid_audio_mute(true);

  if (savePathName) {
    strcpy(savestate_path, savePathName);
    RtlSaveLoad(kSaveLoad_Load, 0);
  }

  odroid_audio_mute(false);
  return true;
}

static void *Screenshot(void)
{
    lcd_wait_for_vblank();

    lcd_clear_active_buffer();
    DrawPpuFrame(lcd_get_active_buffer());
    return lcd_get_active_buffer();
}

static void smw_system_SramSave(void)
{
  char *sram_path = odroid_system_get_path(ODROID_PATH_SAVE_SRAM, ACTIVE_FILE->path);
  FILE *file = fopen(sram_path, "wb");
  if (file != NULL) {
    fwrite(RtlGetSram(), 1, 2048, file);
    fclose(file);
  }
  free(sram_path);
}

static void smw_sound_start(void)
{
  memset(audiobuffer_smw, 0, sizeof(audiobuffer_smw));
  audio_start_playing(SMW_AUDIO_BUFFER_LENGTH);
}

static void smw_sound_submit(void) {
  if (common_emu_sound_loop_is_muted()) {
    return;
  }

  int16_t factor = common_emu_sound_get_volume();
  int16_t* sound_buffer = audio_get_active_buffer();
  uint16_t sound_buffer_length = audio_get_buffer_length();

  for (int i = 0; i < sound_buffer_length; i++) {
    int32_t sample = audiobuffer_smw[i];
    sound_buffer[i] = (sample * factor) >> 8;
  }
}

static const gw_i18n_entry_t i18n_reset[] = {
    { "en", "Reset" },
    { "fr", "Réinitialiser" },
    { "es", "Reiniciar" },
    { "de", "Zurücksetzen" },
    GW_I18N_END
};

static bool reset_cb(odroid_dialog_choice_t *option, odroid_dialog_event_t event, uint32_t repeat)
{
  (void)option;
  (void)repeat;
  if (event == ODROID_DIALOG_ENTER) {
    printf("Resetting\n");
    RtlReset(1);
  }
  return event == ODROID_DIALOG_ENTER;
}

/* A physical button (or combination) is assigned for each SNES controller button.
 * Each nibble is one assignment (MSB→LSB: Select,Start,A,B,X,Y,L,R).
 * Values: GAME modifier (MSB) + 3-bit physical index (0=A,1=B,2=Select,3=Start,4=Time).
 * 0xf = no assignment. */
#define SMW_BINDINGS_ZELDA_A 0x4c012398
#define SMW_BINDINGS_ZELDA_B 0x43201198
#define SMW_BINDINGS_MARIO_A 0x8c0144ff
#define SMW_BINDINGS_MARIO_B 0x4c8011ff

static bool button_pressed(uint8_t button_index) {
  switch (button_index) {
    case 0x0: return joystick.values[ODROID_INPUT_A];
    case 0x1: return joystick.values[ODROID_INPUT_B];
    case 0x2: return joystick.values[ODROID_INPUT_Y];
    case 0x3: return joystick.values[ODROID_INPUT_X];
    case 0x4: return joystick.values[ODROID_INPUT_SELECT];
    default:  return false;
  }
}

static bool get_profile_button(uint8_t command) {
  uint8_t shift = (7 - (command - 5)) * 4;
  uint8_t binding = (g_button_profile >> shift) & 0xf;
  bool game_modifier = (binding & 0x8) == 0x8;
  bool buttonPressed = button_pressed(binding & 0x7);
  bool isPauseModifierPressed = joystick.values[ODROID_INPUT_VOLUME];
  bool isGameModifierPressed = joystick.values[ODROID_INPUT_START];
  return !isPauseModifierPressed && (game_modifier == isGameModifierPressed) && buttonPressed;
}

static void smw_repaint(void)
{
  uint16_t *screen = lcd_get_active_buffer();
  DrawPpuFrame(screen);
  common_ingame_overlay();
}

/* Main */
void app_main(uint8_t load_state, uint8_t start_paused, int8_t save_slot)
{
  /* Launcher already seeds ram_start past code+bss; keep bump past our BSS. */
  ram_start = (uint32_t)(uintptr_t)&__CORE_BSS_END__;

  printf("SMW start\n");
  odroid_system_init(APPID_HOMEBREW, SMW_AUDIO_SAMPLE_RATE);
  odroid_system_emu_init(&smw_system_LoadState, &smw_system_SaveState, &Screenshot,
                         NULL, NULL, &smw_system_SramSave, NULL);

  if (start_paused) {
    common_emu_state.pause_after_frames = 2;
    odroid_audio_mute(true);
  } else {
    common_emu_state.pause_after_frames = 0;
  }
  common_emu_state.frame_time_10us = (uint16_t)(100000 / FRAMERATE + 0.5f);

  LoadAssets();

  SnesInit(NULL, 0);

  g_wanted_features = FEATURES;

  if (!get_ofw_is_mario()) {
    g_button_profile = SMW_BINDINGS_ZELDA_A;
  } else {
    g_button_profile = SMW_BINDINGS_MARIO_B;
  }

  g_spc_player = SmwSpcPlayer_Create();
  g_spc_player->initialize(g_spc_player);

  if (load_state) {
    odroid_system_emu_load_state(save_slot);
  } else {
    lcd_clear_buffers();
  }

  {
    char *sram_path = odroid_system_get_path(ODROID_PATH_SAVE_SRAM, ACTIVE_FILE->path);
    FILE *file = fopen(sram_path, "rb");
    if (file != NULL) {
      fread(RtlGetSram(), 1, 2048, file);
      fclose(file);
    }
    free(sram_path);
  }

  lcd_wait_for_vblank();
  smw_sound_start();

  printf("Entering loop\n");
  while (true) {
    wdog_refresh();

    odroid_input_read_gamepad(&joystick);

    odroid_dialog_choice_t options[] = {
            {300, gw_i18n(i18n_reset), NULL, 1, &reset_cb},
            ODROID_DIALOG_CHOICE_LAST
    };
    common_emu_input_loop(&joystick, options, &smw_repaint);

    bool isPauseModifierPressed = joystick.values[ODROID_INPUT_VOLUME];

    HandleCommand(1, !isPauseModifierPressed && joystick.values[ODROID_INPUT_UP]);
    HandleCommand(2, !isPauseModifierPressed && joystick.values[ODROID_INPUT_DOWN]);
    HandleCommand(3, !isPauseModifierPressed && joystick.values[ODROID_INPUT_LEFT]);
    HandleCommand(4, !isPauseModifierPressed && joystick.values[ODROID_INPUT_RIGHT]);

    HandleCommand(5, get_profile_button(5));   /* SNES Select */
    HandleCommand(6, get_profile_button(6));   /* SNES Start */
    HandleCommand(7, get_profile_button(7));   /* SNES A */
    HandleCommand(8, get_profile_button(8));   /* SNES B */
    HandleCommand(9, get_profile_button(9));   /* SNES X */
    HandleCommand(10, get_profile_button(10)); /* SNES Y */
    HandleCommand(11, get_profile_button(11)); /* SNES L */
    HandleCommand(12, get_profile_button(12)); /* SNES R */

    /* Clear gamepad inputs when joypad directional inputs to avoid wonkiness */
    int inputs = g_input1_state;
    if (g_input1_state & 0xf0)
      g_gamepad_buttons = 0;
    inputs |= g_gamepad_buttons;

    bool drawFrame = common_emu_frame_loop();

    RtlRunFrame(inputs);

    frameCtr++;

#if LIMIT_30FPS != 0
    RtlRenderAudio(audiobuffer_smw, SMW_AUDIO_BUFFER_LENGTH / 2, 1);
    RtlRunFrame(inputs);
    RtlRenderAudio(audiobuffer_smw + (SMW_AUDIO_BUFFER_LENGTH / 2), SMW_AUDIO_BUFFER_LENGTH / 2, 1);
#else
    RtlRenderAudio(audiobuffer_smw, SMW_AUDIO_BUFFER_LENGTH, 1);
#endif /* LIMIT_30FPS */

    if (drawFrame) {
      smw_sound_submit();

      uint16_t *screen = lcd_get_active_buffer();
      DrawPpuFrame(screen);
      common_ingame_overlay();
      lcd_swap();

      renderedFrameCtr++;
    }

    common_emu_sound_sync(false);
  }
}
