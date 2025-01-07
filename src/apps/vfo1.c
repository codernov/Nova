#include "vfo1.h"
#include "../apps/textinput.h"
#include "../driver/bk4819.h"
#include "../helper/channels.h"
#include "../helper/lootlist.h"
#include "../helper/measurements.h"
#include "../helper/numnav.h"
#include "../helper/presetlist.h"
#include "../radio.h"
#include "../scheduler.h"
#include "../svc.h"
#include "../svc_render.h"
#include "../svc_scan.h"
#include "../ui/components.h"
#include "../ui/graphics.h"
#include "../ui/menu.h"
#include "apps.h"
#include "finput.h"
#include "../ui/statusline.h"
#include "level.h"
#include "../dcs.h"


bool gVfo1ProMode = false;

static uint8_t menuIndex = 0;
static bool registerActive = false;

static char String[16];

const RegisterSpec registerSpecs[] = {
    {"Gain", BK4819_REG_13, 0, 0xFFFF, 1},
    {"RF", BK4819_REG_43, 12, 0b111, 1},
    {"RFwe", BK4819_REG_43, 9, 0b111, 1}, 

    {"IF", 0x3D, 0, 0xFFFF, 100},

    {"DEV", 0x40, 0, 0xFFF, 10},
    {"300T", 0x44, 0, 0xFFFF, 1000},
    RS_RF_FILT_BW,
    RS_XTAL_MODE,
    {"AFTxfl", 0x43, 6, 0b111, 1}, // 7 is widest
    {"3kAFrsp", 0x74, 0, 0xFFFF, 100},
    {"CMP", 0x31, 3, 1, 1},
    {"MIC", 0x7D, 0, 0x1F, 1},

    {"AGCL", 0x49, 0, 0b1111111, 1},
    {"AGCH", 0x49, 7, 0b1111111, 1},
    {"AFC", 0x73, 0, 0xFF, 1},
};

static void UpdateRegMenuValue(RegisterSpec s, bool add) {
  uint16_t v, maxValue;

  if (s.num == BK4819_REG_13) {
    v = gCurrentPreset->band.gainIndex;
    maxValue = ARRAY_SIZE(gainTable) - 1;
  } else if (s.num == 0x73) {
    v = BK4819_GetAFC();
    maxValue = 8;
  } else {
    v = BK4819_GetRegValue(s);
    maxValue = s.mask;
  }

  if (add && v <= maxValue - s.inc) {
    v += s.inc;
  } else if (!add && v >= 0 + s.inc) {
    v -= s.inc;
  }

  if (s.num == BK4819_REG_13) {
    RADIO_SetGain(v);
  } else if (s.num == 0x73) {
    BK4819_SetAFC(v);
  } else {
    BK4819_SetRegValue(s, v);
  }
}

static void setChannel(uint16_t v) { RADIO_TuneToCH(v - 1); }
static void tuneTo(uint32_t f) { RADIO_TuneToSave(GetTuneF(f)); }

static bool SSB_Seek_ON = false;
static bool SSB_Seek_UP = true;

static int32_t scanIndex = 0;

static char message[16] = {'\0'};
static void sendDtmf() {
  RADIO_ToggleTX(true);
  if (gTxState == TX_ON) {
    BK4819_EnterDTMF_TX(true);
    BK4819_PlayDTMFString(message, true, 100, 100, 100, 100);
    RADIO_ToggleTX(false);
  }
}

static void channelScanFn(bool forward) {
  IncDecI32(&scanIndex, 0, gScanlistSize, forward ? 1 : -1);
  int32_t chNum = gScanlist[scanIndex];
  radio->channel = chNum;
  RADIO_VfoLoadCH(gSettings.activeVFO);
  RADIO_SetupByCurrentVFO();
}

void VFO1_init(void) {
  gDW.activityOnVFO = -1;
  if (!gVfo1ProMode) {
    gVfo1ProMode = gSettings.iAmPro;
  }
  RADIO_LoadCurrentVFO();
}

//static uint32_t lastUpdate = 0;
//static Loot msm;

