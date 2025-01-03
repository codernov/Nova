#include "components.h"
#include "../driver/st7565.h"
#include "../helper/measurements.h"
#include "../helper/presetlist.h"
#include "graphics.h"
#include <stdint.h>

void UI_Battery(uint8_t Level) {
  DrawRect(LCD_WIDTH - 13, 0, 12, 5, C_FILL);
  FillRect(LCD_WIDTH - 12, 1, Level, 3, C_FILL);
  DrawVLine(LCD_WIDTH - 1, 1, 3, C_FILL);

  if (Level > 10) {
    DrawHLine(LCD_WIDTH - 1 - 3, 1, 3, C_INVERT);
    DrawHLine(LCD_WIDTH - 1 - 7, 1, 5, C_INVERT);
    DrawHLine(LCD_WIDTH - 1 - 3, 3, 3, C_INVERT);
  }
}

//---------------------------------------------------------------------------
void UI_RSSIBar(uint16_t rssi, uint8_t snr, uint32_t f, uint8_t y) 
  {
    //  if (rssi == 0) {
    //    return;
    //  }

    const uint8_t BAR_LEFT_MARGIN = 0;
    uint8_t BAR_WIDTH = LCD_WIDTH - BAR_LEFT_MARGIN - 22;
    const uint8_t BAR_BASE = y + 7;

    FillRect(0, y, LCD_WIDTH, 8, C_CLEAR);

    const uint16_t RSSI_MIN = 10;
    const uint16_t RSSI_MAX = 350;
    const uint16_t SNR_MIN = 0;
    const uint16_t SNR_MAX = 30;
  
    uint8_t rssiW = ConvertDomain(rssi, RSSI_MIN, RSSI_MAX, 0, BAR_WIDTH);
    uint8_t snrW = ConvertDomain(RADIO_GetSNR(), SNR_MIN, SNR_MAX, 0, BAR_WIDTH);

    if (!gIsListening)
      {
        rssiW = 0;
        snrW = 0;
        BAR_WIDTH += 22;
      }

    FillRect(BAR_LEFT_MARGIN, y + 2, rssiW, 4, C_FILL);
    FillRect(BAR_LEFT_MARGIN, y + 7, snrW, 1, C_FILL);

    DrawHLine(0, y + 5, BAR_WIDTH - 2, C_FILL);
    for (int16_t r = Rssi2DBm(RSSI_MIN); r < Rssi2DBm(RSSI_MAX); r++) 
      {
        if (r % 10 == 0) 
          {
            FillRect(ConvertDomain( r, Rssi2DBm(RSSI_MIN), Rssi2DBm(RSSI_MAX), 0,
                                    BAR_WIDTH),
                                    y + 5 - (r % 50 == 0 ? 2 : 1), 1, r % 50 == 0 ? 2 : 1, C_INVERT);
          }
      }
  
    if (!gIsListening)
      return;
      
    PrintMediumEx(LCD_WIDTH - 1, BAR_BASE, 2, true, "%d", Rssi2DBm(rssi));

    uint8_t dBm = Rssi2DBm(rssi)*-1;
    uint8_t dBmMax6 = (f>=3000000) ? 93 : 73;
    uint8_t dBmMax10 = (f>=3000000) ? 33 : 13;

  
    if ((gIsListening || dBmMax6==73) && dBm>0 && dBm<(dBmMax6+49) && y==44) 
      { // active or <30mhz & 1VFO mode - (in 1VFO y=BASE+2)
        if(dBm>(dBmMax6-10))
          { 
            uint8_t s=((dBm-dBmMax6)/6)+(1*((dBm-dBmMax6)%6)>0); 
            if (dBm<dBmMax6) 
              s=0;
              
            PrintMediumEx(LCD_WIDTH - 1, LCD_HEIGHT - 4, POS_R, C_FILL, "S%u", 9-s);
          } 
        else 
          {
            uint8_t s=((dBm-dBmMax10)/10)+(1*((dBm-dBmMax10)%10)>0); 
            if (dBm<dBmMax10) 
              s=0;
     
            PrintMediumEx(LCD_WIDTH - 1, LCD_HEIGHT - 4, POS_R, C_FILL, "S9+%u0", 6-s);
          }
      } 
  }

