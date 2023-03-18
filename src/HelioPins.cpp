/*  Helioduino: Simple automation controller for solar tracking systems.
    Copyright (C) 2023 NachtRaveVL          <nachtravevl@gmail.com>
    Helioduino Pins
*/

#include "Helioduino.h"
#include "AnalogDeviceAbstraction.h"

HelioPin *newPinObjectFromSubData(const HelioPinData *dataIn)
{
    if (!dataIn || !isValidType(dataIn->type)) return nullptr;
    HELIO_SOFT_ASSERT(dataIn && isValidType(dataIn->type), SFP(HStr_Err_InvalidParameter));

    if (dataIn) {
        switch (dataIn->type) {
            case HelioPin::Digital:
                return new HelioDigitalPin(dataIn);
            case HelioPin::Analog:
                return new HelioAnalogPin(dataIn);
            default: break;
        }
    }

    return nullptr;
}

HelioPin::HelioPin()
    : type(Unknown), pin((pintype_t)-1), mode(Helio_PinMode_Undefined), channel(-1)
{ ; }

HelioPin::HelioPin(int classType, pintype_t pinNumber, Helio_PinMode pinMode, int8_t pinChannel)
    : type((typeof(type))classType), pin(pinNumber), mode(pinMode), channel(pinChannel)
{ ; }

HelioPin::HelioPin(const HelioPinData *dataIn)
    : type((typeof(type))(dataIn->type)), pin(dataIn->pin), mode(dataIn->mode), channel(dataIn->channel)
{ ; }

HelioPin::operator HelioDigitalPin() const
{
    return (isDigitalType() || isDigital() || (!isUnknownType() && !isAnalog())) ? HelioDigitalPin(pin, mode, channel) : HelioDigitalPin();
}

HelioPin::operator HelioAnalogPin() const
{
    return (isAnalogType() || isAnalog() || (!isUnknownType() && !isDigital())) ? HelioAnalogPin(pin, mode, isOutput() ? DAC_RESOLUTION : ADC_RESOLUTION, channel) : HelioAnalogPin();
}

void HelioPin::saveToData(HelioPinData *dataOut) const
{
    dataOut->type = (int8_t)type;
    dataOut->pin = pin;
    dataOut->mode = mode;
    dataOut->channel = channel;
}

void HelioPin::init()
{
    #if !HELIO_SYS_DRY_RUN_ENABLE
        if (isValid()) {
            if (!isExpanded()) {
                switch (mode) {
                    case Helio_PinMode_Digital_Input:
                    case Helio_PinMode_Analog_Input:
                        pinMode(pin, INPUT);
                        break;

                    case Helio_PinMode_Digital_Input_PullUp:
                        pinMode(pin, INPUT_PULLUP);
                        break;

                    case Helio_PinMode_Digital_Input_PullDown:
                        #if defined(ARDUINO_ARCH_SAMD) || defined(ARDUINO_ARCH_MBED) || defined(ESP32) || defined(ARDUINO_ARCH_STM32) || defined(CORE_TEENSY) || defined(INPUT_PULLDOWN)
                            pinMode(pin, INPUT_PULLDOWN);
                        #else
                            pinMode(pin, INPUT);
                        #endif
                        break;

                    case Helio_PinMode_Digital_Output:
                    case Helio_PinMode_Digital_Output_PushPull:
                    case Helio_PinMode_Analog_Output:
                        pinMode(pin, OUTPUT);
                        break;

                    default:
                        break;
                }
            } else {
                SharedPtr<HelioPinExpander> expander = getController() ? getController()->getPinExpander(HelioPin::expanderForPinNumber(pin)) : nullptr;
                if (expander) {
                    expander->getIoAbstraction()->pinDirection(abs(channel), isOutput() ? OUTPUT : mode == Helio_PinMode_Digital_Input_PullUp ? INPUT_PULLUP : mode == Helio_PinMode_Digital_Input_PullDown ? INPUT_PULLDOWN : INPUT);
                }
            }
        }
    #endif
}

