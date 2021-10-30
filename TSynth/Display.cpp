#include <TeensyThreads.h>
#include "Display.h"
#include "globals.h"
#include "Parameters.h"
#include "PatchMgr.h"
#include "TimbreMgr.h"
#include "Settings.h"

#define sclk 27
#define mosi 26
#define cs 2
#define dc 3
#define rst 9
#define DISPLAYTIMEOUT 700


ST7735_t3 tft = ST7735_t3(cs, dc, mosi, sclk, rst);

String currentParameter = "";
String currentValue = "";
float currentFloatValue = 0.0;
String currentPgmNum = "";
String currentPatchName = "";
String newPatchName = "";
const char * currentSettingsOption = "";
const char * currentSettingsValue = "";
uint32_t currentSettingsPart = SETTINGS;
uint32_t paramType = PARAMETER;

const char * currentMessageLine1 = "";
const char * currentMessageLine2 = "";

boolean MIDIClkSignal = false;
uint32_t peakCount = 0;
uint16_t prevLen = 0;

#define NUM_COLORS 5
uint32_t colourPriority[NUM_COLORS] = {ST7735_BLACK, ST7735_BLUE, ST7735_YELLOW, ST77XX_ORANGE, ST77XX_DARKRED};

unsigned long timer = 0;

void startTimer() {
  if (state == PARAMETER)
  {
    timer = millis();
  }
}

FLASHMEM void setMIDIClkSignal(bool val) {
  MIDIClkSignal = val;
}

FLASHMEM bool getMIDIClkSignal() {
  return MIDIClkSignal;
}

FLASHMEM void renderBootUpPage()
{
  tft.fillScreen(ST7735_BLACK);
  tft.drawRect(42, 30, 46, 11, ST7735_WHITE);
  tft.fillRect(88, 30, 61, 11, ST7735_WHITE);
  tft.setCursor(45, 31);
  tft.setFont(&Org_01);
  tft.setTextSize(1);
  tft.setTextColor(ST7735_WHITE);
  tft.println("ELECTRO");
  tft.setTextColor(ST7735_BLACK);
  tft.setCursor(91, 37);
  tft.println("TECHNIQUE");
  tft.setTextColor(ST7735_YELLOW);
  tft.setFont(&Yeysk16pt7b);
  tft.setCursor(5, 70);
  tft.setTextSize(1);
  tft.println("TSynth");
  tft.setTextColor(ST7735_RED);
  tft.setFont(&FreeSans9pt7b);
  tft.setCursor(110, 95);
  tft.println(VERSION);
}

FLASHMEM void renderPeak() {
  if (vuMeter && global.peak.available()) {
    uint16_t len = 0;
    if (peakCount > 1) {
      len = (int)(global.peak.read() * 75.0f);
      prevLen = len;
      peakCount = 0;
    } else {
      len = prevLen;
      peakCount++;
    }
    tft.drawFastVLine(158, 103 - len, len ,  len > 72 ? ST77XX_RED : ST77XX_GREEN);
    tft.drawFastVLine(159, 103 - len, len ,  len > 72 ? ST77XX_RED : ST77XX_GREEN);
  }
}

