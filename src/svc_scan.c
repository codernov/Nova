#include "svc_scan.h"
#include "radio.h"
#include "scheduler.h"
#include "settings.h"

uint16_t gScanSwitchT = 10;
bool gScanForward = true;

uint32_t SCAN_TIMEOUTS[9] = {
    1000 * 1,  1000 * 2,      1000 * 5,      1000 * 10,         1000 * 30,
    1000 * 60, 1000 * 60 * 2, 1000 * 60 * 5, ((uint32_t)0) - 1,
};

char *SCAN_TIMEOUT_NAMES[9] = {
    "1s", "2s", "5s", "10s", "30s", "1min", "2min", "5min", "None",
};

void (*gScanFn)(bool) = NULL;

static uint32_t timeout = 0;
static bool lastListenState = false;

void SVC_SCAN_Init(void) {
  gScanForward = true;
  if (!gScanFn) {
    gScanFn = RADIO_NextPresetFreq;
  }
  gScanFn(gScanForward);
  SetTimeout(&timeout, gScanSwitchT);
}

void SVC_SCAN_Update(void) {
  RADIO_UpdateMeasurements();
  if (lastListenState != gIsListening) {
    lastListenState = gIsListening;
    SetTimeout(&timeout, gIsListening
                             ? SCAN_TIMEOUTS[gSettings.sqOpenedTimeout]
                             : SCAN_TIMEOUTS[gSettings.sqClosedTimeout]);
  }

  if (CheckTimeout(&timeout)) {
    gScanFn(gScanForward);
    SetTimeout(&timeout, gScanSwitchT);
    lastListenState = false;
  }
}

void SVC_SCAN_Deinit(void) {
  // TODO: restore some freq
  gScanFn = NULL; // to make simple scan on start
}
