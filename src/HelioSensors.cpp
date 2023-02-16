/*  Helioduino: Simple automation controller for solar tracking systems.
    Copyright (C) 2023 NachtRaveVL          <nachtravevl@gmail.com>
    Helioduino Sensorsare 
*/

#include "Helioduino.h"

HelioSensor *newSensorObjectFromData(const HelioSensorData *dataIn)
{
    if (dataIn && isValidType(dataIn->id.object.idType)) return nullptr;
    HELIO_SOFT_ASSERT(dataIn && dataIn->isObjectData(), SFP(HStr_Err_InvalidParameter));

    if (dataIn && dataIn->isObjectData()) {
        switch (dataIn->id.object.classType) {
            case HelioSensor::Binary:
                return new HelioBinarySensor((const HelioBinarySensorData *)dataIn);
            case HelioSensor::Analog:
                return new HelioAnalogSensor((const HelioAnalogSensorData *)dataIn);
            //case 2: // Digital (not instance-able)
            case HelioSensor::DHT1W:
                return new HelioDHTTempHumiditySensor((const HelioDHTTempHumiditySensorData *)dataIn);
            default: break;
        }
    }

    return nullptr;
}

Helio_UnitsType defaultUnitsForSensor(Helio_SensorType sensorType, uint8_t measureRow, Helio_MeasurementMode measureMode)
{
    measureMode = (measureMode == Helio_MeasurementMode_Undefined && getController() ? getController()->getMeasurementMode() : measureMode);

    switch (sensorType) {
        case Helio_SensorType_Endstop:
        case Helio_SensorType_IceDetector:
        case Helio_SensorType_LightIntensity:
            return Helio_UnitsType_Percentile_100;
        case Helio_SensorType_PowerLevel:
            return defaultPowerUnits(measureMode);
        case Helio_SensorType_StrokePosition:
            return defaultDistanceUnits(measureMode);
        case Helio_SensorType_TempHumidity:
            return defaultTemperatureUnits(measureMode);
        case Helio_SensorType_TiltAngle:
            return Helio_UnitsType_Angle_Degrees_360;
        case Helio_SensorType_WindSpeed:
            return defaultSpeedUnits(measureMode);
        default:
            return Helio_UnitsType_Undefined;
    }
}

Helio_UnitsCategory defaultCategoryForSensor(Helio_SensorType sensorType, uint8_t measureRow)
{
    switch (sensorType) {
        case Helio_SensorType_Endstop:
        case Helio_SensorType_IceDetector:
        case Helio_SensorType_LightIntensity:
            return Helio_UnitsCategory_Percentile;
        case Helio_SensorType_PowerLevel:
            return Helio_UnitsCategory_Power;
        case Helio_SensorType_StrokePosition:
            return Helio_UnitsCategory_Distance;
        case Helio_SensorType_TempHumidity:
            switch (measureRow) {
                case 0: return Helio_UnitsCategory_Temperature;
                case 1: return Helio_UnitsCategory_Percentile; // humidity
                case 2: return Helio_UnitsCategory_Temperature; // heat index
                default: break;
            }
        case Helio_SensorType_TiltAngle:
            return Helio_UnitsCategory_Angle;
        case Helio_SensorType_WindSpeed:
            return Helio_UnitsCategory_Speed;
        default:
            return Helio_UnitsCategory_Undefined;
    }
}


HelioSensor::HelioSensor(Helio_SensorType sensorType, hposi_t sensorIndex, int classTypeIn)
    : HelioObject(HelioIdentity(sensorType, sensorIndex)), classType((typeof(classType))classTypeIn),
      _isTakingMeasure(false), _panel(this), _calibrationData(nullptr)
{
    _calibrationData = getController() ? getController()->getUserCalibrationData(_id.key) : nullptr;
}

HelioSensor::HelioSensor(const HelioSensorData *dataIn)
    : HelioObject(dataIn), classType((typeof(classType))(dataIn->id.object.classType)),
      _isTakingMeasure(false), _panel(this), _calibrationData(nullptr)
{
    _calibrationData = getController() ? getController()->getUserCalibrationData(_id.key) : nullptr;
    _panel.setObject(dataIn->panelName);
}