FLASHMEM void renderCurrentPatchPage() {
  tft.fillScreen(ST7735_BLACK);
  tft.setFont(&FreeSansBold18pt7b);
  tft.setCursor(5, 53);
  tft.setTextColor(ST7735_YELLOW);
  tft.setTextSize(1);
  tft.println(currentPgmNum);

  tft.setTextColor(ST7735_BLACK);
  tft.setFont(&Org_01);

  if (MIDIClkSignal) {
    tft.fillRect(100, 27, 14, 7, ST77XX_ORANGE);
    tft.setCursor(101, 32);
    tft.println("CK");
  }
  renderPeak();

  
  uint8_t fillColour[global.maxVoices()] = {};
  uint8_t borderColour[global.maxVoices()] = {};
  // Select colours based on voice state.
  uint8_t i = 0;
  for (uint8_t group = 0; group < groupvec.size(); group++) {
    auto g = groupvec[group];
    for (uint8_t voice = 0; voice  < g->size(); voice++) {
      borderColour[i] = group + 1;
      if ((*g)[voice]->on()) {
        // Start the fill color on the same color as the group, then
        // continue on for unison colors.
        fillColour[i] = (group + (*g)[voice]->noteId() + 1) % NUM_COLORS;
      }
      else fillColour[i] = 0;
      i++;
    }
  }

  // Draw rectangles to represent each voice.
  uint8_t max_rows = 3;
  uint8_t x_step = 10;
  uint8_t y_step = 10;
  uint8_t x_end = 147;
  uint8_t x_start = x_end - (x_step * ceil(global.maxVoices() / float(max_rows))) + x_step;
  uint8_t y_start = 27;
  uint8_t y_end = 47;
  uint8_t idx = 0;
  for (uint8_t y = y_start; y <= y_end; y += y_step) {
    for (uint8_t x = x_start; x <= x_end; x += x_step) {
      // Always draw border to indicate timbre.
      tft.fillRect(x, y, 8, 8, colourPriority[fillColour[idx]]);
      tft.drawRect(x, y, 8, 8, colourPriority[borderColour[idx]]);
      idx++;
      if (idx >= global.maxVoices()) {
        break;
      }
    }
  }

  tft.drawFastHLine(10, 63, tft.width() - 20, ST7735_RED);
  tft.setFont(&FreeSans12pt7b);
  tft.setTextColor(ST7735_YELLOW);
  tft.setCursor(1, 90);
  tft.setTextColor(ST7735_WHITE);
  tft.println(currentPatchName);
}

FLASHMEM void renderPulseWidth(float value) {
  tft.drawFastHLine(108, 74, 15 + (value * 13), ST7735_CYAN);
  tft.drawFastVLine(123 + (value * 13), 74, 20, ST7735_CYAN);
  tft.drawFastHLine(123 + (value * 13), 94, 16 - (value * 13), ST7735_CYAN);
  if (value < 0) {
    tft.drawFastVLine(108, 74, 21, ST7735_CYAN);
  } else {
    tft.drawFastVLine(138, 74, 21, ST7735_CYAN);
  }
}

FLASHMEM void renderVarTriangle(float value) {
  tft.drawLine(110, 94, 123 + (value * 13), 74, ST7735_CYAN);
  tft.drawLine(123 + (value * 13), 74, 136, 94, ST7735_CYAN);
}

FLASHMEM void renderEnv(float att, float dec, float sus, float rel) {
  tft.drawLine(100, 94, 100 + (att * 15), 74, ST7735_CYAN);
  tft.drawLine(100 + (att * 15), 74.0, 100 + ((att + dec) * 15), 94 - (sus * 20), ST7735_CYAN);
  tft.drawFastHLine(100 + ((att + dec) * 15), 94 - (sus * 20), 40 - ((att + dec) * 15), ST7735_CYAN);
  tft.drawLine(139, 94 - (sus * 20), 139 + (rel * 13), 94, ST7735_CYAN);
}