void VFO1_update(void) {

//  if (Now() - lastUpdate > 50) {
//    lastUpdate = Now();
//    msm.rssi = RADIO_GetRSSI();
//    SP_Shift(-1);
//    SP_AddGraphPoint(&msm);
//    gRedrawScreen = true;
//  }

//  LEVEL_update();
gRedrawScreen = true;

  if (SSB_Seek_ON) {
    if (RADIO_GetRadio() == RADIO_SI4732 && RADIO_IsSSB()) {
      if (Now() - gLastRender >= 150) {
        if (SSB_Seek_UP) {
          gScanForward = true;
          RADIO_NextFreqNoClicks(true);
        } else {
          gScanForward = false;
          RADIO_NextFreqNoClicks(false);
        }
        gRedrawScreen = true;
        if (gVfo1ProMode) {
          RADIO_UpdateMeasurements();
        }
      }
    }
  }

  if (gIsListening && Now() - gLastRender >= 500) {
    gRedrawScreen = true;
    if (gVfo1ProMode) {
      RADIO_UpdateMeasurements();
    }
  }
}

static void prepareABScan() {
  const uint32_t F1 = gVFO[0].rx.f;
  const uint32_t F2 = gVFO[1].rx.f;
  FRange *b = &defaultPreset.band.bounds;

  if (F1 < F2) {
    b->start = F1;
    b->end = F2;
  } else {
    b->start = F2;
    b->end = F1;
  }
  sprintf(defaultPreset.band.name, "%u-%u", b->start / MHZ, b->end / MHZ);
  gCurrentPreset = &defaultPreset;
  defaultPreset.lastUsedFreq = radio->rx.f;
  gSettings.crossBandScan = false;
  RADIO_TuneToPure(b->start, true);
}

static void initChannelScan() {
  scanIndex = 0;
  LOOT_Clear();
  CHANNELS_LoadScanlist(gSettings.currentScanlist);
  if (gScanlistSize == 0) {
    CHANNELS_LoadScanlist(15);
    SETTINGS_DelayedSave();
  }
  for (uint16_t i = 0; i < gScanlistSize; ++i) {
    CH ch;
    int32_t num = gScanlist[i];
    CHANNELS_Load(num, &ch);
    Loot *loot = LOOT_AddEx(ch.rx.f, true);
    loot->open = false;
    loot->lastTimeOpen = 0;
    loot->blacklist = ch.memoryBanks & (1 << 7);
  }

  gScanFn = channelScanFn;
}

static void startScan() {
  if (RADIO_VfoIsCH()) {
    initChannelScan();
  }
  if (RADIO_VfoIsCH() && gScanlistSize == 0) {
    SVC_Toggle(SVC_SCAN, false, 0);
    return;
  }
  SVC_Toggle(SVC_SCAN, true, gSettings.scanTimeout);
}

static void scanlistByKey(KEY_Code_t key) {
  if (key >= KEY_1 && key <= KEY_8) {
    gSettings.currentScanlist = key - 1;
  } else {
    gSettings.currentScanlist = 15;
  }
}

static void selectFirstPresetFromScanlist() {
  uint8_t sl = gSettings.currentScanlist;
  uint8_t scanlistMask = 1 << sl;
  for (uint8_t i = 0; i < PRESETS_COUNT; ++i) {
    if (sl == 15 ||
        (PRESETS_Item(i)->memoryBanks & scanlistMask) == scanlistMask) {
      PRESET_Select(i);
      RADIO_TuneTo(gCurrentPreset->band.bounds.start);
      return;
    }
  }
}

