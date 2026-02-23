#include <string.h>
#include <vmupro_sdk.h>
#include <nofrendo.h>

extern char* itoa(int value, char* buffer, int base);
enum class EmulatorMenuState { EMULATOR_RUNNING, EMULATOR_MENU, EMULATOR_CONTEXT_MENU, EMULATOR_SETTINGS_MENU };

typedef enum ContextMenuEntryType {
  MENU_ACTION,
  MENU_OPTION_VOLUME,
  MENU_OPTION_BRIGHTNESS,
  MENU_OPTION_PALETTE,
  MENU_OPTION_SCALING,
  MENU_OPTION_STATE_SLOT,
  MENU_OPTION_BUTTON_SWAP,
  MENU_OPTION_NONE
};

typedef struct ContextMenuEntry_s {
  const char* title;
  bool enabled              = true;
  ContextMenuEntryType type = MENU_ACTION;
} ContextMenuEntry;

const ContextMenuEntry emuContextEntries[5] = {
    {.title = "Save & Continue", .enabled = true, .type = MENU_ACTION},
    {.title = "Load Game", .enabled = true, .type = MENU_ACTION},
    {.title = "Restart", .enabled = true, .type = MENU_ACTION},
    {.title = "Options", .enabled = true, .type = MENU_ACTION},
    {.title = "Quit", .enabled = true, .type = MENU_ACTION},
};

const ContextMenuEntry emuContextOptionEntries[5] = {
    {.title = "Volume", .enabled = true, .type = MENU_OPTION_VOLUME},
    {.title = "Brightness", .enabled = true, .type = MENU_OPTION_BRIGHTNESS},
    {.title = "Palette", .enabled = true, .type = MENU_OPTION_PALETTE},
    {.title = "State Slot", .enabled = true, .type = MENU_OPTION_STATE_SLOT},
    {.title = "Swap A+B", .enabled = true, .type = MENU_OPTION_BUTTON_SWAP},
};

static const char* PaletteNames[] = {"Nofrendo", "Composite", "Classic", "NTSC", "PVM", "Smooth"};

static const char* kLogNESEmu = "[VMU-PRO NES]";
static bool emuRunning        = true;
static bool appExitFlag       = false;
static bool inOptionsMenu     = false;
static bool swapButtons       = false;

static int frame_counter            = 0;
static int renderFrame              = 0;
static uint32_t num_frames          = 0;
static int nesContextSelectionIndex = 0;
static int nesCurrentPaletteIndex   = 0;

static uint64_t frame_time       = 0.0f;
static uint64_t frame_time_total = 0.0f;
static uint64_t frame_time_max   = 0.0f;
static uint64_t frame_time_min   = 0.0f;
static float frame_time_avg      = 0.0f;
static char* launchfile          = nullptr;
static char* filename            = nullptr;
static nes_t* nes;
static uint16_t* palette        = nullptr;
static uint8_t* nes_back_buffer = nullptr;
static uint8_t* pauseBuffer     = nullptr;

static EmulatorMenuState currentEmulatorState = EmulatorMenuState::EMULATOR_RUNNING;

static void update_frame_time(uint64_t ftime) {
  num_frames++;
  frame_time       = ftime;
  frame_time_total = frame_time_total + frame_time;
}

static void reset_frame_time() {
  num_frames       = 0;
  frame_time       = 0;
  frame_time_total = 0;
  frame_time_max   = 0;
  frame_time_min   = 1000000;  // some large number
  frame_time_avg   = 0.0f;
}

static float get_fps() {
  if (frame_time_total == 0) {
    return 0.0f;
  }
  return num_frames / (frame_time_total / 1e6f);
}

static bool savaStateHandler(const char* filename) {
  // Save to a file named after the game + state (.ggstate)
  char filepath[512];
  strcpy(filepath, "/sdcard/roms/NES/STATES/");
  strcat(filepath, filename);
  strcat(filepath, "state");
  return state_save(filepath) == 0;
}

static bool loadStateHandler(const char* filename) {
  char filepath[512];
  strcpy(filepath, "/sdcard/roms/NES/STATES/");
  strcat(filepath, filename);
  strcat(filepath, "state");

  if (state_load(filepath) != 0) {
    nes_reset(true);
    return false;
  }

  return true;
}