void HelioPin::deinit()
{
    #if !HELIO_SYS_DRY_RUN_ENABLE
        if (isValid()) {
            if (!isExpanded()) {
                pinMode(pin, INPUT);
            } else {
                SharedPtr<HelioPinExpander> expander = getController() ? getController()->getPinExpander(HelioPin::expanderForPinNumber(pin)) : nullptr;
                if (expander) {
                    expander->getIoAbstraction()->pinDirection(abs(channel), INPUT);
                }
            }
        }
    #endif
}

bool HelioPin::enablePin(int step)
{
    #if !HELIO_SYS_DRY_RUN_ENABLE
        if (isValid() && isValidChannel(channel)) {
            if (isMuxed()) {
                SharedPtr<HelioPinMuxer> muxer = getController() ? getController()->getPinMuxer(pin) : nullptr;
                if (muxer) {
                    switch (step) {
                        case 0: muxer->selectChannel(channel); muxer->activate(); return true;
                        case 1: muxer->selectChannel(channel); return true;
                        case 2: muxer->activate(); return true;
                        default: return false;
                    }
                }
            } else if (isExpanded()) {
                SharedPtr<HelioPinExpander> expander = getController() ? getController()->getPinExpander(HelioPin::expanderForPinNumber(pin)) : nullptr;
                return expander && expander->syncChannel();
            }
        }
        return false;
    #else
        return isValid() && isValidChannel(channel);
    #endif
}


HelioDigitalPin::HelioDigitalPin()
    : HelioPin(Digital)
{ ; }

HelioDigitalPin::HelioDigitalPin(pintype_t pinNumber, ard_pinmode_t pinMode, int8_t pinChannel)
    : HelioPin(Digital, pinNumber, pinMode != OUTPUT ? (pinMode != INPUT ? (pinMode == INPUT_PULLUP ? Helio_PinMode_Digital_Input_PullUp : Helio_PinMode_Digital_Input_PullDown)
                                                                         : Helio_PinMode_Digital_Input)
                                                     : (pinMode == OUTPUT ? Helio_PinMode_Digital_Output : Helio_PinMode_Digital_Output_PushPull), pinChannel),
      activeLow(pinMode == INPUT || pinMode == INPUT_PULLUP || pinMode == OUTPUT)
{ ; }

HelioDigitalPin::HelioDigitalPin(pintype_t pinNumber, Helio_PinMode pinMode, int8_t pinChannel)
    : HelioPin(Digital, pinNumber, pinMode, pinChannel),
      activeLow(pinMode == Helio_PinMode_Digital_Input ||
                pinMode == Helio_PinMode_Digital_Input_PullUp ||
                pinMode == Helio_PinMode_Digital_Output)
{ ; }

HelioDigitalPin::HelioDigitalPin(pintype_t pinNumber, ard_pinmode_t pinMode, bool isActiveLow, int8_t pinChannel)
    : HelioPin(Digital, pinNumber, pinMode != OUTPUT ? (isActiveLow ? Helio_PinMode_Digital_Input_PullUp : Helio_PinMode_Digital_Input_PullDown)
                                                     : (isActiveLow ? Helio_PinMode_Digital_Output : Helio_PinMode_Digital_Output_PushPull), pinChannel),
      activeLow(isActiveLow)
{ ; }

HelioDigitalPin::HelioDigitalPin(pintype_t pinNumber, Helio_PinMode pinMode, bool isActiveLow, int8_t pinChannel)
    : HelioPin(Digital, pinNumber, pinMode, pinChannel),
      activeLow(isActiveLow)
{ ; }

HelioDigitalPin::HelioDigitalPin(const HelioPinData *dataIn)
    : HelioPin(dataIn), activeLow(dataIn->dataAs.digitalPin.activeLow)
{ ; }

void HelioDigitalPin::saveToData(HelioPinData *dataOut) const
{
    HelioPin::saveToData(dataOut);

    dataOut->dataAs.digitalPin.activeLow = activeLow;
}