bool VFOPRO_key(KEY_Code_t key, bool bKeyPressed, bool bKeyHeld) {
  if (key == KEY_PTT) {
    RADIO_ToggleTX(bKeyHeld);
    return true;
  }
  if (bKeyHeld && bKeyPressed && !gRepeatHeld) {
    switch (key) {
    case KEY_4: // freq catch
      if (RADIO_GetRadio() != RADIO_BK4819) {
        gShowAllRSSI = !gShowAllRSSI;
      }
      return true;
    case KEY_5:
      registerActive = !registerActive;
      return true;
    default:
      break;
    }
  }

  bool isSsb = RADIO_IsSSB();

  if (!bKeyHeld || bKeyPressed) {
    switch (key) {
    case KEY_1:
      RADIO_UpdateStep(true);
      return true;
    case KEY_7:
      RADIO_UpdateStep(false);
      return true;
    case KEY_3:
      RADIO_UpdateSquelchLevel(true);
      return true;
    case KEY_9:
      RADIO_UpdateSquelchLevel(false);
      return true;
    case KEY_4:
      return true;
    case KEY_0:
      RADIO_ToggleModulation();
      return true;
    case KEY_6:
      RADIO_ToggleListeningBW();
      return true;
    case KEY_F:
      APPS_run(APP_VFO_CFG);
      return true;
    case KEY_SIDE1:
      if (!gVfo1ProMode) {
        gMonitorMode = !gMonitorMode;
        return true;
      }

      if (RADIO_GetRadio() == RADIO_SI4732 && isSsb) {
        RADIO_TuneToSave(radio->rx.f + 1);
        return true;
      }
      break;
    case KEY_SIDE2:
      if (RADIO_GetRadio() == RADIO_SI4732 && isSsb) {
        RADIO_TuneToSave(radio->rx.f - 1);
        return true;
      }
      break;
    case KEY_EXIT:
      if (registerActive) {
        registerActive = false;
        return true;
      }
      APPS_exit();
      return true;
    default:
      break;
    }
  }

  if (!bKeyPressed && !bKeyHeld) {
    switch (key) {
    case KEY_2:
      if (registerActive) {
        UpdateRegMenuValue(registerSpecs[menuIndex], true);
      } else {
        IncDec8(&menuIndex, 0, ARRAY_SIZE(registerSpecs), -1);
      }
      return true;
    case KEY_8:
      if (registerActive) {
        UpdateRegMenuValue(registerSpecs[menuIndex], false);
      } else {
        IncDec8(&menuIndex, 0, ARRAY_SIZE(registerSpecs), 1);
      }
      return true;
    case KEY_5:
      gFInputCallback = RADIO_TuneToSave;
      APPS_run(APP_FINPUT);
      return true;
    default:
      break;
    }
  }

  return false;
}