static void buildPalette(nespal_t palIdx) {  // eventually pass a param to choose the palette
  // allocate the palette
  if (!palette) {
    palette = (uint16_t*)malloc(256 * sizeof(uint16_t));
  }
  else {
    memset(palette, 0x00, 256 * sizeof(uint16_t));
  }

  uint16_t* pal = (uint16_t*)nofrendo_buildpalette(palIdx, 16);
  for (int i = 0; i < 256; ++i) {
    uint16_t color = (pal[i] >> 8) | (pal[i] << 8);
    palette[i]     = color;
  }
  nes->builtPalette = palette;
  free(pal);
}

void Tick() {
  static constexpr uint64_t max_frame_time = 1000000 / 60.0f;  // microseconds per frame
  vmupro_display_clear(VMUPRO_COLOR_BLACK);
  vmupro_display_refresh();

  int64_t next_frame_time = vmupro_get_time_us();
  int64_t lastTime        = next_frame_time;
  int64_t accumulated_us  = 0;

  while (emuRunning) {
    int64_t currentTime = vmupro_get_time_us();
    float fps_now       = get_fps();

    vmupro_btn_read();

    if (currentEmulatorState == EmulatorMenuState::EMULATOR_MENU) {
      vmupro_display_clear(VMUPRO_COLOR_BLACK);
      // vmupro_blit_buffer_dithered(pauseBuffer, 0, 0, 240, 240, 2);
      vmupro_blit_buffer_blended(pauseBuffer, 0, 0, 240, 240, 150);
      // We'll only use our front buffer for this to simplify things
      int startY = 50;
      vmupro_draw_fill_rect(40, 37, 200, 170, VMUPRO_COLOR_NAVY);

      vmupro_set_font(VMUPRO_FONT_OPEN_SANS_15x18);
      for (int x = 0; x < 5; ++x) {
        uint16_t fgColor = nesContextSelectionIndex == x ? VMUPRO_COLOR_NAVY : VMUPRO_COLOR_WHITE;
        uint16_t bgColor = nesContextSelectionIndex == x ? VMUPRO_COLOR_WHITE : VMUPRO_COLOR_NAVY;
        if (nesContextSelectionIndex == x) {
          vmupro_draw_fill_rect(50, startY + (x * 22), 190, (startY + (x * 22) + 20), VMUPRO_COLOR_WHITE);
        }
        if (inOptionsMenu) {
          if (!emuContextOptionEntries[x].enabled) fgColor = VMUPRO_COLOR_GREY;
          vmupro_draw_text(emuContextOptionEntries[x].title, 60, startY + (x * 22), fgColor, bgColor);
          // For options, we need to draw the current option value right aligned within the selection boundary
          switch (emuContextOptionEntries[x].type) {
            case MENU_OPTION_BRIGHTNESS: {
              char currentBrightness[5] = "0%";
              itoa((vmupro_get_global_brightness() * 10) + 10, currentBrightness, 10);
              strcat(currentBrightness, "%");
              int tlen = vmupro_calc_text_length(currentBrightness);
              vmupro_draw_text(currentBrightness, 190 - tlen - 5, startY + (x * 22), fgColor, bgColor);
            } break;
            case MENU_OPTION_VOLUME: {
              char currentVolume[5] = "0%";
              itoa((vmupro_get_global_volume() * 10) + 10, currentVolume, 10);
              strcat(currentVolume, "%");
              int tlen = vmupro_calc_text_length(currentVolume);
              vmupro_draw_text(currentVolume, 190 - tlen - 5, startY + (x * 22), fgColor, bgColor);
            } break;
            case MENU_OPTION_PALETTE: {
              int tlen = vmupro_calc_text_length(PaletteNames[nesCurrentPaletteIndex]);
              vmupro_draw_text(
                  PaletteNames[nesCurrentPaletteIndex], 190 - tlen - 5, startY + (x * 22), fgColor, bgColor
              );
            } break;
            case MENU_OPTION_BUTTON_SWAP: {
              char swapTextState[5] = "Yes";
              strcpy(swapTextState, swapButtons ? "Yes" : "No");
              int tlen = vmupro_calc_text_length(swapTextState);
              vmupro_draw_text(swapTextState, 190 - tlen - 5, startY + (x * 22), fgColor, bgColor);
            } break;
            default:
              break;
          }
        }
        else {
          if (!emuContextEntries[x].enabled) fgColor = VMUPRO_COLOR_GREY;
          vmupro_draw_text(emuContextEntries[x].title, 60, startY + (x * 22), fgColor, bgColor);
        }
      }
      vmupro_display_refresh();

      if (vmupro_btn_pressed(Btn_B)) {
        // Exit the context menu, resume execution
        // Reset our frame counters otherwise the emulation
        // will run at full blast :)
        if (inOptionsMenu) {
          inOptionsMenu = false;
        }
        else {
          vmupro_resume_double_buffer_renderer();
          reset_frame_time();
          lastTime             = vmupro_get_time_us();
          accumulated_us       = 0;
          currentEmulatorState = EmulatorMenuState::EMULATOR_RUNNING;
        }
      }
      else if (vmupro_btn_pressed(Btn_A) && !inOptionsMenu) {
        // Get the selection index. What are we supposed to do?
        if (nesContextSelectionIndex == 0) {
          vmupro_resume_double_buffer_renderer();
          // Save in both cases
          savaStateHandler((const char*)filename);

          // Close the modal
          reset_frame_time();
          lastTime             = vmupro_get_time_us();
          accumulated_us       = 0;
          currentEmulatorState = EmulatorMenuState::EMULATOR_RUNNING;
        }
        else if (nesContextSelectionIndex == 1) {
          vmupro_resume_double_buffer_renderer();
          loadStateHandler((const char*)filename);

          // Close the modal
          reset_frame_time();
          lastTime             = vmupro_get_time_us();
          accumulated_us       = 0;
          currentEmulatorState = EmulatorMenuState::EMULATOR_RUNNING;
        }
        else if (nesContextSelectionIndex == 2) {  // Reset
          vmupro_resume_double_buffer_renderer();
          reset_frame_time();
          lastTime       = vmupro_get_time_us();
          accumulated_us = 0;
          nes_reset(true);
          currentEmulatorState = EmulatorMenuState::EMULATOR_RUNNING;
        }
        else if (nesContextSelectionIndex == 3) {  // Options
          // let's change a flag, let the rendered in the next iteration re-render the menu
          inOptionsMenu = true;
        }
        else if (nesContextSelectionIndex == 4) {  // Quit
          appExitFlag = true;
          emuRunning  = false;
          nes_shutdown();
        }
      }
      else if (vmupro_btn_pressed(DPad_Down)) {
        if (nesContextSelectionIndex == 4)
          nesContextSelectionIndex = 0;
        else
          nesContextSelectionIndex++;
      }
      else if (vmupro_btn_pressed(DPad_Up)) {
        if (nesContextSelectionIndex == 0)
          nesContextSelectionIndex = 4;
        else
          nesContextSelectionIndex--;
      }
      else if ((vmupro_btn_pressed(DPad_Right) || vmupro_btn_pressed(DPad_Left)) && inOptionsMenu) {
        // Adjust the setting
        switch (emuContextOptionEntries[nesContextSelectionIndex].type) {
          case MENU_OPTION_BRIGHTNESS: {
            uint8_t currentBrightness = vmupro_get_global_brightness();
            uint8_t newBrightness     = 1;
            if (vmupro_btn_pressed(DPad_Right)) {
              newBrightness = currentBrightness < 10 ? currentBrightness + 1 : 10;
            }
            else {
              newBrightness = currentBrightness > 0 ? currentBrightness - 1 : 0;
            }
            // settings->brightness = newBrightness;
            vmupro_set_global_brightness(newBrightness);
            // OLEDDisplay::getInstance()->setBrightness(newBrightness, true, false);
          } break;
          case MENU_OPTION_VOLUME: {
            uint8_t currentVolume = vmupro_get_global_volume();
            uint8_t newVolume     = 1;
            if (vmupro_btn_pressed(DPad_Right)) {
              newVolume = currentVolume < 10 ? currentVolume + 1 : 10;
            }
            else {
              newVolume = currentVolume > 0 ? currentVolume - 1 : 0;
            }
            vmupro_set_global_volume(newVolume);
          } break;
          case MENU_OPTION_PALETTE: {
            if (vmupro_btn_pressed(DPad_Right)) {
              nesCurrentPaletteIndex =
                  nesCurrentPaletteIndex < NES_PALETTE_COUNT - 1 ? nesCurrentPaletteIndex + 1 : NES_PALETTE_COUNT - 1;
            }
            else {
              nesCurrentPaletteIndex = nesCurrentPaletteIndex > 0 ? nesCurrentPaletteIndex - 1 : 0;
            }
            buildPalette((nespal_t)nesCurrentPaletteIndex);
          } break;
          case MENU_OPTION_BUTTON_SWAP:
            swapButtons = !swapButtons;
            break;
          default:
            break;
        }
      }
    }
    else {  // Run!

      int pad = 0;
      pad |= vmupro_btn_held(DPad_Up) ? NES_PAD_UP : 0;
      pad |= vmupro_btn_held(DPad_Right) ? NES_PAD_RIGHT : 0;
      pad |= vmupro_btn_held(DPad_Down) ? NES_PAD_DOWN : 0;
      pad |= vmupro_btn_held(DPad_Left) ? NES_PAD_LEFT : 0;
      pad |= vmupro_btn_held(Btn_Mode) ? NES_PAD_START : 0;
      pad |= vmupro_btn_held(Btn_Power) ? NES_PAD_SELECT : 0;
      pad |= vmupro_btn_held(Btn_A) ? (swapButtons ? NES_PAD_B : NES_PAD_A) : 0;
      pad |= vmupro_btn_held(Btn_B) ? (swapButtons ? NES_PAD_A : NES_PAD_B) : 0;

      input_update(0, pad);

      // The bottom button is for bringing up the menu overlay!
      if (vmupro_btn_pressed(Btn_Bottom)) {
        // Copy the rendered framebuffer to our pause buffer
        vmupro_delay_ms(16);

        // get the last rendered buffer
        if (vmupro_get_last_blitted_fb_side() == 0) {
          memcpy(pauseBuffer, vmupro_get_front_fb(), 115200);
        }
        else {
          memcpy(pauseBuffer, vmupro_get_back_fb(), 115200);
        }
        vmupro_pause_double_buffer_renderer();

        // Set the state to show the overlay
        currentEmulatorState = EmulatorMenuState::EMULATOR_MENU;

        // Delay by one frame to allow the display to finish refreshing
        vmupro_delay_ms(16);
        continue;
      }

      if (renderFrame) {
        nes_back_buffer = vmupro_get_back_buffer();
        nes_setvidbuf(nes_back_buffer);
        nes_emulate(true);
        vmupro_push_double_buffer_frame();
      }
      else {
        nes_emulate(false);
        renderFrame = 1;
      }

      vmupro_audio_add_stream_samples(
          (int16_t*)nes->apu->buffer, nes->apu->samples_per_frame * 2, vmupro_stereo_mode_t::VMUPRO_AUDIO_STEREO, true
      );

      ++frame_counter;

      int64_t elapsed_us = vmupro_get_time_us() - currentTime;
      int64_t sleep_us   = max_frame_time - elapsed_us + accumulated_us;

      vmupro_log(
          VMUPRO_LOG_INFO,
          kLogNESEmu,
          "loop %i, fps: %.2f, elapsed: %lli, sleep: %lli",
          frame_counter,
          fps_now,
          elapsed_us,
          sleep_us
      );

      if (sleep_us > 450) {
        vmupro_delay_us(sleep_us - 450);  // subtract 360 micros for jitter
        accumulated_us = 0;
      }
      else if (sleep_us < 0) {
        renderFrame    = 0;
        accumulated_us = sleep_us;
      }

      update_frame_time(currentTime - lastTime);
      lastTime = currentTime;
    }
  }
}