ard_pinstatus_t HelioDigitalPin::digitalRead()
{
    #if !HELIO_SYS_DRY_RUN_ENABLE
        if (isValid()) {
            if (isValidChannel(channel)) { selectAndActivatePin(); }
            if (!isExpanded()) {
                return ::digitalRead(pin);
            } else {
                SharedPtr<HelioPinExpander> expander = getController() ? getController()->getPinExpander(HelioPin::expanderForPinNumber(pin)) : nullptr;
                if (expander) {
                    return (ard_pinstatus_t)(expander->getIoAbstraction()->readValue(abs(channel)));
                }
            }
        }
    #endif
    return (ard_pinstatus_t)-1;
}

void HelioDigitalPin::digitalWrite(ard_pinstatus_t status)
{
    #if !HELIO_SYS_DRY_RUN_ENABLE
        if (isValid()) {
            if (!isExpanded()) {
                if (isMuxed()) { selectPin(); }
                ::digitalWrite(pin, status);
            } else {
                SharedPtr<HelioPinExpander> expander = getController() ? getController()->getPinExpander(HelioPin::expanderForPinNumber(pin)) : nullptr;
                if (expander) {
                    expander->getIoAbstraction()->writeValue(abs(channel), (uint8_t)status);
                }
            }
            if (isValidChannel(channel)) { activatePin(); }
        }
    #endif
}


HelioAnalogPin::HelioAnalogPin()
    : HelioPin(Analog), bitRes(0)
#ifdef ESP32
      , pwmChannel(-1)
#endif
#ifdef ESP_PLATFORM
      , pwmFrequency(0)
#endif
{ ; }

HelioAnalogPin::HelioAnalogPin(pintype_t pinNumber, ard_pinmode_t pinMode, uint8_t analogBitRes,
#ifdef ESP32
                               uint8_t pinPWMChannel,
#endif
#ifdef ESP_PLATFORM
                               float pinPWMFrequency,
#endif
                               int8_t pinChannel)
    : HelioPin(Analog, pinNumber, pinMode != OUTPUT ? Helio_PinMode_Analog_Input : Helio_PinMode_Analog_Output, pinChannel),
      bitRes(analogBitRes ? analogBitRes : (pinMode == OUTPUT ? DAC_RESOLUTION : ADC_RESOLUTION))
#ifdef ESP32
      , pwmChannel(pinPWMChannel)
#endif
#ifdef ESP_PLATFORM
      , pwmFrequency(pinPWMFrequency)
#endif
{ ; }

HelioAnalogPin::HelioAnalogPin(pintype_t pinNumber, Helio_PinMode pinMode, uint8_t analogBitRes,
#ifdef ESP32
                               uint8_t pinPWMChannel,
#endif
#ifdef ESP_PLATFORM
                               float pinPWMFrequency,
#endif
                               int8_t pinChannel)
    : HelioPin(Analog, pinNumber, pinMode, pinChannel),
      bitRes(analogBitRes ? analogBitRes : (pinMode == Helio_PinMode_Analog_Output ? DAC_RESOLUTION : ADC_RESOLUTION))
#ifdef ESP32
      , pwmChannel(pinPWMChannel)
#endif
#ifdef ESP_PLATFORM
      , pwmFrequency(pinPWMFrequency)
#endif
{ ; }

HelioAnalogPin::HelioAnalogPin(const HelioPinData *dataIn)
    : HelioPin(dataIn), bitRes(dataIn->dataAs.analogPin.bitRes)
#ifdef ESP32
      , pwmChannel(dataIn->dataAs.analogPin.pwmChannel)
#endif
#ifdef ESP_PLATFORM
      , pwmFrequency(dataIn->dataAs.analogPin.pwmFrequency)
#endif
{ ; }