bool VFO1_key(KEY_Code_t key, bool bKeyPressed, bool bKeyHeld) {
  if (!SVC_Running(SVC_SCAN) && !bKeyPressed && !bKeyHeld &&
      RADIO_VfoIsCH()) {
    if (!gIsNumNavInput && key <= KEY_9) {
      NUMNAV_Init(radio->channel + 1, 1, CHANNELS_GetCountMax());
      gNumNavCallback = setChannel;
    }
    if (gIsNumNavInput) {
      NUMNAV_Input(key);
      return true;
    }
  }

  if (gVfo1ProMode && VFOPRO_key(key, bKeyPressed, bKeyHeld)) {
    return true;
  }

  if (key == KEY_PTT && !gIsNumNavInput) {
    RADIO_ToggleTX(bKeyHeld);
    //RADIO_ToggleVfoMR();
    return true;
  }

  // up-down keys
  if (bKeyPressed || (!bKeyPressed && !bKeyHeld)) {
    bool isSsb = RADIO_IsSSB();
    
    switch (key) {
    case KEY_UP:
      if (SSB_Seek_ON) {
        SSB_Seek_ON = false;
        SSB_Seek_UP = true;
        return true;
      }
      if (SVC_Running(SVC_SCAN)) {
        gScanForward = true;
      }
      RADIO_NextFreqNoClicks(true);
      return true;
    case KEY_DOWN:
      if (SSB_Seek_ON) {
        SSB_Seek_ON = false;
        SSB_Seek_UP = false;
        return true;
      }
      if (SVC_Running(SVC_SCAN)) {
        gScanForward = false;
      }
      RADIO_NextFreqNoClicks(false);
      return true;
    case KEY_SIDE1:
      if (RADIO_GetRadio() == RADIO_SI4732 && isSsb) {
        RADIO_TuneToSave(radio->rx.f + 5);
        return true;
      }
      break;
    case KEY_SIDE2:
      if (RADIO_GetRadio() == RADIO_SI4732 && isSsb) {
        RADIO_TuneToSave(radio->rx.f - 5);
        return true;
      }
      break;
    default:
      break;
    }
  }

  // long held
  if (bKeyHeld && bKeyPressed && !gRepeatHeld) {
    OffsetDirection offsetDirection = gCurrentPreset->offsetDir;
    switch (key) {
    case KEY_EXIT:
      prepareABScan();
      startScan();
      return true;
    case KEY_1:
      APPS_run(APP_PRESETS_LIST);
      return true;
    case KEY_2:
      gSettings.iAmPro = !gSettings.iAmPro;
      gVfo1ProMode = gSettings.iAmPro;
      SETTINGS_Save();
      return true;
    case KEY_3:
      RADIO_ToggleVfoMR();
      return true;
    case KEY_4: // freq catch
      if (RADIO_GetRadio() == RADIO_BK4819) {
        SVC_Toggle(SVC_FC, !SVC_Running(SVC_FC), 100);
      } else {
        gShowAllRSSI = !gShowAllRSSI;
      }
      return true;
    case KEY_5: // noaa
      SVC_Toggle(SVC_BEACON, !SVC_Running(SVC_BEACON), 15000);
      return true;
    case KEY_6:
      RADIO_ToggleTxPower();
      return true;
    case KEY_7:
      //RADIO_UpdateStep(true);
      //APPS_run(APP_LEVEL);
      return true;
    case KEY_8:
      IncDec8(&offsetDirection, 0, OFFSET_MINUS, 1);
      gCurrentPreset->offsetDir = offsetDirection;
      return true;
    case KEY_9: // call
      gTextInputSize = 15;
      gTextinputText = message;
      gTextInputCallback = sendDtmf;
      APPS_run(APP_TEXTINPUT);
      return true;
    case KEY_0:
      RADIO_ToggleModulation();
      return true;
    case KEY_STAR:
      if (RADIO_GetRadio() == RADIO_SI4732 && RADIO_IsSSB()) {
        SSB_Seek_ON = true;
        // todo: scan by snr
      } else {
        if (gSettings.crossBandScan && radio->channel <= -1) {
          selectFirstPresetFromScanlist();
        }
        startScan();
      }
      return true;
    case KEY_SIDE1:
      APPS_run(APP_ANALYZER);
      return true;
    case KEY_SIDE2:
      APPS_run(APP_SPECTRUM);
      return true;
    default:
      break;
    }
  }

  // Simple keypress
  if (!bKeyPressed && !bKeyHeld) {
    switch (key) {
    case KEY_0:
    case KEY_1:
    case KEY_2:
    case KEY_3:
    case KEY_4:
    case KEY_5:
    case KEY_6:
    case KEY_7:
    case KEY_8:
    case KEY_9:
      if (SVC_Running(SVC_SCAN)) {
        if (radio->channel <= -1) {
          if (gSettings.crossBandScan) {
            scanlistByKey(key);
            selectFirstPresetFromScanlist();
          } else {
            RADIO_SelectPresetSave(key + 6);
          }
        } else {
          scanlistByKey(key);
          initChannelScan();
          SETTINGS_DelayedSave();
        }
      } else {
        gFInputCallback = tuneTo;
        APPS_run(APP_FINPUT);
        APPS_key(key, bKeyPressed, bKeyHeld);
      }
      return true;
    case KEY_F:
      APPS_run(APP_VFO_CFG);
      return true;
    case KEY_STAR:
      APPS_run(APP_LOOT_LIST);
      return true;
    case KEY_SIDE1:
      if (SVC_Running(SVC_SCAN)) {
        LOOT_BlacklistLast();
        RADIO_NextFreqNoClicks(true);
        return true;
      }
      gMonitorMode = !gMonitorMode;
      return true;
    case KEY_SIDE2:
      if (SVC_Running(SVC_SCAN)) {
        LOOT_WhitelistLast();
        RADIO_NextFreqNoClicks(true);
        return true;
      }
      break;
    case KEY_EXIT:
      if (SSB_Seek_ON) {
        SSB_Seek_ON = false;
        return true;
      }
      if (SVC_Running(SVC_SCAN)) {
        SVC_Toggle(SVC_SCAN, false, 0);
        return true;
      }
      if (!APPS_exit()) {
        SVC_Toggle(SVC_SCAN, false, 0);
        LOOT_Standby();
        RADIO_NextVFO();
      }
      return true;
    default:
      break;
    }
  }
  return false;
}

