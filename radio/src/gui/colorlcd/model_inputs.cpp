/*
 * Copyright (C) OpenTX
 *
 * Based on code named
 *   th9x - http://code.google.com/p/th9x
 *   er9x - http://code.google.com/p/er9x
 *   gruvin9x - http://code.google.com/p/gruvin9x
 *
 * License GPLv2: http://www.gnu.org/licenses/gpl-2.0.html
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include "model_inputs.h"
#include "opentx.h"

#define SET_DIRTY() storageDirty(EE_MODEL)

#define PASTE_BEFORE    -2
#define PASTE_AFTER     -1

uint8_t getExposCount()
{
  uint8_t count = 0;
  uint8_t ch ;

  for (int i=MAX_EXPOS-1 ; i>=0; i--) {
    ch = EXPO_VALID(expoAddress(i));
    if (ch != 0) {
      count++;
    }
  }
  return count;
}

bool reachExposLimit()
{
  if (getExposCount() >= MAX_EXPOS) {
    POPUP_WARNING(STR_NOFREEEXPO);
    return true;
  }
  return false;
}

void copyExpo(uint8_t source, uint8_t dest, int8_t input)
{
  pauseMixerCalculations();
  ExpoData sourceExpo;
  memcpy(&sourceExpo, expoAddress(source), sizeof(ExpoData));
  ExpoData * expo = expoAddress(dest);

  if(input == PASTE_AFTER) {
    memmove(expo+2, expo+1, (MAX_EXPOS-(source+1))*sizeof(ExpoData));
    memcpy(expo+1, &sourceExpo, sizeof(ExpoData));
    (expo+1)->chn = (expo)->chn;
  }
  else if(input == PASTE_BEFORE) {
    memmove(expo+1, expo, (MAX_EXPOS-(source+1))*sizeof(ExpoData));
    memcpy(expo, &sourceExpo, sizeof(ExpoData));
    expo->chn = (expo+1)->chn;
  }
  else {
    memmove(expo+1, expo, (MAX_EXPOS-(source+1))*sizeof(ExpoData));
    memcpy(expo, &sourceExpo, sizeof(ExpoData));
    expo->chn = input;
  }
  resumeMixerCalculations();
  storageDirty(EE_MODEL);
}

void deleteExpo(uint8_t idx)
{
  pauseMixerCalculations();
  ExpoData * expo = expoAddress(idx);
  int input = expo->chn;
  memmove(expo, expo+1, (MAX_EXPOS-(idx+1))*sizeof(ExpoData));
  memclear(&g_model.expoData[MAX_EXPOS-1], sizeof(ExpoData));
  if (!isInputAvailable(input)) {
    memclear(&g_model.inputNames[input], LEN_INPUT_NAME);
  }
  resumeMixerCalculations();
  storageDirty(EE_MODEL);
}

class InputEditWindow: public Page {
  public:
    InputEditWindow(int8_t input, uint8_t index):
      Page(ICON_MODEL_INPUTS),
      input(input),
      index(index),
      preview(this, {LCD_W - 158, 0, 158, 158},
              [=](int x) -> int {
                ExpoData * line = expoAddress(index);
                int16_t anas[MAX_INPUTS] = {0};
                applyExpos(anas, e_perout_mode_inactive_flight_mode, line->srcRaw, x);
                return anas[line->chn];
              },
              [=]() -> int {
                return getValue(expoAddress(index)->srcRaw);
              })
    {
      buildBody(&body);
      buildHeader(&header);
    }

  protected:
    uint8_t input;
    uint8_t index;
    Curve preview;
    Choice * trimChoice = nullptr;
    Window * updateCurvesWindow = nullptr;
    Choice * curveTypeChoice = nullptr;

    void buildHeader(Window * window) {
      new StaticText(window, { 50, 0, 100, 20 }, STR_MENUINPUTS, MENU_TITLE_COLOR);
      new StaticText(window, { 50, FH+2, 100, 20 }, getSourceString(MIXSRC_FIRST_INPUT + input), MENU_TITLE_COLOR);
    }

    void updateCurves() {
      FormGridLayout grid;
      grid.setLabelWidth(120);

      updateCurvesWindow->clear();

      ExpoData * line = expoAddress(index) ;

      new StaticText(updateCurvesWindow, grid.getLabelSlot(), STR_CURVE);
      curveTypeChoice = new Choice(updateCurvesWindow, grid.getFieldSlot(2, 0), "\004DiffExpoFuncCstm", 0, CURVE_REF_CUSTOM,
                                   GET_DEFAULT(line->curve.type),
                                   [=](int32_t newValue) {
                                     line->curve.type = newValue;
                                     line->curve.value = 0;
                                     SET_DIRTY();
                                     updateCurves();
                                     curveTypeChoice->setFocus();
                                   });

      switch (line->curve.type) {
        case CURVE_REF_DIFF:
        case CURVE_REF_EXPO: {
          // TODO GVAR
          NumberEdit * edit = new NumberEdit(updateCurvesWindow, grid.getFieldSlot(2, 1), -100, 100,
                                             GET_SET_DEFAULT(line->curve.value));
          edit->setSuffix("%");
          break;
        }
        case CURVE_REF_FUNC:
          new Choice(updateCurvesWindow, grid.getFieldSlot(2, 1), STR_VCURVEFUNC, 0, CURVE_BASE-1, GET_SET_DEFAULT(line->curve.value));
          break;
        case CURVE_REF_CUSTOM:
          #warning "TODO Custom Curve Choice"
          // new CustomCurveChoice(updateCurvesWindow, grid.getFieldSlot(2, 1), -MAX_CURVES, MAX_CURVES, GET_SET_DEFAULT(line->curve.value));
          break;
      }
    }

    void buildBody(FormWindow * window)
    {
      NumberEdit * edit;

      FormGridLayout grid;
      grid.setLabelWidth(120);
      grid.spacer(PAGE_PADDING);

      ExpoData * line = expoAddress(index) ;

      grid.setMarginRight(163);

      // Input Name
      new StaticText(window, grid.getLabelSlot(), STR_INPUTNAME);
      auto name = new TextEdit(window, grid.getFieldSlot(), g_model.inputNames[line->chn], sizeof(g_model.inputNames[line->chn]));
      window->setFirstField(name);
      grid.nextLine();

      // Switch
      new StaticText(window, grid.getLabelSlot(), STR_SWITCH);
      new SwitchChoice(window, grid.getFieldSlot(), SWSRC_FIRST_IN_MIXES, SWSRC_LAST_IN_MIXES, GET_SET_DEFAULT(line->swtch));
      grid.nextLine();

      // Side
      new StaticText(window, grid.getLabelSlot(), STR_SIDE);
      new Choice(window, grid.getFieldSlot(), STR_VCURVEFUNC, 1, 3,
                 [=]() -> int16_t {
                   return 4 - line->mode;
                 },
                 [=](int16_t newValue) {
                   line->mode = 4 - newValue;
                   SET_DIRTY();
                 });
      grid.nextLine();

      // Name
      new StaticText(window, grid.getLabelSlot(), STR_EXPONAME);
      new TextEdit(window, grid.getFieldSlot(), line->name, sizeof(line->name));
      grid.nextLine();

      // Source
      new StaticText(window, grid.getLabelSlot(), STR_SOURCE);
      new SourceChoice(window, grid.getFieldSlot(), INPUTSRC_FIRST, INPUTSRC_LAST,
                       GET_DEFAULT(line->srcRaw),
                       [=] (int32_t newValue) {
                         line->srcRaw = newValue;
                         if (line->srcRaw > MIXSRC_Ail && line->carryTrim == TRIM_ON) {
                           line->carryTrim = TRIM_OFF;
                           trimChoice->invalidate();
                         }
                         SET_DIRTY();
                       }
      );
      /* TODO telemetry current value
      if (ed->srcRaw >= MIXSRC_FIRST_TELEM) {
        drawSensorCustomValue(EXPO_ONE_2ND_COLUMN+75, y, (ed->srcRaw - MIXSRC_FIRST_TELEM)/3, convertTelemValue(ed->srcRaw - MIXSRC_FIRST_TELEM + 1, ed->scale), LEFT|(menuHorizontalPosition==1?attr:0));
      } */
      grid.nextLine();

      // Scale
      // TODO only displayed when source is telemetry + unfinished
      new StaticText(window, grid.getLabelSlot(), STR_SCALE);
      new NumberEdit(window, grid.getFieldSlot(), -100, 100, GET_SET_DEFAULT(line->scale));
      grid.nextLine();

      // Weight
      new StaticText(window, grid.getLabelSlot(), STR_WEIGHT);
      // TODO GVAR ?
      edit = new NumberEdit(window, grid.getFieldSlot(), -100, 100, GET_SET_DEFAULT(line->weight));
      edit->setSuffix("%");
      grid.nextLine();

      // Offset
      new StaticText(window, grid.getLabelSlot(), STR_OFFSET);
      edit = new NumberEdit(window, grid.getFieldSlot(), -100, 100, GET_SET_DEFAULT(line->offset));
      edit->setSuffix("%");
      grid.nextLine();

      // Trim
      new StaticText(window, grid.getLabelSlot(), STR_TRIM);
      trimChoice = new Choice(window, grid.getFieldSlot(), STR_VMIXTRIMS, -TRIM_OFF, -TRIM_LAST,
                              GET_VALUE(-line->carryTrim),
                              SET_VALUE(line->carryTrim, -newValue));
      trimChoice->setAvailableHandler([=](int value) {
        return value != TRIM_ON || line->srcRaw <= MIXSRC_Ail;
      });
      grid.nextLine();

      // grid.setMarginRight(10);

      // Curve
      updateCurvesWindow = new Window(window, { 0, grid.getWindowHeight(), LCD_W - 162, 0 });
      updateCurves();
      grid.addWindow(updateCurvesWindow);

      // Flight modes
      new StaticText(window, grid.getLabelSlot(), STR_FLMODE);
      TextButton * flightmode = nullptr;
      for (uint8_t i=0; i<MAX_FLIGHT_MODES; i++) {
        char fm[2] = { char('0' + i), '\0'};
        if (i > 0 && (i % 4) == 0)
          grid.nextLine();
        flightmode = new TextButton(window, grid.getFieldSlot(4, i % 4), fm,
                                    [=]() -> uint8_t {
                                        BFBIT_FLIP(line->flightModes, bfBit<uint32_t>(i));
                                        SET_DIRTY();
                                        return !(bfSingleBitGet(line->flightModes, i));
                                    },
                                    bfSingleBitGet(line->flightModes, i) ? 0 : BUTTON_CHECKED);
      }
      grid.nextLine();
      window->setLastField(flightmode);
      window->setInnerHeight(grid.getWindowHeight());
    }
};