HelioSensor::~HelioSensor()
{
    _isTakingMeasure = false;
}

void HelioSensor::update()
{
    HelioObject::update();

    _panel.resolve();
}

bool HelioSensor::isTakingMeasurement() const
{
    return _isTakingMeasure;
}

bool HelioSensor::getNeedsPolling(hframe_t allowance) const
{
    auto latestMeasurement = getLatestMeasurement();
    return getController() && latestMeasurement ? getController()->isPollingFrameOld(latestMeasurement->frame, allowance) : false;
}

HelioAttachment &HelioSensor::getParentPanel()
{
    return _panel;
}

void HelioSensor::setUserCalibrationData(HelioCalibrationData *userCalibrationData)
{
    if (getController()) {
        if (userCalibrationData && getController()->setUserCalibrationData(userCalibrationData)) {
            _calibrationData = getController()->getUserCalibrationData(_id.key);
        } else if (!userCalibrationData && _calibrationData && getController()->dropUserCalibrationData(_calibrationData)) {
            _calibrationData = nullptr;
        }
    } else {
        _calibrationData = userCalibrationData;
    }
}

Signal<const HelioMeasurement *, HELIO_SENSOR_SIGNAL_SLOTS> &HelioSensor::getMeasurementSignal()
{
    return _measureSignal;
}

HelioData *HelioSensor::allocateData() const
{
    return _allocateDataForObjType((int8_t)_id.type, (int8_t)classType);
}

void HelioSensor::saveToData(HelioData *dataOut)
{
    HelioObject::saveToData(dataOut);

    dataOut->id.object.classType = (int8_t)classType;
    if (_panel.getId()) {
        strncpy(((HelioSensorData *)dataOut)->panelName, _panel.getKeyString().c_str(), HELIO_NAME_MAXSIZE);
    }
}


HelioBinarySensor::HelioBinarySensor(Helio_SensorType sensorType, hposi_t sensorIndex, HelioDigitalPin inputPin, int classType)
    : HelioSensor(sensorType, sensorIndex, classType),
      _inputPin(inputPin), _usingISR(false)
{
    HELIO_HARD_ASSERT(_inputPin.isValid(), SFP(HStr_Err_InvalidPinOrType));
    _inputPin.init();
}

HelioBinarySensor::HelioBinarySensor(const HelioBinarySensorData *dataIn)
    : HelioSensor(dataIn),
      _inputPin(&dataIn->inputPin), _usingISR(false)
{
    HELIO_HARD_ASSERT(_inputPin.isValid(), SFP(HStr_Err_InvalidPinOrType));
    _inputPin.init();

    if (dataIn->usingISR) { tryRegisterAsISR(); }
}

HelioBinarySensor::~HelioBinarySensor()
{
    if (_usingISR) {
        // TODO: detach ISR from taskManger (not currently possible, maybe in future?)
    }
}

bool HelioBinarySensor::takeMeasurement(bool force)
{
    if (_inputPin.isValid() && (force || getNeedsPolling()) && !_isTakingMeasure) {
        _isTakingMeasure = true;
        bool stateBefore = _lastMeasurement.state;

        bool state = _inputPin.isActive();
        auto timestamp = unixNow();

        _lastMeasurement = HelioBinaryMeasurement(state, timestamp);
        getController()->returnPinLock(_inputPin.pin);
        _isTakingMeasure = false;

        #ifdef HELIO_USE_MULTITASKING
            scheduleSignalFireOnce<const HelioMeasurement *>(getSharedPtr(), _measureSignal, &_lastMeasurement);
        #else
            _measureSignal.fire(&_lastMeasurement);
        #endif

        if (state != stateBefore) {
            #ifdef HELIO_USE_MULTITASKING
                scheduleSignalFireOnce<bool>(getSharedPtr(), _stateSignal, _lastMeasurement.state);
            #else
                _stateSignal.fire(_lastMeasurement.state);
            #endif
        }


        return true;
    }
    return false;
}

const HelioMeasurement *HelioBinarySensor::getLatestMeasurement() const
{
    return &_lastMeasurement;
}