FLASHMEM void renderCurrentParameterPage() {
  switch (state) {
    case PARAMETER:
      tft.fillScreen(ST7735_BLACK);
      tft.setFont(&FreeSans12pt7b);
      tft.setCursor(0, 53);
      tft.setTextColor(ST7735_YELLOW);
      tft.setTextSize(1);
      tft.println(currentParameter);

      // Not a necessary feature perhaps
      //      if (midiOutCh > 0) {
      //        tft.setTextColor(ST77XX_ORANGE);
      //        tft.setFont(&Org_01);
      //        tft.setTextSize(2);
      //        tft.setCursor(140, 35);
      //        tft.println(midiOutCh);
      //        tft.setFont(&FreeSans12pt7b);
      //        tft.setTextSize(1);
      //      }
      renderPeak();
      tft.drawFastHLine(10, 63, tft.width() - 20, ST7735_RED);
      tft.setCursor(1, 90);
      tft.setTextColor(ST7735_WHITE);
      tft.println(currentValue);
      if (pickUpActive) {
        tft.fillCircle(150, 70, 5, ST77XX_DARKGREY);
        tft.drawFastHLine(146, 70, 4, ST7735_WHITE);
      }
      switch (paramType) {
        case PULSE:
          renderPulseWidth(currentFloatValue);
          break;
        case VAR_TRI:
          renderVarTriangle(currentFloatValue);
          break;
        case FILTER_ENV:
          renderEnv(groupvec[activeGroupIndex]->getFilterAttack() * 0.0001f, groupvec[activeGroupIndex]->getFilterDecay() * 0.0001f, groupvec[activeGroupIndex]->getFilterSustain(), groupvec[activeGroupIndex]->getFilterRelease() * 0.0001f);
          break;
        case AMP_ENV:
          renderEnv(groupvec[activeGroupIndex]->getAmpAttack() * 0.0001f, groupvec[activeGroupIndex]->getAmpDecay() * 0.0001f, groupvec[activeGroupIndex]->getAmpSustain(), groupvec[activeGroupIndex]->getAmpRelease() * 0.0001f);
          break;
      }
      break;
  }
}

FLASHMEM void renderSelectPage(String question, String num1, String name1, String num2, String name2) {
  tft.fillScreen(ST7735_BLACK);
  tft.setFont(&FreeSansBold18pt7b);
  tft.setCursor(5, 53);
  tft.setTextColor(ST7735_YELLOW);
  tft.setTextSize(1);
  tft.println(question);
  tft.drawFastHLine(10, 60, tft.width() - 20, ST7735_RED);
  tft.setFont(&FreeSans9pt7b);
  tft.setCursor(0, 78);
  tft.setTextColor(ST7735_YELLOW);
  tft.println(num1);
  tft.setCursor(35, 78);
  tft.setTextColor(ST7735_WHITE);
  tft.println(name1);
  tft.fillRect(0, 85, tft.width(), 23, ST77XX_DARKRED);
  tft.setCursor(0, 98);
  tft.setTextColor(ST7735_YELLOW);
  tft.println(num2);
  tft.setCursor(35, 98);
  tft.setTextColor(ST7735_WHITE);
  tft.println(name2);
}

FLASHMEM void renderMessagePage() {
  tft.fillScreen(ST7735_BLACK);
  tft.setFont(&FreeSans12pt7b);
  tft.setCursor(2, 53);
  tft.setTextColor(ST7735_YELLOW);
  tft.setTextSize(1);
  tft.println(currentMessageLine1);
  tft.setCursor(10, 90);
  tft.println(currentMessageLine2);
}

FLASHMEM void renderSavePage() {
  tft.fillScreen(ST7735_BLACK);
  tft.setFont(&FreeSansBold18pt7b);
  tft.setCursor(5, 53);
  tft.setTextColor(ST7735_YELLOW);
  tft.setTextSize(1);
  tft.println("Save?");
  tft.drawFastHLine(10, 60, tft.width() - 20, ST7735_RED);
  tft.setFont(&FreeSans9pt7b);
  tft.setCursor(0, 78);
  tft.setTextColor(ST7735_YELLOW);
  tft.println(patches[patches.size() - 2].patchNo);
  tft.setCursor(35, 78);
  tft.setTextColor(ST7735_WHITE);
  tft.println(patches[patches.size() - 2].patchName);
  tft.fillRect(0, 85, tft.width(), 23, ST77XX_DARKRED);
  tft.setCursor(0, 98);
  tft.setTextColor(ST7735_YELLOW);
  tft.println(patches.last().patchNo);
  tft.setCursor(35, 98);
  tft.setTextColor(ST7735_WHITE);
  tft.println(patches.last().patchName);
}

FLASHMEM void renderReinitialisePage()
{
  tft.fillScreen(ST7735_BLACK);
  tft.setFont(&FreeSans12pt7b);
  tft.setTextColor(ST7735_YELLOW);
  tft.setTextSize(1);
  tft.setCursor(5, 53);
  tft.println("Initialise to");
  tft.setCursor(5, 90);
  tft.println("panel setting");
}