void HelioAnalogPin::init()
{
    #if !HELIO_SYS_DRY_RUN_ENABLE
        if (isValid()) {
            if (!isExpanded()) {
                HelioPin::init();

                #ifdef ESP32
                    ledcAttachPin(pin, pwmChannel);
                    ledcSetup(pwmChannel, pwmFrequency, bitRes.bits);
                #endif
            } else {
                SharedPtr<HelioPinExpander> expander = getController() ? getController()->getPinExpander(HelioPin::expanderForPinNumber(pin)) : nullptr;
                if (expander) {
                    auto ioDir = isOutput() ? AnalogDirection::DIR_OUT : AnalogDirection::DIR_IN;
                    auto analogIORef = (AnalogDevice *)(expander->getIoAbstraction());
                    analogIORef->initPin(abs(channel), ioDir);

                    auto ioRefBits = analogIORef->getBitDepth(ioDir, abs(channel));
                    if (bitRes.bits != ioRefBits) {
                        bitRes = BitResolution(ioRefBits);
                    }
                }
            }
        }
    #endif
}

void HelioAnalogPin::saveToData(HelioPinData *dataOut) const
{
    HelioPin::saveToData(dataOut);

    dataOut->dataAs.analogPin.bitRes = bitRes.bits;
    #ifdef ESP32
        dataOut->dataAs.analogPin.pwmChannel = pwmChannel;
    #endif
    #ifdef ESP_PLATFORM
        dataOut->dataAs.analogPin.pwmFrequency = pwmFrequency;
    #endif
}

float HelioAnalogPin::analogRead()
{
    return bitRes.transform(analogRead_raw());
}

int HelioAnalogPin::analogRead_raw()
{
    #if !HELIO_SYS_DRY_RUN_ENABLE
        if (isValid()) {
            if (isValidChannel(channel)) { selectAndActivatePin(); }
            if (!isExpanded()) {
                #if defined(ARDUINO_ARCH_SAM) || defined(ARDUINO_ARCH_SAMD)
                    analogReadResolution(bitRes.bits);
                #endif
                return ::analogRead(pin);
            } else {
                SharedPtr<HelioPinExpander> expander = getController() ? getController()->getPinExpander(HelioPin::expanderForPinNumber(pin)) : nullptr;
                if (expander) {
                    auto analogIORef = (AnalogDevice *)(expander->getIoAbstraction());
                    analogIORef->getCurrentValue(abs(channel));
                }
            }
        }
    #endif
    return 0;
}

void HelioAnalogPin::analogWrite(float amount)
{
    analogWrite_raw(bitRes.inverseTransform(amount));
}

void HelioAnalogPin::analogWrite_raw(int amount)
{
    #if !HELIO_SYS_DRY_RUN_ENABLE
        if (isValid()) {
            if (!isExpanded()) {
                if (isMuxed()) { selectPin(); }
                #ifdef ESP32
                    ledcWrite(pwmChannel, amount);
                #else
                    #if defined(ARDUINO_ARCH_SAM) || defined(ARDUINO_ARCH_SAMD)
                        analogWriteResolution(bitRes.bits);
                    #elif defined(ESP8266)
                        analogWriteRange(bitRes.maxVal);
                        analogWriteFreq(pwmFrequency);
                    #endif
                    ::analogWrite(pin, amount);
                #endif
            } else {
                SharedPtr<HelioPinExpander> expander = getController() ? getController()->getPinExpander(HelioPin::expanderForPinNumber(pin)) : nullptr;
                if (expander) {
                    auto analogIORef = (AnalogDevice *)(expander->getIoAbstraction());
                    analogIORef->setCurrentValue(abs(channel), amount);
                }
            }
            if (isValidChannel(channel)) { activatePin(); }
        }
    #endif
}


HelioPinData::HelioPinData()
    : HelioSubData((int8_t)HelioPin::Unknown), pin((pintype_t)-1), mode(Helio_PinMode_Undefined), channel(-1), dataAs{0}
{ ; }