void HelioBinarySensor::setMeasureUnits(Helio_UnitsType measureUnits, uint8_t measureRow)
{ ; }

Helio_UnitsType HelioBinarySensor::getMeasureUnits(uint8_t measureRow) const
{
    return Helio_UnitsType_Raw_1;
}

bool HelioBinarySensor::tryRegisterAsISR()
{
    #ifdef HELIO_USE_MULTITASKING
        if (!_usingISR && checkPinCanInterrupt(_inputPin.pin)) {
            taskManager.addInterrupt(&interruptImpl, _inputPin.pin, CHANGE);
            _usingISR = true;
        }
    #endif
    return _usingISR;
}

Signal<bool, HELIO_SENSOR_SIGNAL_SLOTS> &HelioBinarySensor::getStateSignal()
{
    return _stateSignal;
}

void HelioBinarySensor::saveToData(HelioData *dataOut)
{
    HelioSensor::saveToData(dataOut);

    _inputPin.saveToData(&((HelioBinarySensorData *)dataOut)->inputPin);
    ((HelioBinarySensorData *)dataOut)->usingISR = _usingISR;
}


HelioAnalogSensor::HelioAnalogSensor(Helio_SensorType sensorType, hposi_t sensorIndex, HelioAnalogPin inputPin, bool inputInversion, int classType)
    : HelioSensor(sensorType, sensorIndex, classType),
      HelioMeasureUnitsStorage<1>(defaultUnitsForSensor(sensorType)),
      _inputPin(inputPin), _inputInversion(inputInversion)
{
    HELIO_HARD_ASSERT(_inputPin.isValid(), SFP(HStr_Err_InvalidPinOrType));
    _inputPin.init();
}

HelioAnalogSensor::HelioAnalogSensor(const HelioAnalogSensorData *dataIn)
    : HelioSensor(dataIn),
      HelioMeasureUnitsStorage<1>(definedUnitsElse(dataIn->measureUnits, defaultUnitsForSensor((Helio_SensorType)(dataIn->id.object.objType)))),
      _inputPin(&dataIn->inputPin), _inputInversion(dataIn->inputInversion)
{
    HELIO_HARD_ASSERT(_inputPin.isValid(), SFP(HStr_Err_InvalidPinOrType));
    _inputPin.init();
}

bool HelioAnalogSensor::takeMeasurement(bool force)
{
    if (_inputPin.isValid() && (force || getNeedsPolling()) && !_isTakingMeasure) {
        _isTakingMeasure = true;

        #ifdef HELIO_USE_MULTITASKING
            if (isValidTask(scheduleObjectMethodCallWithTaskIdOnce(::getSharedPtr<HelioAnalogSensor>(this), &HelioAnalogSensor::_takeMeasurement))) {
                return true;
            } else {
                HELIO_SOFT_ASSERT(false, SFP(HStr_Err_OperationFailure));
                _isTakingMeasure = false;
            }
        #else
            _takeMeasurement(0xffffU);
        #endif
    }
    return false;
}

void HelioAnalogSensor::_takeMeasurement(unsigned int taskId)
{
    if (_isTakingMeasure && _inputPin.isValid()) {
        if (getController()->tryGetPinLock(_inputPin.pin, 5)) {
            Helio_UnitsType outUnits = definedUnitsElse(_measureUnits[0],
                                                        _calibrationData ? _calibrationData->calibrationUnits : Helio_UnitsType_Undefined,
                                                        defaultUnitsForSensor(_id.objTypeAs.sensorType));

            unsigned int rawRead = 0;
            #if HELIO_SENSOR_ANALOGREAD_SAMPLES > 1
                for (int sampleIndex = 0; sampleIndex < HELIO_SENSOR_ANALOGREAD_SAMPLES; ++sampleIndex) {
                    #if HELIO_SENSOR_ANALOGREAD_DELAY > 0
                        if (sampleIndex) { delay(HELIO_SENSOR_ANALOGREAD_DELAY); }
                    #endif
                    rawRead += _inputPin.analogRead_raw();
                }
                rawRead /= HELIO_SENSOR_ANALOGREAD_SAMPLES;
            #else
                rawRead = _inputPin.analogRead_raw();
            #endif // /if HELIO_SENSOR_ANALOGREAD_SAMPLES > 1
            if (_inputInversion) { rawRead = _inputPin.bitRes.maxVal - rawRead; }
            auto timestamp = unixNow();

            HelioSingleMeasurement newMeasurement(
                _inputPin.bitRes.transform(rawRead),
                Helio_UnitsType_Raw_1,
                timestamp
            );

            calibrationTransform(&newMeasurement);
            convertUnits(&newMeasurement, outUnits);

            _lastMeasurement = newMeasurement;
            getController()->returnPinLock(_inputPin.pin);
            _isTakingMeasure = false;

            #ifdef HELIO_USE_MULTITASKING
                scheduleSignalFireOnce<const HelioMeasurement *>(getSharedPtr(), _measureSignal, &_lastMeasurement);
            #else
                _measureSignal.fire(&_lastMeasurement);
            #endif
        } else {
            _isTakingMeasure = false;
        }
    }
}