FLASHMEM void renderPatchNamingPage()
{
  tft.fillScreen(ST7735_BLACK);
  tft.setFont(&FreeSans12pt7b);
  tft.setTextColor(ST7735_YELLOW);
  tft.setTextSize(1);
  tft.setCursor(0, 53);
  tft.println("Rename Patch");
  tft.drawFastHLine(10, 63, tft.width() - 20, ST7735_RED);
  tft.setTextColor(ST7735_WHITE);
  tft.setCursor(5, 90);
  tft.println(newPatchName);
}

FLASHMEM void renderChooserPage(String id1, String field1, String id2, String field2, String id3, String field3)
{
  tft.fillScreen(ST7735_BLACK);
  tft.setFont(&FreeSans9pt7b);
  tft.setCursor(0, 45);
  tft.setTextColor(ST7735_YELLOW);
  tft.println(id1);
  tft.setCursor(35, 45);
  tft.setTextColor(ST7735_WHITE);
  tft.println(field1);

  tft.fillRect(0, 56, tft.width(), 23, 0xA000);
  tft.setCursor(0, 72);
  tft.setTextColor(ST7735_YELLOW);
  tft.println(id2);
  tft.setCursor(35, 72);
  tft.setTextColor(ST7735_WHITE);
  tft.println(field2);

  tft.setCursor(0, 98);
  tft.setTextColor(ST7735_YELLOW);
  tft.println(id3);
  tft.setCursor(35, 98);
  tft.setTextColor(ST7735_WHITE);
  //patches.size() > 1 ? tft.println(patches[1].patchName) : tft.println(patches.last().patchName);
  tft.println(field3);
}

FLASHMEM void showRenamingPage(String newName) {
  newPatchName = newName;
}

FLASHMEM void renderUpDown(uint16_t  x, uint16_t  y, uint16_t  colour) {
  //Produces up/down indicator glyph at x,y
  tft.setCursor(x, y);
  tft.fillTriangle(x, y, x + 8, y - 8, x + 16, y, colour);
  tft.fillTriangle(x, y + 4, x + 8, y + 12, x + 16, y + 4, colour);
}

FLASHMEM void renderSettingsPage() {
  tft.fillScreen(ST7735_BLACK);
  tft.setFont(&FreeSans12pt7b);
  tft.setTextColor(ST7735_YELLOW);
  tft.setTextSize(1);
  tft.setCursor(0, 53);
  tft.println(currentSettingsOption);
  if (currentSettingsPart == SETTINGS) renderUpDown(140, 42, ST7735_YELLOW);
  tft.drawFastHLine(10, 63, tft.width() - 20, ST7735_RED);
  tft.setTextColor(ST7735_WHITE);
  tft.setCursor(5, 90);
  tft.println(currentSettingsValue);
  if (currentSettingsPart == SETTINGSVALUE) renderUpDown(140, 80, ST7735_WHITE);
}

FLASHMEM void showMessage(const char *line1, const char *line2){
  currentMessageLine1 = line1;
  currentMessageLine2 = line2;
}

FLASHMEM void showCurrentParameterPage(const char *param, float val, int pType) {
  currentParameter = param;
  currentValue = String(val);
  currentFloatValue = val;
  paramType = pType;
  startTimer();
}

FLASHMEM void showCurrentParameterPage(const char *param, String val, int pType) {
  if (state == SETTINGS || state == SETTINGSVALUE)state = PARAMETER;//Exit settings page if showing
  currentParameter = param;
  currentValue = val;
  paramType = pType;
  startTimer();
}

FLASHMEM void showCurrentParameterPage(const char *param, String val) {
  showCurrentParameterPage(param, val, PARAMETER);
}

FLASHMEM void showPatchPage(String number, String patchName) {
  currentPgmNum = number;
  currentPatchName = patchName;
}

FLASHMEM void showSettingsPage(const char *  option, const char * value, int settingsPart) {
  Serial.println(option);
  Serial.println(value);
  Serial.println(settingsPart);
  currentSettingsOption = option;
  currentSettingsValue = value;
  currentSettingsPart = settingsPart;
}

