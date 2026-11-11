#include "u_common.h"
#include "u_fb.h"

// Colors are 0xAABBGGRR.
#define DUCK   0xFF00FFFF   // Y bright yellow body
#define BEAK   0xFF0045FF   // o orange beak
#define AMBER  0xFF10A0F0   // d darker orange (belly patch, beak shadow)
#define OUTLN  0xFF000000   // K black outline / eye
#define NAVY   0xFF6E281E   // N hat
#define GOLD   0xFF00C0FF   // A anchor + hat stripe
#define WATER  0xFFEBB45A   // B light blue
#define DWATER 0xFFB45A28   // D darker blue (right)

// Captain rubber duck, faced left. One char per block:
//   Y body  o beak  d amber  K outline/eye  N navy hat  A gold  B water  D dark water
//   '.' background
static const char* DUCK_ART[] = {
  "......KKKKK.......................",
  ".....KKNNNNKK.....................",
  ".....KNNNNNNNK....................",
  ".....KNYYNNNNNKK..................",
  "...KK.YAANNNNNNNK.................",
  ".KKKKKKKKYYYNNNNNK................",
  "...KKKKKKKKKYY.NKK................",
  "....KKYYYYYYKKKKK.................",
  "....KYYYYYYY...KK.................",
  "....KYYYYYYYYYYK..................",
  ".KKKKYYKYYYYYYYYK.................",
  "KoooYYYKYYYYYYYYK.................",
  "KooooYYYYYYYYYYYK.................",
  ".KKooYYYYYYYYYYYK.................",
  "...KKAYYYYYYYYYK..................",
  ".....KKYYYYYYYKK..................",
  "....KYYYYYYYYKKKKKKKKKKKKKKK......",
  "....KYYYYYYYYYYYYYYYYYYYYYYK......",
  "...KYYYYYYYYYYYYYYYYYYYYYYK.......",
  ".KKYYYYYYYYdddddddddYYYYYYK.......",
  ".KKYYYYYYYYYdddddddYYYYYYK........",
  ".KKYYYYYYYYYYddddYYYYYYYYK........",
  "...KYYYYYYYYYYYYYYYYYYY.K.........",
  "...KYYYYYYYYYYYYYYYYYYKKKK........",
  ".KKBBYYYYYYYYYYYYYYYYBBBBBDDDNBBBK",
  "...KBBBBBBBBBBBBBBBBBBBBBBBKKKKKK.",
  "...KKKKKKKKKKKKKKKKKKKKKKKK.......",
};
#define ROWS ((int)(sizeof(DUCK_ART) / sizeof(DUCK_ART[0])))
#define PX   20   // size of one sprite cell, in screen pixels

static uint32 cell_color(char c) {
  switch (c) {
    case 'Y': return DUCK;
    case 'o': return BEAK;
    case 'd': return AMBER;
    case 'K': return OUTLN;
    case 'N': return NAVY;
    case 'A': return GOLD;
    case 'B': return WATER;
    case 'D': return DWATER;
    default:  return 0;   // transparent
  }
}

int main(void) {
  struct canvas* cv = init_canvas();   // claim this process's window, or NULL
  if (!cv) {
    printf("logo: no canvas, nothing to draw\r\n");
    for (;;) yield();
  }

  struct canvas screen = *cv;
  fb_fill_rect(&screen, 0, 0, screen.width, screen.height, 0xFFF0F0F0);  // grey

  int cols = 0;   // widest row, so ragged rows still center
  for (int r = 0; r < ROWS; r++) {
    int len = 0;
    while (DUCK_ART[r][len]) len++;
    if (len > cols) cols = len;
  }
  const char* name = "Duckie OS \x01";
  uint32 scale_n = 8;
  int name_len = 0; while (name[name_len]) name_len++;
  int name_w = 9 * (int)scale_n * name_len;    // 9px advance per glyph
  int name_h = 8 * (int)scale_n;
  
  const char* caption = "A thoroughly debugged kernel";
  uint32 scale_c = 4;
  int cap_len = 0; while (caption[cap_len]) cap_len++;
  int cap_w = 9 * (int)scale_c * cap_len;    // 9px advance per glyph
  int cap_h = 8 * (int)scale_c;

  // Stack the duck and the name as one block, vertically centered.
  int gap = 24;
  int block_h = ROWS * PX + gap + name_h + gap + cap_h;
  int ox = ((int)screen.width  - cols * PX) / 2;
  int oy = ((int)screen.height - block_h) / 2;
  int name_y = oy + ROWS * PX + gap;
  int cap_y = name_y + name_h + gap;

  for (int r = 0; r < ROWS; r++)
    for (int c = 0; DUCK_ART[r][c]; c++) {
      uint32 color = cell_color(DUCK_ART[r][c]);
      if (color)
        fb_fill_rect(&screen, (uint32)(ox + c * PX), (uint32)(oy + r * PX), PX, PX, color);
    }

  draw_string(&screen, ((int)screen.width - name_w) / 2,
              name_y, OUTLN, name, scale_n);
  draw_string(&screen, ((int)screen.width - cap_w) / 2,
              cap_y, OUTLN, caption, scale_c);
  // We drew into a local copy of the canvas header, so cv->changed was never
  // set. Flag the REAL canvas so the compositor copies our pixels (like clock).
  cv->changed = 1;
  for (;;) {
    yield();
  }
  exit(0);
}