const HelioMeasurement *HelioAnalogSensor::getLatestMeasurement() const
{
    return &_lastMeasurement;
}

void HelioAnalogSensor::setMeasureUnits(Helio_UnitsType measureUnits, uint8_t measureRow)
{
    if (_measureUnits[0] != measureUnits) {
        _measureUnits[0] = measureUnits;

        if (_lastMeasurement.frame) {
            convertUnits(&_lastMeasurement, _measureUnits[0]);
        }
    }
}

Helio_UnitsType HelioAnalogSensor::getMeasureUnits(uint8_t measureRow) const
{
    return _measureUnits[0];
}

void HelioAnalogSensor::saveToData(HelioData *dataOut)
{
    HelioSensor::saveToData(dataOut);

    _inputPin.saveToData(&((HelioAnalogSensorData *)dataOut)->inputPin);
    ((HelioAnalogSensorData *)dataOut)->inputInversion = _inputInversion;
    ((HelioAnalogSensorData *)dataOut)->measureUnits = _measureUnits[0];
}


HelioDigitalSensor::HelioDigitalSensor(Helio_SensorType sensorType, hposi_t sensorIndex, HelioDigitalPin inputPin, uint8_t bitRes1W, bool allocate1W, int classType)
    : HelioSensor(sensorType, sensorIndex, classType), _inputPin(inputPin), _oneWire(nullptr), _wireBitRes(bitRes1W), _wirePosIndex(-1), _wireDevAddress{0}
{
    HELIO_HARD_ASSERT(_inputPin.isValid(), SFP(HStr_Err_InvalidPinOrType));
    if (allocate1W && _inputPin.isValid()) {
        _oneWire = getController() ? getController()->getOneWireForPin(_inputPin.pin) : nullptr;
        HELIO_SOFT_ASSERT(_oneWire, SFP(HStr_Err_AllocationFailure));
    }
}

HelioDigitalSensor::HelioDigitalSensor(const HelioDigitalSensorData *dataIn, bool allocate1W)
    : HelioSensor(dataIn), _inputPin(&dataIn->inputPin), _oneWire(nullptr), _wireBitRes(dataIn->wireBitRes), _wirePosIndex(-1), _wireDevAddress{0}
{
    HELIO_HARD_ASSERT(_inputPin.isValid(), SFP(HStr_Err_InvalidPinOrType));
    if (allocate1W && _inputPin.isValid()) {
        _oneWire = getController() ? getController()->getOneWireForPin(_inputPin.pin) : nullptr;
        HELIO_SOFT_ASSERT(_oneWire, SFP(HStr_Err_AllocationFailure));

        if (!arrayElementsEqual<uint8_t>(dataIn->wireDevAddress, 8, 0)) {
            _wirePosIndex = -1 - dataIn->wirePosIndex;
            memcpy(_wireDevAddress, dataIn->wireDevAddress, 8);
        } else {
            _wirePosIndex = -1 - dataIn->wirePosIndex;
        }
    }
}