void HelioPinData::toJSONObject(JsonObject &objectOut) const
{
    HelioSubData::toJSONObject(objectOut);

    if (isValidPin(pin)) { objectOut[SFP(HStr_Key_Pin)] = pin; }
    if (mode != Helio_PinMode_Undefined) { objectOut[SFP(HStr_Key_Mode)] = pinModeToString(mode); }
    if (isValidChannel(channel)) { objectOut[SFP(HStr_Key_Channel)] = channel; }

    if (mode != Helio_PinMode_Undefined) {
        if (!(mode == Helio_PinMode_Analog_Input || mode == Helio_PinMode_Analog_Output)) {
            objectOut[SFP(HStr_Key_ActiveLow)] = dataAs.digitalPin.activeLow;
        } else {
            objectOut[SFP(HStr_Key_BitRes)] = dataAs.analogPin.bitRes;
            #ifdef ESP32
                objectOut[SFP(HStr_Key_PWMChannel)] = dataAs.analogPin.pwmChannel;
            #endif
            #ifdef ESP_PLATFORM
                objectOut[SFP(HStr_Key_PWMFrequency)] = dataAs.analogPin.pwmFrequency;
            #endif
        }
    }
}

void HelioPinData::fromJSONObject(JsonObjectConst &objectIn)
{
    HelioSubData::fromJSONObject(objectIn);

    pin = objectIn[SFP(HStr_Key_Pin)] | pin;
    mode = pinModeFromString(objectIn[SFP(HStr_Key_Mode)]);
    channel = objectIn[SFP(HStr_Key_Channel)] | channel;

    if (mode != Helio_PinMode_Undefined) {
        if (!(mode == Helio_PinMode_Analog_Input || mode == Helio_PinMode_Analog_Output)) {
            type = (int8_t)HelioPin::Digital;
            dataAs.digitalPin.activeLow = objectIn[SFP(HStr_Key_ActiveLow)] | dataAs.digitalPin.activeLow;
        } else {
            type = (int8_t)HelioPin::Analog;
            dataAs.analogPin.bitRes = objectIn[SFP(HStr_Key_BitRes)] | dataAs.analogPin.bitRes;
            #ifdef ESP32
                dataAs.analogPin.pwmChannel = objectIn[SFP(HStr_Key_PWMChannel)] | dataAs.analogPin.pwmChannel;
            #endif
            #ifdef ESP_PLATFORM
                dataAs.analogPin.pwmFrequency = objectIn[SFP(HStr_Key_PWMFrequency)] | dataAs.analogPin.pwmFrequency;
            #endif
        }
    } else {
        type = (int8_t)HelioPin::Unknown;
    }
}


HelioPinMuxer::HelioPinMuxer()
    : _signal(), _chipEnable(), _channelPins{(pintype_t)-1,(pintype_t)-1,(pintype_t)-1,(pintype_t)-1,(pintype_t)-1},
      _channelBits(0), _channelSelect(-1)
{
    _signal.channel = -127; // unused
}

HelioPinMuxer::HelioPinMuxer(HelioPin signalPin,
                             pintype_t *muxChannelPins, int8_t muxChannelBits,
                             HelioDigitalPin chipEnablePin)
    : _signal(signalPin), _chipEnable(chipEnablePin),
      _channelPins{ muxChannelBits > 0 ? muxChannelPins[0] : (pintype_t)-1,
                    muxChannelBits > 1 ? muxChannelPins[1] : (pintype_t)-1,
                    muxChannelBits > 2 ? muxChannelPins[2] : (pintype_t)-1,
                    muxChannelBits > 3 ? muxChannelPins[3] : (pintype_t)-1,
                    muxChannelBits > 4 ? muxChannelPins[4] : (pintype_t)-1 },
      _channelBits(muxChannelBits), _channelSelect(-1)
{
    _signal.channel = -127; // unused
}