void CommonInputOrMixButton::checkEvents()
{
  if (active != isActive()) {
    invalidate();
    active = !active;
  }

  Button::checkEvents();
}

void CommonInputOrMixButton::drawFlightModes(BitmapBuffer *dc, FlightModesType value)
{
  dc->drawBitmap(146, PAGE_LINE_HEIGHT + FIELD_PADDING_TOP, mixerSetupFlightmodeBitmap);
  coord_t x = 166;
  for (int i = 0; i < MAX_FLIGHT_MODES; i++) {
    char s[] = " ";
    s[0] = '0' + i;
    if (value & (1 << i)) {
      dc->drawText(x, PAGE_LINE_HEIGHT + FIELD_PADDING_TOP, s, SMLSIZE | TEXT_DISABLE_COLOR);
    }
    else {
      dc->drawSolidFilledRect(x, PAGE_LINE_HEIGHT + FIELD_PADDING_TOP, 8, 3, SCROLLBOX_COLOR);
      dc->drawText(x, PAGE_LINE_HEIGHT + FIELD_PADDING_TOP, s, SMLSIZE);
    }
    x += 8;
  }
}

void CommonInputOrMixButton::paint(BitmapBuffer * dc)
{
  if (active)
    dc->drawSolidFilledRect(2, 2, rect.w - 4, rect.h - 4, WARNING_COLOR);
  paintBody(dc);
  drawSolidRect(dc, 0, 0, rect.w, rect.h, 2, hasFocus() ? SCROLLBOX_COLOR : CURVE_AXIS_COLOR);
}