bool HelioDigitalSensor::setWirePositionIndex(hposi_t wirePosIndex)
{
    wirePosIndex = constrain(wirePosIndex, 0, 62);
    if (_oneWire && wirePosIndex >= 0 && (_wirePosIndex != wirePosIndex || arrayElementsEqual<uint8_t>(_wireDevAddress, 8, 0)) &&
        getController()->tryGetPinLock(_inputPin.pin)) {
        hposi_t posIndex = 0;
        uint8_t devAddress[8];

        _oneWire->reset_search();
        while (posIndex <= wirePosIndex && _oneWire->search(devAddress)) {
            if (posIndex == wirePosIndex && _oneWire->crc8(devAddress, 7) == devAddress[7]) {
                _wirePosIndex = posIndex;
                memcpy(_wireDevAddress, devAddress, 8);

                getController()->returnPinLock(_inputPin.pin);
                return true;
            }
            posIndex++;
        }

        getController()->returnPinLock(_inputPin.pin);
    }
    return false;
}

hposi_t HelioDigitalSensor::getWirePositionIndex() const
{
    return _wirePosIndex >= 0 ? _wirePosIndex : (_wirePosIndex > -64 ? -_wirePosIndex - 1 : -_wirePosIndex - 64);
}

bool HelioDigitalSensor::setWireDeviceAddress(const uint8_t wireDevAddress[8])
{
    if (_oneWire && !arrayElementsEqual<uint8_t>(wireDevAddress, 8, 0) && (_wirePosIndex < 0 || memcmp(_wireDevAddress, wireDevAddress, 8) != 0) &&
        _oneWire->crc8(wireDevAddress, 7) == wireDevAddress[7] && getController()->tryGetPinLock(_inputPin.pin)) {
        hposi_t posIndex = 0;
        uint8_t devAddress[8];

        _oneWire->reset_search();
        while (_oneWire->search(devAddress)) {
            if (memcmp(devAddress, wireDevAddress, 8) == 0) {
                _wirePosIndex = posIndex;
                memcpy(_wireDevAddress, devAddress, 8);

                getController()->returnPinLock(_inputPin.pin);
                return true;
            }
            posIndex++;
        }

        getController()->returnPinLock(_inputPin.pin);
    }
    return false;
}

const uint8_t *HelioDigitalSensor::getWireDeviceAddress() const
{
    return _wireDevAddress;
}

void HelioDigitalSensor::resolveDeviceAddress()
{
    if (_oneWire && !(_wirePosIndex >= 0)) {
        setWireDeviceAddress(_wireDevAddress);

        if (!(_wirePosIndex >= 0) && _wirePosIndex > -64) {
            hposi_t posIndex = -_wirePosIndex - 1;
            setWirePositionIndex(posIndex);

            if (!(_wirePosIndex >= 0)) { _wirePosIndex = -64 - posIndex; } // disables further resolve attempts
        }
    }
}

void HelioDigitalSensor::saveToData(HelioData *dataOut)
{
    HelioSensor::saveToData(dataOut);

    _inputPin.saveToData(&((HelioDigitalSensorData *)dataOut)->inputPin);
    ((HelioDigitalSensorData *)dataOut)->wireBitRes = _wireBitRes;
    ((HelioDigitalSensorData *)dataOut)->wirePosIndex = getWirePositionIndex();
    memcpy(((HelioDigitalSensorData *)dataOut)->wireDevAddress, _wireDevAddress, 8);
}


HelioDHTTempHumiditySensor::HelioDHTTempHumiditySensor(hposi_t sensorIndex, HelioDigitalPin inputPin, Helio_DHTType dhtType, bool computeHeatIndex, int classType)
    : HelioDigitalSensor(Helio_SensorType_TempHumidity, sensorIndex, inputPin, 9, false, classType),
      HelioMeasureUnitsStorage<3>(defaultUnitsForSensor(Helio_SensorType_TempHumidity)),
      _dht(new DHT(inputPin.pin, dhtType)), _dhtType(dhtType), _computeHeatIndex(computeHeatIndex)
{
    _measureUnits[1] = Helio_UnitsType_Percentile_100;
    HELIO_SOFT_ASSERT(_dht, SFP(HStr_Err_AllocationFailure));
    if (_inputPin.isValid() && _dht) { _dht->begin(); }
    else if (_dht) { delete _dht; _dht = nullptr; }
}

