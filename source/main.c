#include <nds.h>
#include <math.h>
#include <stdio.h>
#include <time.h>

#include "entries.h"
#include "bgsub.h"
#include "selector.h"

#define WIDTH 256
#define HEIGHT 192

#define SELECTOR_Y 139
#define SELECTOR_X 49

int max(int x, int y) {
  if (x > y) return x;
  return y;
}

int min(int x, int y) {
  if (x > y) return y;
  return x;
}

void init_sub_bg() {
    int subBg = bgInitSub(3, BgType_Bmp8, BgSize_B8_256x256, 1, 0);
    bgSetPriority(subBg, 3);
    dmaCopy(bgsubBitmap, bgGetGfxPtr(subBg), bgsubBitmapLen);
    dmaCopy(bgsubPal, BG_PALETTE_SUB, bgsubPalLen);
}

u16* selector_gfx;

void copy_selector_gfx(int step) {
  u8* src = (u8*)selectorTiles; // TODO anim
  dmaCopy(src, selector_gfx, 16 * 16);
}

void init_selector_gfx() {
  dmaCopy(selectorPal, SPRITE_PALETTE_SUB, selectorPalLen);
  selector_gfx = oamAllocateGfx(&oamSub, SpriteSize_16x16, SpriteColorFormat_256Color);
  copy_selector_gfx(0);
}

int selected = 0;

void set_selector_gfx() {
  int y = SELECTOR_Y;
  int x = SELECTOR_X + selected * 24;
  oamSet(&oamSub, SELECTOR_ENTRY, x, y, 0, 0,
         SpriteSize_16x16, SpriteColorFormat_256Color, selector_gfx, -1,
         false, false, false, false, false);
}

int get_pixel(int x, int y) {
  return VRAM_A[y * 256 + x];
}

void set_pixel(int x, int y, int c) {
  VRAM_A[y * 256 + x] = c;
}

void clear_screen() {
  dmaCopy(VRAM_B, VRAM_A, 256 * 192 * 2);
}

void add_sand(int x, int y, int color) {
  if(!get_pixel(x, y)) set_pixel(x, y, color);
}

void update_cell(int x, int y) {
  int c = get_pixel(x, y);
  if(c && y < HEIGHT-1) {
    if(!get_pixel(x, y+1)) {
      set_pixel(x, y+1, c);
      set_pixel(x, y, 0);
    } else {
      int leftOk = x > 0 && !get_pixel(x-1,y+1);
      int rightOk = x < WIDTH-1 && !get_pixel(x+1,y+1);
      if(leftOk && rightOk) {
        if(rand() % 2) {
          set_pixel(x-1, y+1, c);
          set_pixel(x, y, 0);
        } else {
          set_pixel(x+1, y+1, c);
          set_pixel(x, y, 0);
        }
      } else if(leftOk) {
        set_pixel(x-1, y+1, c);
        set_pixel(x, y, 0);
      } else if(rightOk) {
        set_pixel(x+1, y+1, c);
        set_pixel(x, y, 0);
      }
    }
  }
}

void update_world() {
  for(int y = HEIGHT-1; y >= 0; y--) {
    if(y % 2) {
      for(int x = 0; x < WIDTH; x++) update_cell(x, y);
    } else {
      for(int x = WIDTH-1; x >= 0; x--) update_cell(x, y);
    }
  }
}

// 0 <= h < 360
int hsv_to_rgb(int h, float v, float s) {
  float c = v * s;
  float h1 = (float)h / 60;
  while(h1 > 2) { h1 -= 2; }
  float x = c * (1 - fabs(h1 - 1));
  float r1 = 0;
  float g1 = 0;
  float b1 = 0;
  if(h < 60) { r1 = c; g1 = x; }
  else if(h < 120) { r1 = x; g1 = c; }
  else if(h < 180) { g1 = c; b1 = x; }
  else if(h < 240) { g1 = x; b1 = c; }
  else if(h < 300) { r1 = x; b1 = c; }
  else { r1 = c; b1 = x; }
  float m = v - c;
  return RGB15((int)((r1 + m) * 31), (int)((g1 + m) * 31), (int)((b1 + m) * 31));
}

int get_color() {
  static int current_hue;

  switch(selected) {
  case 1: return RGB15(31, 0, 0);
  case 2: return RGB15(31, 15, 0);
  case 3: return RGB15(31, 31, 0);
  case 4: return RGB15(0, 31, 0);
  case 5: return RGB15(0, 31, 31);
  case 6: return RGB15(0, 0, 31);
  case 7: return RGB15(31, 0, 31);
  default:
    current_hue = (current_hue+1)%360;
    return hsv_to_rgb(current_hue, 1, 1);
  }
}

void vblank() {

  scanKeys();
  int held = keysHeld();

  if(held & KEY_TOUCH) {
    touchPosition touch;
    touchRead(&touch);

    int c = get_color();
    for(int x = touch.px-1; x <= touch.px+1; x++) {
      for(int y = touch.py-1; y <= touch.py+1; y++) {
        add_sand(min(WIDTH-1, max(0, x)), min(HEIGHT-1, max(0, y)), c);
      }
    }
  }

  int down = keysDown();
  if(down & KEY_START) {
    clear_screen();
  }
  if(down & KEY_LEFT) {
    selected = (selected + 7) % 8;
  } else if(down & KEY_RIGHT) {
    selected = (selected + 1) % 8;
  }

  oamUpdate(&oamSub);
}

int main() {
  srand(time(NULL));
  lcdSwap();

  // Main screen
  videoSetMode(MODE_FB0);
  vramSetBankA(VRAM_A_LCD);

  // Sub screen (touch screen)
  videoSetModeSub(MODE_5_2D);
  vramSetBankC(VRAM_C_SUB_BG);
  vramSetBankD(VRAM_D_SUB_SPRITE);
  init_sub_bg();
  oamInit(&oamSub, SpriteMapping_1D_32, false);
  init_selector_gfx();

  irqSet(IRQ_VBLANK, vblank);

  while(true) {
    update_world();
    set_selector_gfx();
    swiWaitForVBlank();
  }
}