HelioPinMuxer::HelioPinMuxer(const HelioPinMuxerData *dataIn)
    : _signal(&dataIn->signal), _chipEnable(&dataIn->chipEnable),
      _channelPins{ dataIn->channelPins[0],
                    dataIn->channelPins[1],
                    dataIn->channelPins[2],
                    dataIn->channelPins[3],
                    dataIn->channelPins[4] },
      _channelBits(dataIn->channelBits), _channelSelect(-1)
{
    _signal.channel = -127; // unused
}

void HelioPinMuxer::saveToData(HelioPinMuxerData *dataOut) const
{
    _signal.saveToData(&dataOut->signal);
    _chipEnable.saveToData(&dataOut->chipEnable);
    dataOut->channelPins[0] = _channelPins[0];
    dataOut->channelPins[1] = _channelPins[1];
    dataOut->channelPins[2] = _channelPins[2];
    dataOut->channelPins[3] = _channelPins[3];
    dataOut->channelPins[4] = _channelPins[4];
    dataOut->channelBits = _channelBits;
}

void HelioPinMuxer::init()
{
    _signal.deinit();
    _chipEnable.init();
    _chipEnable.deactivate();

    if (isValidPin(_channelPins[0])) {
        pinMode(_channelPins[0], OUTPUT);
        ::digitalWrite(_channelPins[0], LOW);

        if (isValidPin(_channelPins[1])) {
            pinMode(_channelPins[1], OUTPUT);
            ::digitalWrite(_channelPins[1], LOW);

            if (isValidPin(_channelPins[2])) {
                pinMode(_channelPins[2], OUTPUT);
                ::digitalWrite(_channelPins[2], LOW);

                if (isValidPin(_channelPins[3])) {
                    pinMode(_channelPins[3], OUTPUT);
                    ::digitalWrite(_channelPins[3], LOW);

                    if (isValidPin(_channelPins[4])) {
                        pinMode(_channelPins[4], OUTPUT);
                        ::digitalWrite(_channelPins[4], LOW);
                    }
                }
            }
        }
    }
    _channelSelect = 0;
}

void HelioPinMuxer::selectChannel(uint8_t channelNumber)
{
    if (_channelSelect != channelNumber) {
        // While we could be a bit smarter about which muxers we disable, storing that
        // wouldn't necessarily be worth the gain. The assumption is all that muxers in
        // system occupy the same channel select bus, even if that isn't the case.
        if (getController()) { getController()->deactivatePinMuxers(); }

        if (isValidPin(_channelPins[0])) {
            ::digitalWrite(_channelPins[0], (channelNumber >> 0) & 1 ? HIGH : LOW);

            if (isValidPin(_channelPins[1])) {
                ::digitalWrite(_channelPins[1], (channelNumber >> 1) & 1 ? HIGH : LOW);

                if (isValidPin(_channelPins[2])) {
                    ::digitalWrite(_channelPins[2], (channelNumber >> 2) & 1 ? HIGH : LOW);

                    if (isValidPin(_channelPins[3])) {
                        ::digitalWrite(_channelPins[3], (channelNumber >> 3) & 1 ? HIGH : LOW);

                        if (isValidPin(_channelPins[4])) {
                            ::digitalWrite(_channelPins[4], (channelNumber >> 4) & 1 ? HIGH : LOW);
                        }
                    }
                }
            }
        }
        _channelSelect = channelNumber;
    }
}

void HelioPinMuxer::setIsActive(bool isActive)
{
    if (isActive) {
        _signal.init();
        _chipEnable.activate();
    } else {
        _chipEnable.deactivate();
        _signal.deinit();
    }
}


HelioPinExpander::HelioPinExpander()
    : _signal(), _ioRef(nullptr), _channelBits(0)
{
    _signal.pin = 100;
    _signal.channel = _channelBits;
}

HelioPinExpander::HelioPinExpander(HelioPin signalPin, IoAbstractionRef ioRef, uint8_t channelBits)
    : _signal(signalPin), _ioRef(ioRef),
      _channelBits(channelBits)
{
    _signal.pin = isValidPin(_signal.pin) ? ((_signal.pin >= 100 ? _signal.pin - 100 : _signal.pin) & ~0b1111) + 100 : -1;
    _signal.channel = _channelBits;
}