HelioDHTTempHumiditySensor::HelioDHTTempHumiditySensor(const HelioDHTTempHumiditySensorData *dataIn)
    : HelioDigitalSensor(dataIn, false),
      HelioMeasureUnitsStorage<3>(definedUnitsElse(dataIn->measureUnits, defaultUnitsForSensor(Helio_SensorType_TempHumidity))),
      _dht(new DHT(dataIn->inputPin.pin, dataIn->dhtType)), _dhtType(dataIn->dhtType), _computeHeatIndex(dataIn->computeHeatIndex)
{
    _measureUnits[1] = Helio_UnitsType_Percentile_100;
    HELIO_SOFT_ASSERT(_dht, SFP(HStr_Err_AllocationFailure));
    if (_inputPin.isValid() && _dht) { _dht->begin(); }
    else if (_dht) { delete _dht; _dht = nullptr; }
}

HelioDHTTempHumiditySensor::~HelioDHTTempHumiditySensor()
{
    if (_dht) { delete _dht; _dht = nullptr; }
}

bool HelioDHTTempHumiditySensor::takeMeasurement(bool force)
{
    if (getController() && _dht && (force || getNeedsPolling()) && !_isTakingMeasure) {
        _isTakingMeasure = true;

        #ifdef HELIO_USE_MULTITASKING
            if (isValidTask(scheduleObjectMethodCallWithTaskIdOnce(::getSharedPtr<HelioDHTTempHumiditySensor>(this), &HelioDHTTempHumiditySensor::_takeMeasurement))) {
                return true;
            } else {
                HELIO_SOFT_ASSERT(false, SFP(HStr_Err_OperationFailure));
                _isTakingMeasure = false;
            }
        #else
            _takeMeasurement(0xffffU);
        #endif
    }
    return false;
}

void HelioDHTTempHumiditySensor::_takeMeasurement(unsigned int taskId)
{
    if (_isTakingMeasure && _dht) {
        if (getController()->tryGetPinLock(_inputPin.pin, 5)) {
            Helio_UnitsType outUnits[3] = { definedUnitsElse(_measureUnits[0],
                                                             _calibrationData ? _calibrationData->calibrationUnits : Helio_UnitsType_Undefined,
                                                             defaultTemperatureUnits()),
                                            definedUnitsElse(_measureUnits[1],
                                                             Helio_UnitsType_Percentile_100),
                                            definedUnitsElse(_measureUnits[2],
                                                             _calibrationData ? _calibrationData->calibrationUnits : Helio_UnitsType_Undefined,
                                                             defaultTemperatureUnits()) };
            bool readInFahrenheit = outUnits[0] == Helio_UnitsType_Temperature_Fahrenheit;
            Helio_UnitsType readUnits = readInFahrenheit ? Helio_UnitsType_Temperature_Fahrenheit
                                                         : Helio_UnitsType_Temperature_Celsius;

            auto tempRead = _dht->readTemperature(readInFahrenheit, true);
            auto humidRead = _dht->readHumidity(true);
            auto timestamp = unixNow();

            HelioTripleMeasurement newMeasurement(
                tempRead, readUnits,
                humidRead, Helio_UnitsType_Percentile_100,
                0.0f, Helio_UnitsType_Undefined,
                timestamp
            );

            calibrationTransform(&newMeasurement.value[0], &newMeasurement.units[0]);
            convertUnits(&newMeasurement.value[0], &newMeasurement.units[0], outUnits[0]);
            convertUnits(&newMeasurement.value[1], &newMeasurement.units[1], outUnits[1]);

            if (_computeHeatIndex) {
                convertUnits(newMeasurement.value[0], &newMeasurement.value[2], newMeasurement.units[0], readUnits, &newMeasurement.units[2]);
                newMeasurement.value[2] = _dht->computeHeatIndex(newMeasurement.value[2], humidRead, readInFahrenheit);
                convertUnits(&newMeasurement.value[2], &newMeasurement.units[2], outUnits[2]);
            }

            _lastMeasurement = newMeasurement;
            getController()->returnPinLock(_inputPin.pin);
            _isTakingMeasure = false;

            #ifdef HELIO_USE_MULTITASKING
                scheduleSignalFireOnce<const HelioMeasurement *>(getSharedPtr(), _measureSignal, &_lastMeasurement);
            #else
                _measureSignal.fire(&_lastMeasurement);
            #endif
        } else {
            _isTakingMeasure = false;
        }
    }
}