static void DrawRegs(void) {
  RegisterSpec rs = registerSpecs[menuIndex];

  if (rs.num == BK4819_REG_13) {
    if (gCurrentPreset->band.gainIndex == AUTO_GAIN_INDEX) {
      sprintf(String, "auto");
    } else {
      sprintf(String, "%+ddB",
              -gainTable[gCurrentPreset->band.gainIndex].gainDb + 33);
    }
  } else if (rs.num == 0x73) {
    uint8_t afc = BK4819_GetAFC();
    if (afc) {
      sprintf(String, "%u", afc);
    } else {
      sprintf(String, onOff[0]);
    }
  } else {
    sprintf(String, "%u", BK4819_GetRegValue(rs));
  }

  PrintMedium(2, LCD_HEIGHT - 4, "%u. %s: %s", menuIndex, rs.name, String);
  if (registerActive) {
    FillRect(0, LCD_HEIGHT - 4 - 7, LCD_WIDTH, 9, C_INVERT);
  }
}

// Render VFO 1 
//---------------------------------------------------------------------------
void VFO1_render(void) 
  {  
    // Base Y-coordinate for rendering UI elements
    const uint8_t BASE = 42;  

    char name[10];
    const Loot *loot = &gLoot[gSettings.activeVFO];
    VFO *vfo = &gVFO[gSettings.activeVFO];           // Get pointer to the active VFO
    Preset *p = gVFOPresets[gSettings.activeVFO];    // Get pointer to the preset for the active VFO
    uint32_t rx_f = vfo->rx.f;                       // Retrieve the frequency for receiving
    uint32_t f = gTxState == TX_ON ? RADIO_GetTXF() : GetScreenF(rx_f);  // Determine the current frequency based on TX state

    uint16_t fp1 = f / MHZ;           // Frequency in MHz
    uint16_t fp2 = f / 100 % 1000;    // Frequency in kHz
    uint8_t fp3 = f % 100;            // Frequency in Hz
    ModulationType vfo_mod = vfo->modulation;        // Get the modulation type of the active VFO
    ModulationType band_mod = p->band.modulation;    // Get the modulation type of the current band
    const char *mod = modulationTypeOptions[vfo_mod == MOD_PRST ? band_mod : vfo_mod];    // Determine which modulation type to display
    uint8_t activeVFO = gSettings.activeVFO;         // Store the active VFO index

    // Update the status line based on user input or current preset
    if (gIsNumNavInput) 
      {
        // Display selected input if in number navigation mode
        STATUSLINE_SetText("Select: %s", gNumNavInput);  
      } 
    else 
      {
        // Display the current band name and channel number
        STATUSLINE_SetText( "%s:%u", 
                            gCurrentPreset->band.name,  
                            PRESETS_GetChannel(gCurrentPreset, rx_f) + 1 
                          );
      }
                                           


    // Check if the current VFO is in channel mode
    if (RADIO_VfoIsCH()) 
      {
        PrintMediumEx(/*LCD_WIDTH-23*/105, 28, POS_R, C_FILL, gVFONames[activeVFO]);  
      }
//    else if (vfo->tx.f)
//      {
//        PrintMediumEx( /*LCD_WIDTH-23*/105, 28, POS_R, C_FILL, 
//                      "TX %4u.%03u.%02u", 
//                      vfo->tx.f / MHZ, 
//                      vfo->tx.f / 100 % 1000, 
//                      vfo->tx.f % 100
//                    );
//      }
      
    switch(gTxState)
      {
        case TX_UNKNOWN:
          break;
          
        case TX_ON:
            FillRect(35, 8, 17, 9, C_FILL);                      // Fill a rectangle indicating TX mode
            PrintMediumBoldEx(36, 15, POS_L, C_INVERT, "TX");    // Display "TX" indicator    
            UI_TxBar(/*BASE+2*/45);              
          break;
      
        default:
            FillRect(0, /*BASE+3*/45, LCD_WIDTH, 9, C_FILL);
            PrintMediumBoldEx(40, /*BASE+10*/52, POS_L, C_INVERT, "%s", TX_STATE_NAMES[gTxState]);
            if (gSettings.errorBeep) 
              AUDIO_PlayTone(450, 100);
          break;  
      }
      
    // Display the frequency
    PrintBiggestDigitsEx(/*LCD_WIDTH-21*/107, /*BASE+1*/43, POS_R, C_FILL, "%4u.%03u", fp1, fp2);    // Display MHz and kHz
    PrintBigDigitsEx(LCD_WIDTH, /*BASE+1*/43, POS_R, C_FILL, "%02u", fp3);                           // Display Hz
        
    // Display the modulation type
    PrintMediumEx(/*LCD_WIDTH-2*/126, /*BASE-12*/30, POS_R, C_FILL, mod); 
    // Indicate if the current modulation differs from the band modulation
    if (vfo_mod != MOD_PRST && vfo_mod != band_mod) 
      {
        FillRect(/*LCD_WIDTH-20*/108, /*BASE-20*/23, 20, 9, C_INVERT); 
      }
      
    //SP_RenderGraph();
    
    
    
    if (gTxState == TX_UNKNOWN)
      {
        // Render the RSSI bar for the active VFO
        //UI_RSSIBar(gLoot[activeVFO].rssi, RADIO_GetSNR(), rx_f, /*BASE+3*/45);
      
        // Check if the device is listening
        if (gIsListening)
          {
            FillRect(35, 8, 17, 9, C_FILL);                      // Fill a rectangle indicating RX mode
            PrintMediumBoldEx(36, 15, POS_L, C_INVERT, "RX");    // Display "RX" indicator            
          }           
      }
  
    
    FillRect(0, 8, 25, 9, C_FILL);
    PrintMediumBoldEx(1, 15, POS_L, C_INVERT, "VA3");
    
    
      
    if (gSettings.scrambler=true)
      {
        FillRect(53, 8, 9, 9, C_FILL);                      // Fill a rectangle indicating scrambler mode
        PrintMediumBoldEx(54, 15, POS_L, C_INVERT, "$");    // Display "$" indicator      
      }
      
      

    if (gSettings.compander=true)
      {
        FillRect(63, 8, 9, 9, C_FILL);                      // Fill a rectangle indicating compander mode
        PrintMediumBoldEx(64, 15, POS_L, C_INVERT, "C");    // Display "C" indicator     
      }



    //if (vfo->rx.codeType)

    if (vfo->rx.codeType)
      {
        PrintSmallEx(92, 20, POS_R, C_FILL, "%c%c", TX_CODE_TYPES[vfo->rx.codeType][0], TX_CODE_TYPES[vfo->rx.codeType][1]);
      }

    if (vfo->tx.codeType)
      {      
        PrintSmallEx(105, 20, POS_R, C_FILL, "%c%c", TX_CODE_TYPES[vfo->tx.codeType][0], TX_CODE_TYPES[vfo->tx.codeType][1]);

      }
      
    if (loot->ct != 0xff)
      {
        PrintSmallEx(76, 13, POS_L, C_FILL, "%u.%u", CTCSS_Options[loot->ct] / 10, CTCSS_Options[loot->ct] % 10);      
      }
      
    if (loot->cd != 0xff && loot->ct == 0xff)
      {
        PrintSmallEx( 76, 13, POS_L, C_FILL, "D%03oN", DCS_Options[loot->cd]);
      }

   
      
    // Check if in pro mode for additional functionality
    if (gVfo1ProMode) 
      {    
        DrawRegs();    // Draw additional register information if in pro mode
      }
    else
      {
        // Fill a rectangle for VFO/channel A & B
        FillRect(0 + 44 * activeVFO, /*LCD_HEIGHT-9*/55, 43, 9, C_FILL);    
        for (uint8_t i = 0; i < 2; i++)
          {
            // Display channel number if it exists
            if (gVFO[i].channel >= 0) 
              {
                PrintMediumEx(1 + i * 45, 62, POS_L, C_INVERT, "CH-%04u", gVFO[i].channel + 1); 
              }
            else
              {
                // Otherwise display the VFO identifier
                PrintMediumEx(7 + i * 45, 62, POS_L, C_INVERT, "VFO-%c", 'A' + i); 
              }
        }
    }

    // Update the small frequency display at the bottom
    UI_FSmall(gTxState == TX_ON ? RADIO_GetTXF() : GetScreenF(rx_f)); 
  }

//---------------------------------------------------------------------------


















