void Exit() {
  char sramfile[512];
  strcpy(sramfile, "/sdcard/roms/NES/SAVES/");
  strcat(sramfile, filename);
  strcat(sramfile, ".sram");
  rom_savesram(sramfile);

  if (launchfile) {
    free(launchfile);
    launchfile = nullptr;
  }

  if (palette) {
    free(palette);
    palette = nullptr;
  }

  if (pauseBuffer) {
    free(pauseBuffer);
    pauseBuffer = nullptr;
  }

  vmupro_audio_exit_listen_mode();
}

void app_main(void) {
  vmupro_log(VMUPRO_LOG_INFO, kLogNESEmu, "Starting %s v%s", APP_STRING, APP_VERSION);

  // // ============================================================
  // // Flash DCache security test
  // // Tests whether Core 1 can read firmware through DCache at
  // // 0x3C000000. Writes full dump to SD and reports statistics.
  // // TRM says unauthorized reads return 0xdeadbeaf.
  // // ============================================================

  // vmupro_log(VMUPRO_LOG_INFO, "SECURITY", "=== Flash DCache dump test ===");

  // // Flash DCache window: 0x3C000000 - 0x3CFFFFFF (16MB max)
  // // Firmware is ~2-4MB typically. We'll scan 4MB.
  // const uint32_t FLASH_BASE     = 0x3C000000;
  // const uint32_t SCAN_SIZE      = 4 * 1024 * 1024;  // 4MB
  // const uint32_t PAGE_SIZE      = 4096;
  // const uint32_t WORDS_PER_PAGE = PAGE_SIZE / 4;
  // const uint32_t TOTAL_PAGES    = SCAN_SIZE / PAGE_SIZE;
  // const uint32_t DEADBEAF       = 0xdeadbeaf;

  // // Allocate one page buffer for reading (must use 32-bit reads)
  // uint32_t* pageBuf = (uint32_t*)malloc(PAGE_SIZE);
  // if (!pageBuf) {
  //   vmupro_log(VMUPRO_LOG_ERROR, "SECURITY", "Failed to allocate page buffer");
  //   return;
  // }

  // // --- Test 1: Full dump to file + statistics ---
  // vmupro_log(VMUPRO_LOG_INFO, "SECURITY", "Test 1: Full 4MB dump to /sdcard/flash_dump.bin");

  // FILE* f                 = fopen("/sdcard/flash_dump.bin", "wb");
  // uint32_t pages_readable = 0;
  // uint32_t pages_blocked  = 0;
  // uint32_t pages_zero     = 0;
  // uint32_t pages_mixed    = 0;
  // uint32_t total_deadbeaf = 0;
  // uint32_t total_zero     = 0;
  // uint32_t total_real     = 0;

  // for (uint32_t page = 0; page < TOTAL_PAGES; page++) {
  //   volatile uint32_t* src = (volatile uint32_t*)(FLASH_BASE + page * PAGE_SIZE);
  //   uint32_t dead_count    = 0;
  //   uint32_t zero_count    = 0;
  //   uint32_t real_count    = 0;

  //   // Read page using 32-bit reads
  //   for (uint32_t w = 0; w < WORDS_PER_PAGE; w++) {
  //     uint32_t val = src[w];
  //     pageBuf[w]   = val;
  //     if (val == DEADBEAF)
  //       dead_count++;
  //     else if (val == 0)
  //       zero_count++;
  //     else
  //       real_count++;
  //   }

  //   // Write page to file
  //   if (f) {
  //     fwrite(pageBuf, 1, PAGE_SIZE, f);
  //   }

  //   // Classify page
  //   if (dead_count == WORDS_PER_PAGE) {
  //     pages_blocked++;
  //   }
  //   else if (zero_count == WORDS_PER_PAGE) {
  //     pages_zero++;
  //   }
  //   else if (dead_count == 0 && real_count > 0) {
  //     pages_readable++;
  //   }
  //   else {
  //     pages_mixed++;
  //   }

  //   total_deadbeaf += dead_count;
  //   total_zero += zero_count;
  //   total_real += real_count;

  //   // Log every 256 pages (1MB)
  //   if ((page & 0xFF) == 0) {
  //     vmupro_log(
  //         VMUPRO_LOG_INFO,
  //         "SECURITY",
  //         "  page %lu/%lu: readable=%lu blocked=%lu zero=%lu mixed=%lu",
  //         (unsigned long)page,
  //         (unsigned long)TOTAL_PAGES,
  //         (unsigned long)pages_readable,
  //         (unsigned long)pages_blocked,
  //         (unsigned long)pages_zero,
  //         (unsigned long)pages_mixed
  //     );
  //   }
  // }

  // if (f) fclose(f);

  // uint32_t total_words = SCAN_SIZE / 4;
  // vmupro_log(VMUPRO_LOG_INFO, "SECURITY", "--- Full dump results (4MB / %lu pages) ---", (unsigned long)TOTAL_PAGES);
  // vmupro_log(VMUPRO_LOG_INFO, "SECURITY", "  Pages readable (real data):  %lu", (unsigned long)pages_readable);
  // vmupro_log(VMUPRO_LOG_INFO, "SECURITY", "  Pages blocked (0xdeadbeaf): %lu", (unsigned long)pages_blocked);
  // vmupro_log(VMUPRO_LOG_INFO, "SECURITY", "  Pages all-zero:             %lu", (unsigned long)pages_zero);
  // vmupro_log(VMUPRO_LOG_INFO, "SECURITY", "  Pages mixed:                %lu", (unsigned long)pages_mixed);
  // vmupro_log(
  //     VMUPRO_LOG_INFO,
  //     "SECURITY",
  //     "  Words: real=%lu dead=%lu zero=%lu / %lu total",
  //     (unsigned long)total_real,
  //     (unsigned long)total_deadbeaf,
  //     (unsigned long)total_zero,
  //     (unsigned long)total_words
  // );

  // // --- Test 2: Probe specific non-cached regions ---
  // // Core 0 accesses .rodata near the start of flash mapping.
  // // Deep offsets (2MB+, 3MB+) are unlikely to be cached.
  // vmupro_log(VMUPRO_LOG_INFO, "SECURITY", "--- Test 2: Probe specific offsets ---");

  // const uint32_t probe_offsets[] = {
  //     0x00000000,  // Start of flash (likely cached - GOT, early .rodata)
  //     0x00010000,  // 64KB in
  //     0x00040000,  // 256KB in
  //     0x00080000,  // 512KB in
  //     0x00100000,  // 1MB in
  //     0x00200000,  // 2MB in (likely NOT cached)
  //     0x00300000,  // 3MB in (likely NOT cached)
  //     0x003F0000,  // Near end of 4MB (likely NOT cached)
  // };
  // const int num_probes = sizeof(probe_offsets) / sizeof(probe_offsets[0]);

  // for (int p = 0; p < num_probes; p++) {
  //   volatile uint32_t* probe = (volatile uint32_t*)(FLASH_BASE + probe_offsets[p]);
  //   uint32_t w0              = probe[0];
  //   uint32_t w1              = probe[1];
  //   uint32_t w2              = probe[2];
  //   uint32_t w3              = probe[3];

  //   const char* status;
  //   if (w0 == DEADBEAF && w1 == DEADBEAF && w2 == DEADBEAF && w3 == DEADBEAF) {
  //     status = "BLOCKED";
  //   }
  //   else if (w0 == 0 && w1 == 0 && w2 == 0 && w3 == 0) {
  //     status = "ALL-ZERO";
  //   }
  //   else {
  //     status = "READABLE";
  //   }

  //   vmupro_log(
  //       VMUPRO_LOG_INFO,
  //       "SECURITY",
  //       "  0x%08lx: [%08lx %08lx %08lx %08lx] %s",
  //       (unsigned long)(FLASH_BASE + probe_offsets[p]),
  //       (unsigned long)w0,
  //       (unsigned long)w1,
  //       (unsigned long)w2,
  //       (unsigned long)w3,
  //       status
  //   );
  // }

  // // --- Test 3: First 16 words at 0x3C000000 (for comparison with previous tests) ---
  // vmupro_log(VMUPRO_LOG_INFO, "SECURITY", "--- Test 3: First 16 words at 0x3C000000 ---");
  // volatile uint32_t* flashStart = (volatile uint32_t*)FLASH_BASE;
  // for (int i = 0; i < 16; i++) {
  //   uint32_t val = flashStart[i];
  //   vmupro_log(
  //       VMUPRO_LOG_INFO,
  //       "SECURITY",
  //       "  [%02d] 0x%08lx %s",
  //       i,
  //       (unsigned long)val,
  //       val == DEADBEAF ? "(BLOCKED)"
  //       : val == 0      ? "(ZERO)"
  //                       : ""
  //   );
  // }

  // vmupro_log(VMUPRO_LOG_INFO, "SECURITY", "=== Flash DCache dump test complete ===");

  // free(pageBuf);

  vmupro_emubrowser_settings_t emuSettings = {
      .version         = 1,
      .title           = "NES",
      .rootPath        = "/sdcard/roms/NES",
      .filterExtension = ".nes",
      .showFiles       = true,
      .showFolders     = true,
      .showIcons       = true
  };
  vmupro_emubrowser_init(emuSettings);

  launchfile = (char*)malloc(512);
  memset(launchfile, 0x00, 512);
  vmupro_emubrowser_render_contents(launchfile);
  if (strlen(launchfile) == 0) {
    vmupro_log(VMUPRO_LOG_ERROR, kLogNESEmu, "We somehow exited the browser with no file to show!");
    return;
  }

  // char launchPath[512 + 22];
  // vmupro_snprintf(launchPath, (512 + 22), "/sdcard/roms/NES/%s", launchfile);
  filename = (char*)malloc(512);
  memset(filename, 0x00, 512);
  char* filename_ptr = strrchr(launchfile, '/');
  if (filename_ptr != nullptr) {
    filename_ptr++;  // Move past the '/'
    strncpy(filename, filename_ptr, 511);
  }
  else {
    strncpy(filename, launchfile, 511);
  }

  pauseBuffer = (uint8_t*)malloc(115200);

  nes = nes_init(SYS_DETECT, 44100, true, NULL);
  if (!nes) {
    vmupro_log(VMUPRO_LOG_ERROR, kLogNESEmu, "Error initialising NES Emulator!");
    return;
  }

  int ret = -1;
  ret     = nes_loadfile(launchfile);

  if (ret == -1) {
    vmupro_log(VMUPRO_LOG_ERROR, kLogNESEmu, "Error loading rom %s", launchfile);
    return;
  }
  else if (ret == -2) {
    vmupro_log(VMUPRO_LOG_ERROR, kLogNESEmu, "Unsupported mapper for rom %s", launchfile);
    return;
  }
  else if (ret == -3) {
    vmupro_log(VMUPRO_LOG_ERROR, kLogNESEmu, "BIOS file required for rom %s", launchfile);
    return;
  }
  else if (ret < 0) {
    vmupro_log(VMUPRO_LOG_ERROR, kLogNESEmu, "Unsupported ROM %s", launchfile);
    return;
  }

  // nes->refresh_rate gets us the refresh rate
  vmupro_start_double_buffer_renderer();
  // nes->blit_func  = blitScreen;
  nes_back_buffer = vmupro_get_back_buffer();
  nes_setvidbuf(nes_back_buffer);

  // nsfPlayer whatever this is: nes->cart->type == ROM_TYPE_NSF;

  ppu_setopt(PPU_LIMIT_SPRITES, true);  // Make this configurable
  buildPalette(NES_PALETTE_SMOOTH);

  vmupro_audio_start_listen_mode();

  if (!vmupro_folder_exists("/sdcard/roms/NES/STATE")) {
    vmupro_create_folder("/sdcard/roms/NES/STATE");
  }

  if (!vmupro_folder_exists("/sdcard/roms/NES/SAVES")) {
    vmupro_create_folder("/sdcard/roms/NES/SAVES");
  }

  char sramfile[512];
  strcpy(sramfile, "/sdcard/roms/NES/SAVES/");
  strcat(sramfile, filename);
  strcat(sramfile, ".sram");
  rom_loadsram(sramfile);

  // Apparently we need to emulate two frames in order to restore the state
  nes_emulate(false);
  nes_emulate(false);

  renderFrame = 1;

  vmupro_log(VMUPRO_LOG_INFO, kLogNESEmu, "NES Emulator initialisation done");

  // Loop
  Tick();

  // When loop exits, we fall through below
  Exit();
}