const HelioMeasurement *HelioDHTTempHumiditySensor::getLatestMeasurement() const
{
    return &_lastMeasurement;
}

void HelioDHTTempHumiditySensor::setMeasureUnits(Helio_UnitsType measureUnits, uint8_t measureRow)
{
    if (_measureUnits[measureRow] != measureUnits) {
        _measureUnits[measureRow] = measureUnits;

        if (_lastMeasurement.frame) {
            convertUnits(&_lastMeasurement.value[measureRow], &_lastMeasurement.units[measureRow], _measureUnits[measureRow]);
        }
    }
}

Helio_UnitsType HelioDHTTempHumiditySensor::getMeasureUnits(uint8_t measureRow) const
{
    return _measureUnits[measureRow];
}

bool HelioDHTTempHumiditySensor::setWirePositionIndex(hposi_t wirePosIndex)
{
    return false;
}

hposi_t HelioDHTTempHumiditySensor::getWirePositionIndex() const
{
    return -1;
}

bool HelioDHTTempHumiditySensor::setWireDeviceAddress(const uint8_t wireDevAddress[8])
{
    return false;
}

const uint8_t *HelioDHTTempHumiditySensor::getWireDeviceAddress() const
{
    return nullptr;
}

void HelioDHTTempHumiditySensor::setComputeHeatIndex(bool computeHeatIndex)
{
    if (_computeHeatIndex != computeHeatIndex) {
        _computeHeatIndex = computeHeatIndex;
    }
}

void HelioDHTTempHumiditySensor::saveToData(HelioData *dataOut)
{
    HelioDigitalSensor::saveToData(dataOut);

    ((HelioDHTTempHumiditySensorData *)dataOut)->dhtType = _dhtType;
    ((HelioDHTTempHumiditySensorData *)dataOut)->computeHeatIndex = _computeHeatIndex;
    ((HelioDHTTempHumiditySensorData *)dataOut)->measureUnits = _measureUnits[0];
}


HelioSensorData::HelioSensorData()
    : HelioObjectData(), inputPin(), cropName{0}, panelName{0}
{
    _size = sizeof(*this);
}

void HelioSensorData::toJSONObject(JsonObject &objectOut) const
{
    HelioObjectData::toJSONObject(objectOut);

    if (isValidPin(inputPin.pin)) {
        JsonObject inputPinObj = objectOut.createNestedObject(SFP(HStr_Key_InputPin));
        inputPin.toJSONObject(inputPinObj);
    }
    if (panelName[0]) { objectOut[SFP(HStr_Key_PanelName)] = charsToString(panelName, HELIO_NAME_MAXSIZE); }
}

void HelioSensorData::fromJSONObject(JsonObjectConst &objectIn)
{
    HelioObjectData::fromJSONObject(objectIn);

    JsonObjectConst inputPinObj = objectIn[SFP(HStr_Key_InputPin)];
    if (!inputPinObj.isNull()) { inputPin.fromJSONObject(inputPinObj); }
    const char *panelNameStr = objectIn[SFP(HStr_Key_PanelName)];
    if (panelNameStr && panelNameStr[0]) { strncpy(panelName, panelNameStr, HELIO_NAME_MAXSIZE); }
}

HelioBinarySensorData::HelioBinarySensorData()
    : HelioSensorData(), usingISR(false)
{
    _size = sizeof(*this);
}

void HelioBinarySensorData::toJSONObject(JsonObject &objectOut) const
{
    HelioSensorData::toJSONObject(objectOut);

    if (usingISR != false) { objectOut[SFP(HStr_Key_UsingISR)] = usingISR; }
}

void HelioBinarySensorData::fromJSONObject(JsonObjectConst &objectIn)
{
    HelioSensorData::fromJSONObject(objectIn);

    usingISR = objectIn[SFP(HStr_Key_UsingISR)] | usingISR;
}