class InputLineButton : public CommonInputOrMixButton {
  public:
    InputLineButton(Window * parent, const rect_t & rect, uint8_t index):
      CommonInputOrMixButton(parent, rect, index)
    {
      const ExpoData & line = g_model.expoData[index];
      if (line.swtch || line.curve.value != 0 || line.flightModes) {
        setHeight(getHeight() + 20);
      }
    }

    bool isActive() override
    {
      return isExpoActive(index);
    }

    void paintBody(BitmapBuffer * dc) override
    {
      const ExpoData & line = g_model.expoData[index];

      // first line ...
      drawValueOrGVar(dc, FIELD_PADDING_LEFT, FIELD_PADDING_TOP, line.weight);
      drawSource(dc, 60, FIELD_PADDING_TOP, line.srcRaw);

      if (line.name[0]) {
        dc->drawBitmap(146, 2 + FIELD_PADDING_TOP, mixerSetupLabelBitmap);
        dc->drawSizedText(166, FIELD_PADDING_TOP, line.name, sizeof(line.name), ZCHAR);
      }

      // second line ...
      if (line.swtch) {
        dc->drawMask(3, PAGE_LINE_HEIGHT + FIELD_PADDING_TOP, mixerSetupSwitchIcon, TEXT_COLOR);
        drawSwitch(dc, 21, PAGE_LINE_HEIGHT + FIELD_PADDING_TOP, line.swtch);
      }

      if (line.curve.value != 0 ) {
        dc->drawBitmap(60, PAGE_LINE_HEIGHT + FIELD_PADDING_TOP, mixerSetupCurveBitmap);
        drawCurveRef(dc, 80, PAGE_LINE_HEIGHT + FIELD_PADDING_TOP, line.curve);
      }

      if (line.flightModes) {
        drawFlightModes(dc, line.flightModes);
      }
    }
};