FLASHMEM void enableScope(boolean enable) {
  enable ? global.scope.ScreenSetup(&tft) : global.scope.ScreenSetup(NULL);
}

void displayThread() {
  threads.delay(2000); //Give bootup page chance to display
  while (1) {
    switch (state) {
      case PARAMETER:
        if ((millis() - timer) > DISPLAYTIMEOUT) {
          pickUpActive = false;
          renderCurrentPatchPage();
        } else {
          pickUpActive = pickUp;
          renderCurrentParameterPage();
        }
        break;
      case RECALL:
        renderChooserPage(
          patches.last().patchNo,
          patches.last().patchName,
          patches.first().patchNo,
          patches.first().patchName,
          patches.size() > 1 ? patches[1].patchNo : patches.last().patchNo,
          patches.size() > 1 ? patches[1].patchName : patches.last().patchName
        );
        break;
      case MT_PROFILE_LIST:
        renderChooserPage(
          timbreProfiles.last().profileNo,
          timbreProfiles.last().profileName,
          timbreProfiles.first().profileNo,
          timbreProfiles.first().profileName,
          timbreProfiles.size() > 1 ? timbreProfiles[1].profileNo : timbreProfiles.last().profileNo,
          timbreProfiles.size() > 1 ? timbreProfiles[1].profileName : timbreProfiles.last().profileName
        );
        break;
      case MT_TIMBRE_LIST:
        if (timbres.size() == 1) {
          renderChooserPage(
            "",
            "",
            timbres.first().timbreVoiceGroupIdx,
            timbres.first().timbreName,
            "",
            ""
          );
        }
        renderChooserPage(
          timbres.last().timbreVoiceGroupIdx,
          timbres.last().timbreName,
          timbres.first().timbreVoiceGroupIdx,
          timbres.first().timbreName,
          timbres.size() > 1 ? timbres[1].timbreVoiceGroupIdx : timbres.last().timbreVoiceGroupIdx,
          timbres.size() > 1 ? timbres[1].timbreName : timbres.last().timbreName
        );
        break;
      case SAVE:
        renderSavePage();
        break;
      case REINITIALISE:
        renderReinitialisePage();
        tft.updateScreen(); //update before delay
        threads.delay(1000);
        state = PARAMETER;
        break;
      case PATCHNAMING:
        renderPatchNamingPage();
        break;
      case PATCH:
        renderCurrentPatchPage();
        break;
      case MT_TIMBRE_ADD_SELECT:
        renderSelectPage(
          "Timbre?",
          patches.last().patchNo,
          patches.last().patchName,
          patches.first().patchNo,
          patches.first().patchName);
        break;
      case DELETE:
        renderSelectPage(
          "Delete?",
          patches.last().patchNo,
          patches.last().patchName,
          patches.first().patchNo,
          patches.first().patchName);
        break;
      case DELETE_MT_PROFILE:
        renderSelectPage(
          "Delete?",
          timbreProfiles.last().profileNo,
          timbreProfiles.last().profileName,
          timbreProfiles.first().profileNo,
          timbreProfiles.first().profileName);
        break;
      case DELETE_MT_TIMBRE:
        renderSelectPage(
          "Delete?",
          timbres.last().timbreVoiceGroupIdx,
          timbres.last().timbreName,
          timbres.first().timbreVoiceGroupIdx,
          timbres.first().timbreName);
        break;
      case MESSAGE:
        renderMessagePage();
        break;
      case MT_TIMBRE_SETTINGS:
      case MT_TIMBRE_SETTINGS_VALUE:
      case SETTINGS:
      case SETTINGSVALUE:
        renderSettingsPage();
        break;
    }
    tft.updateScreen();
  }
}

void setupDisplay() {
  tft.initR(INITR_GREENTAB);
  tft.useFrameBuffer(true);
  tft.setRotation(3);
  tft.invertDisplay(true);
  renderBootUpPage();
  tft.updateScreen();
  threads.addThread(displayThread);
}