HelioPinExpander::HelioPinExpander(const HelioPinExpanderData *dataIn, IoAbstractionRef ioRef)
    : _signal(&dataIn->signal), _ioRef(ioRef),
      _channelBits(dataIn->channelBits)
{
    _signal.pin = isValidPin(_signal.pin) ? ((_signal.pin >= 100 ? _signal.pin - 100 : _signal.pin) & ~0b1111) + 100 : -1;
    _signal.channel = _channelBits;
}

void HelioPinExpander::saveToData(HelioPinExpanderData *dataOut) const
{
    _signal.saveToData(&dataOut->signal);
    dataOut->channelBits = _channelBits;
}

bool HelioPinExpander::syncChannel()
{
    return _ioRef->sync();
}


HelioPinMuxerData::HelioPinMuxerData()
    : HelioSubData(0), signal(), chipEnable(), channelPins{(pintype_t)-1,(pintype_t)-1,(pintype_t)-1,(pintype_t)-1,(pintype_t)-1},
      channelBits(0)
{ ; }

void HelioPinMuxerData::toJSONObject(JsonObject &objectOut) const
{
    //HelioSubData::toJSONObject(objectOut); // purposeful no call to base method (ignores type)

    if (isValidPin(signal.pin)) {
        JsonObject signalPinObj = objectOut.createNestedObject(SFP(HStr_Key_SignalPin));
        signal.toJSONObject(signalPinObj);
    }
    if (isValidPin(chipEnable.pin)) {
        JsonObject chipEnablePinObj = objectOut.createNestedObject(SFP(HStr_Key_ChipEnablePin));
        chipEnable.toJSONObject(chipEnablePinObj);
    }
    if (channelBits && isValidPin(channelPins[0])) {
        objectOut[SFP(HStr_Key_ChannelPins)] = commaStringFromArray(channelPins, channelBits);
    }
}

void HelioPinMuxerData::fromJSONObject(JsonObjectConst &objectIn)
{
    //HelioSubData::fromJSONObject(objectIn); // purposeful no call to base method (ignores type)

    JsonObjectConst signalPinObj = objectIn[SFP(HStr_Key_SignalPin)];
    if (!signalPinObj.isNull()) { signal.fromJSONObject(signalPinObj); }
    JsonObjectConst chipEnablePinObj = objectIn[SFP(HStr_Key_ChipEnablePin)];
    if (!chipEnablePinObj.isNull()) { chipEnable.fromJSONObject(chipEnablePinObj); }
    JsonVariantConst channelPinsVar = objectIn[SFP(HStr_Key_ChannelPins)];
    commaStringToArray(channelPinsVar, channelPins, 5);
    for (channelBits = 0; channelBits < 5 && isValidPin(channelPins[channelBits]); ++channelBits) { ; }
}


HelioPinExpanderData::HelioPinExpanderData()
    : HelioSubData(0), signal(), channelBits(0)
{
    signal.channel = channelBits;
}

void HelioPinExpanderData::toJSONObject(JsonObject &objectOut) const
{
    //HelioSubData::toJSONObject(objectOut); // purposeful no call to base method (ignores type)

    if (isValidPin(signal.pin)) {
        JsonObject signalPinObj = objectOut.createNestedObject(SFP(HStr_Key_SignalPin));
        signal.toJSONObject(signalPinObj);
    }
}

void HelioPinExpanderData::fromJSONObject(JsonObjectConst &objectIn)
{
    //HelioSubData::fromJSONObject(objectIn); // purposeful no call to base method (ignores type)

    JsonObjectConst signalPinObj = objectIn[SFP(HStr_Key_SignalPin)];
    if (!signalPinObj.isNull()) { signal.fromJSONObject(signalPinObj); }
    channelBits = signal.channel;
}