//---------------------------------------------------------------------------
void UI_FSmall(uint32_t f) {
  SQL sq = GetSql(gCurrentPreset->band.squelch);

  PrintSmall(0, 18, "%u", RADIO_GetRSSI());
  PrintSmall(38, 18, "%u", BK4819_GetNoise());
  PrintSmall(76, 18, "%u", BK4819_GetGlitch());

  PrintSmall(0, 12,"%u/%u", sq.ro, sq.rc);
  PrintSmall(38, 12,"%u/%u", sq.no, sq.nc);
  PrintSmall(76, 12,"%u/%u", sq.go, sq.gc);

  PrintMediumEx(LCD_WIDTH, 14, POS_R, C_FILL, "%u", RADIO_GetSNR());

  PrintSmallEx(LCD_WIDTH, 20, POS_R, true,
               RADIO_GetBWName(gCurrentPreset->band.bw));
   
  if (gCurrentPreset->band.gainIndex != AUTO_GAIN_INDEX) 
    {
      PrintSmall(0, 29, "%+ddB", -gainTable[gCurrentPreset->band.gainIndex].gainDb + 33);
    }
  else
    {
      PrintSmall(0, 29, "AUTO");
    }


               
  PrintMedium(0, 43, "%s %u", sqTypeNames[gCurrentPreset->band.squelchType], gCurrentPreset->band.squelch);
               
  const uint32_t step = StepFrequencyTable[gCurrentPreset->band.step];
  PrintSmallEx(0, 35, POS_L, C_FILL, "%d.%02dk", step / 100, step % 100);

}


void UI_FSmallest(uint32_t f, uint8_t x, uint8_t y) {
  PrintSmall(x, y, "%u.%05u", f / MHZ, f % MHZ);
}

void drawTicks(uint8_t y, uint32_t fs, uint32_t fe, uint32_t div, uint8_t h) {
  for (uint32_t f = fs - (fs % div) + div; f < fe; f += div) {
    uint8_t x = ConvertDomain(f, fs, fe, 0, LCD_WIDTH - 1);
    DrawVLine(x, y, h, C_FILL);
  }
}

void UI_DrawTicks(uint8_t y, const Band *band) {
  const FRange *range = &band->bounds;
  uint32_t fs = range->start;
  uint32_t fe = range->end;
  uint32_t bw = fe - fs;

  for (uint32_t p = 100000000; p >= 10; p /= 10) {
    if (p < bw) {
      drawTicks(y, fs, fe, p / 2, 2);
      drawTicks(y, fs, fe, p, 3);
      return;
    }
  }
}

void UI_DrawSpectrumElements(const uint8_t sy, uint8_t msmDelay, int16_t sq,
                             Band *currentBand) {
  PrintSmallEx(0, sy - 3, POS_L, C_FILL, "%ums", msmDelay);
  if (sq >= 255) {
    PrintSmallEx(LCD_WIDTH - 2, sy - 3, POS_R, C_FILL, "SQ off");
  } else {
    PrintSmallEx(LCD_WIDTH - 2, sy - 3, POS_R, C_FILL, "SQ %d", sq);
  }
  PrintSmallEx(LCD_WIDTH - 2, sy - 3 + 6, POS_R, C_FILL, "%s",
               modulationTypeOptions[currentBand->modulation]);

  if (gLastActiveLoot) {
    PrintMediumBoldEx(LCD_XCENTER, 14, POS_C, C_FILL, "%u.%05u",
                      gLastActiveLoot->f / MHZ, gLastActiveLoot->f % MHZ);
  }

  uint32_t fs = currentBand->bounds.start;
  uint32_t fe = currentBand->bounds.end;

  PrintSmallEx(0, LCD_HEIGHT - 1, POS_L, C_FILL, "%u.%05u", fs / MHZ, fs % MHZ);
  PrintSmallEx(LCD_WIDTH, LCD_HEIGHT - 1, POS_R, C_FILL, "%u.%05u", fe / MHZ,
               fe % MHZ);
}

void UI_ShowWait() {
  FillRect(0, 32 - 5, 128, 9, C_FILL);
  PrintMediumBoldEx(64, 32 + 2, POS_C, C_CLEAR, "WAIT");
  ST7565_Blit();
}
