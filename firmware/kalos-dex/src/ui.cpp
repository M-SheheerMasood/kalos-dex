#include "ui.h"
#include "fonts/nunito_xb.inc"

namespace ui {
static const uint8_t *const FONT[] = {FONT_SMALL, FONT_BODY, FONT_TITLE};
static constexpr int RIM = 2;

void text(TFT_eSPI &g, const char *s, int x, int y, Font f) {
  g.loadFont(FONT[f]);
  g.setTextWrap(false, false);   // a string running off the edge must not wrap onto the next line
  g.drawString(s, x, y);
  g.unloadFont();
}

int width(TFT_eSPI &g, const char *s, Font f) {
  g.loadFont(FONT[f]);
  const int w = g.textWidth(s);
  g.unloadFont();
  return w;
}

Font fit(TFT_eSPI &g, const char *s, int maxW, Font f) {
  while (f > SMALL && width(g, s, f) > maxW) f = Font(f - 1);
  return f;
}

void hpBar(TFT_eSPI &g, int x, int y, int w, int h, float frac, uint16_t fill, uint16_t track) {
  const int iw = w - 2 * RIM, ih = h - 2 * RIM, ir = ih / 2;
  g.fillRoundRect(x, y, w, h, h / 2, TFT_BLACK);
  g.fillRoundRect(x + RIM, y + RIM, iw, ih, ir, track);
  int fw = lroundf(iw * constrain(frac, 0.0f, 1.0f));
  if (fw <= 0) return;
  fw = max(fw, ih);   // a sliver still looks round
  g.fillRoundRect(x + RIM, y + RIM, fw, ih, ir, fill);
  if (ih >= 5 && fw > ir + 3)   // shine: a lighter line along the top of the fill
    g.drawFastHLine(x + RIM + ir / 2 + 1, y + RIM + 1, fw - 2 * (ir / 2) - 2, g.alphaBlend(128, TFT_WHITE, fill));
}
}  // namespace ui
