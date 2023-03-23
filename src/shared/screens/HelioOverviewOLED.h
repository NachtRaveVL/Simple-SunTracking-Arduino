/*  Helioduino: Simple automation controller for solar tracking systems.
    Copyright (C) 2023 NachtRaveVL          <nachtravevl@gmail.com>
    Helioduino U8g2 OLED Overview Screen
*/

#include <Helioduino.h>
#ifdef HELIO_USE_GUI
#ifndef HelioOverviewOLED_H
#define HelioOverviewOLED_H

class HelioOverviewOLED;

#include "../HelioduinoUI.h"

// OLED Overview Screen
// Overview screen built for OLED displays.
class HelioOverviewOLED : public HelioOverview {
public:
    HelioOverviewOLED(HelioDisplayU8g2OLED *display);
    virtual ~HelioOverviewOLED();

    virtual void renderOverview(bool isLandscape, Pair<uint16_t, uint16_t> screenSize) override;

protected:
    U8G2 &_gfx;                                             // Graphics (strong)
    #ifdef HELIO_UI_ENABLE_STM32_LDTC
        StChromaArtDrawable &_drawable;                     // Drawable (strong)
    #else
        U8g2Drawable &_drawable;                            // Drawable (strong)
    #endif
};

#endif // /ifndef HelioOverviewOLED_H
#endif