HelioAnalogSensorData::HelioAnalogSensorData()
    : HelioSensorData(), inputInversion(false), measureUnits(Helio_UnitsType_Undefined)
{
    _size = sizeof(*this);
}

void HelioAnalogSensorData::toJSONObject(JsonObject &objectOut) const
{
    HelioSensorData::toJSONObject(objectOut);

    if (inputInversion != false) { objectOut[SFP(HStr_Key_InputInversion)] = inputInversion; }
    if (measureUnits != Helio_UnitsType_Undefined) { objectOut[SFP(HStr_Key_MeasureUnits)] = unitsTypeToSymbol(measureUnits); }
}

void HelioAnalogSensorData::fromJSONObject(JsonObjectConst &objectIn)
{
    HelioSensorData::fromJSONObject(objectIn);

    inputInversion = objectIn[SFP(HStr_Key_InputInversion)] | inputInversion;
    measureUnits = unitsTypeFromSymbol(objectIn[SFP(HStr_Key_MeasureUnits)]);
}


HelioDigitalSensorData::HelioDigitalSensorData()
    : HelioSensorData(), wireBitRes(9), wirePosIndex(-1), wireDevAddress{0}
{
    _size = sizeof(*this);
}

void HelioDigitalSensorData::toJSONObject(JsonObject &objectOut) const
{
    HelioSensorData::toJSONObject(objectOut);

    if (wireBitRes != 9) { objectOut[SFP(HStr_Key_BitRes)] = wireBitRes; }
    if (wirePosIndex > 0) { objectOut[SFP(HStr_Key_WirePosIndex)] = wirePosIndex; }
    if (!arrayElementsEqual<uint8_t>(wireDevAddress, 8, 0)) { objectOut[SFP(HStr_Key_WireDevAddress)] = hexStringFromBytes(wireDevAddress, 8); }
}

void HelioDigitalSensorData::fromJSONObject(JsonObjectConst &objectIn)
{
    HelioSensorData::fromJSONObject(objectIn);

    wireBitRes = objectIn[SFP(HStr_Key_BitRes)] | wireBitRes;
    wirePosIndex = objectIn[SFP(HStr_Key_WirePosIndex)] | wirePosIndex;
    JsonVariantConst wireDevAddressVar = objectIn[SFP(HStr_Key_WireDevAddress)];
    hexStringToBytes(wireDevAddressVar, wireDevAddress, 8);
    for (int addrIndex = 0; addrIndex < 8; ++addrIndex) { 
        wireDevAddress[addrIndex] = wireDevAddressVar[addrIndex] | wireDevAddress[addrIndex];
    }
}

HelioDHTTempHumiditySensorData::HelioDHTTempHumiditySensorData()
    : HelioDigitalSensorData(), dhtType(Helio_DHTType_None), computeHeatIndex(false), measureUnits(Helio_UnitsType_Undefined)
{
    _size = sizeof(*this);
}

void HelioDHTTempHumiditySensorData::toJSONObject(JsonObject &objectOut) const
{
    HelioDigitalSensorData::toJSONObject(objectOut);

    if (dhtType != Helio_DHTType_None) { objectOut[SFP(HStr_Key_DHTType)] = dhtType; }
    if (computeHeatIndex != false) { objectOut[SFP(HStr_Key_ComputeHeatIndex)] = computeHeatIndex; }
    if (measureUnits != Helio_UnitsType_Undefined) { objectOut[SFP(HStr_Key_MeasureUnits)] = unitsTypeToSymbol(measureUnits); }
}

void HelioDHTTempHumiditySensorData::fromJSONObject(JsonObjectConst &objectIn)
{
    HelioDigitalSensorData::fromJSONObject(objectIn);

    dhtType = objectIn[SFP(HStr_Key_DHTType)] | dhtType;
    computeHeatIndex = objectIn[SFP(HStr_Key_ComputeHeatIndex)] | computeHeatIndex;
    measureUnits = unitsTypeFromSymbol(objectIn[SFP(HStr_Key_MeasureUnits)]);
}