ModelInputsPage::ModelInputsPage():
  PageTab(STR_MENUINPUTS, ICON_MODEL_INPUTS)
{
}

void ModelInputsPage::rebuild(FormWindow * window, int8_t focusIndex)
{
  coord_t scrollPosition = window->getScrollPositionY();
  window->clear();
  build(window, focusIndex);
  window->setScrollPositionY(scrollPosition);
}

void ModelInputsPage::editInput(FormWindow * window, uint8_t input, uint8_t index)
{
  Window * editWindow = new InputEditWindow(input, index);
  editWindow->setCloseHandler([=]() {
    rebuild(window, index);
  });
}

void ModelInputsPage::build(FormWindow * window, int8_t focusIndex)
{
  FormGridLayout grid;
  grid.spacer(PAGE_PADDING);
  grid.setLabelWidth(66);

  Window::clearFocus();

  int inputIndex = 0;
  ExpoData * line = g_model.expoData;
  for (uint8_t input=0; input<MAX_INPUTS; input++) {
    if (inputIndex < MAX_EXPOS && line->chn == input && EXPO_VALID(line)) {
      new StaticText(window, grid.getLabelSlot(), getSourceString(MIXSRC_FIRST_INPUT + input), BUTTON_BACKGROUND | CENTERED);
      while (inputIndex < MAX_EXPOS && line->chn == input && EXPO_VALID(line)) {
        Button * button = new InputLineButton(window, grid.getFieldSlot(), inputIndex);
        if (focusIndex == inputIndex)
          button->setFocus();
        button->setPressHandler([=]() -> uint8_t {
          button->bringToTop();
          Menu * menu = new Menu();
          menu->addLine(STR_EDIT, [=]() {
            editInput(window, input, inputIndex);
          });
          if (!reachExposLimit()) {
            menu->addLine(STR_INSERT_BEFORE, [=]() {
              insertExpo(inputIndex, input);
              editInput(window, input, inputIndex);
            });
            menu->addLine(STR_INSERT_AFTER, [=]() {
              insertExpo(inputIndex + 1, input);
              editInput(window, input, inputIndex + 1);
            });
            menu->addLine(STR_COPY, [=]() {
              s_copyMode = COPY_MODE;
              s_copySrcIdx = inputIndex;
            });
            if (s_copyMode != 0) {
              menu->addLine(STR_PASTE_BEFORE, [=]() {
                copyExpo(s_copySrcIdx, inputIndex, PASTE_BEFORE);
                if (s_copyMode == MOVE_MODE) {
                  deleteExpo((s_copySrcIdx > inputIndex) ? s_copySrcIdx+1 : s_copySrcIdx);
                  s_copyMode = 0;
                }
                rebuild(window, inputIndex);
              });
              menu->addLine(STR_PASTE_AFTER, [=]() {
                copyExpo(s_copySrcIdx, inputIndex, PASTE_AFTER);
                if (s_copyMode == MOVE_MODE) {
                  deleteExpo((s_copySrcIdx > inputIndex) ? s_copySrcIdx+1 : s_copySrcIdx);
                  s_copyMode = 0;
                }
                rebuild(window, inputIndex+1);
              });
            }
          }
          menu->addLine(STR_MOVE, [=]() {
            s_copyMode = MOVE_MODE;
            s_copySrcIdx = inputIndex;
          });
          menu->addLine(STR_DELETE, [=]() {
            deleteExpo(inputIndex);
            rebuild(window, -1);
          });
          return 0;
        });

        grid.spacer(button->height() - 2);

        ++inputIndex;
        ++line;
      }

      grid.spacer(7);
    }
    else {
      auto button = new TextButton(window, grid.getLabelSlot(), getSourceString(MIXSRC_FIRST_INPUT + input));
      if (focusIndex == inputIndex)
        button->setFocus();
      button->setPressHandler([=]() -> uint8_t {
        button->bringToTop();
        Menu * menu = new Menu();
        menu->addLine(STR_EDIT, [=]() {
          insertExpo(inputIndex, input);
          editInput(window, input, inputIndex);
          return 0;
        });
        if (!reachExposLimit()) {
          if (s_copyMode != 0) {
            menu->addLine(STR_PASTE, [=]() {
              copyExpo(s_copySrcIdx, inputIndex, input);
              if(s_copyMode == MOVE_MODE) {
                deleteExpo((s_copySrcIdx >= inputIndex) ? s_copySrcIdx+1 : s_copySrcIdx);
                s_copyMode = 0;
              }
              rebuild(window, -1);
              return 0;
            });
          }
        }
        return 0;
      });

      grid.spacer(button->height() + 5);
    }
  }

  Window * focus = Window::getFocus();
  if (focus) {
    focus->bringToTop();
  }

  grid.nextLine();

  window->setLastField();
  window->setInnerHeight(grid.getWindowHeight());
}

// TODO port: avoid global s_currCh on ARM boards (as done here)...
int8_t s_currCh;
uint8_t s_copyMode;
int8_t s_copySrcRow;

void insertExpo(uint8_t idx, uint8_t input)
{
  pauseMixerCalculations();
  ExpoData * expo = expoAddress(idx);
  memmove(expo+1, expo, (MAX_EXPOS-(idx+1))*sizeof(ExpoData));
  memclear(expo, sizeof(ExpoData));
  expo->srcRaw = (input >= 4 ? MIXSRC_Rud + input : MIXSRC_Rud + channelOrder(input + 1) - 1);
  expo->curve.type = CURVE_REF_EXPO;
  expo->mode = 3; // pos+neg
  expo->chn = input;
  expo->weight = 100;
  resumeMixerCalculations();
  storageDirty(EE_MODEL);
}