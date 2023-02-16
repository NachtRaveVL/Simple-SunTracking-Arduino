/*  Helioduino: Simple automation controller for solar tracking systems.
    Copyright (C) 2023 NachtRaveVL          <nachtravevl@gmail.com>
    Helioduino Sensor Measurements
*/

#include "Helioduino.h"

HelioMeasurement *newMeasurementObjectFromSubData(const HelioMeasurementData *dataIn)
{
    if (!dataIn || !isValidType(dataIn->type)) return nullptr;
    HELIO_SOFT_ASSERT(dataIn && isValidType(dataIn->type), SFP(HStr_Err_InvalidParameter));

    if (dataIn) {
        switch (dataIn->type) {
            case (int8_t)HelioMeasurement::Binary:
                return new HelioBinaryMeasurement(dataIn);
            case (int8_t)HelioMeasurement::Single:
                return new HelioSingleMeasurement(dataIn);
            case (int8_t)HelioMeasurement::Double:
                return new HelioDoubleMeasurement(dataIn);
            case (int8_t)HelioMeasurement::Triple:
                return new HelioTripleMeasurement(dataIn);
            default: break;
        }
    }

    return nullptr;
}

float getMeasurementValue(const HelioMeasurement *measurement, uint8_t measureRow, float binTrue)
{
    if (measurement) {
        switch (measurement->type) {
            case HelioMeasurement::Binary:
                return ((HelioBinaryMeasurement *)measurement)->state ? binTrue : 0.0f;
            case HelioMeasurement::Single:
                return ((HelioSingleMeasurement *)measurement)->value;
            case HelioMeasurement::Double:
                return ((HelioDoubleMeasurement *)measurement)->value[measureRow];
            case HelioMeasurement::Triple:
                return ((HelioTripleMeasurement *)measurement)->value[measureRow];
            default: break;
        }
    }
    return 0.0f;
}

Helio_UnitsType getMeasureUnits(const HelioMeasurement *measurement, uint8_t measureRow, Helio_UnitsType binUnits)
{
    if (measurement) {
        switch (measurement->type) {
            case HelioMeasurement::Binary:
                return binUnits;
            case HelioMeasurement::Single:
                return ((HelioSingleMeasurement *)measurement)->units;
            case HelioMeasurement::Double:
                return ((HelioDoubleMeasurement *)measurement)->units[measureRow];
            case HelioMeasurement::Triple:
                return ((HelioTripleMeasurement *)measurement)->units[measureRow];
            default: break;
        }
    }
    return Helio_UnitsType_Undefined;
}

uint8_t getMeasureRowCount(const HelioMeasurement *measurement)
{
    return measurement ? max(1, (int)(measurement->type)) : 0;
}

HelioSingleMeasurement getAsSingleMeasurement(const HelioMeasurement *measurement, uint8_t measureRow, float binTrue, Helio_UnitsType binUnits)
{
    if (measurement) {
        switch (measurement->type) {
            case HelioMeasurement::Binary:
                return ((HelioBinaryMeasurement *)measurement)->getAsSingleMeasurement(binTrue, binUnits);
            case HelioMeasurement::Single:
                return *((const HelioSingleMeasurement *)measurement);
            case HelioMeasurement::Double:
                return ((HelioDoubleMeasurement *)measurement)->getAsSingleMeasurement(measureRow);
            case HelioMeasurement::Triple:
                return ((HelioTripleMeasurement *)measurement)->getAsSingleMeasurement(measureRow);
            default: break;
        }
    }
    HelioSingleMeasurement retVal;
    retVal.frame = 0; // force fails frame checks
    return retVal;
}


HelioMeasurement::HelioMeasurement(int classType, time_t timestampIn)
    : type((typeof(type))classType), timestamp(timestampIn)
{
    updateFrame();
}

HelioMeasurement::HelioMeasurement(const HelioMeasurementData *dataIn)
    : type((typeof(type))(dataIn->type)), timestamp(dataIn->timestamp)
{
    updateFrame(1);
}

void HelioMeasurement::saveToData(HelioMeasurementData *dataOut, uint8_t measureRow, unsigned int additionalDecPlaces) const
{
    dataOut->type = (int8_t)type;
    dataOut->measureRow = measureRow;
    dataOut->timestamp = timestamp;
}

void HelioMeasurement::updateFrame(hframe_t minFrame)
{
    frame = max(minFrame, getController() ? getController()->getPollingFrame() : 0);
}


HelioBinaryMeasurement::HelioBinaryMeasurement()
    : HelioMeasurement(), state(false)
{ ; }

HelioBinaryMeasurement::HelioBinaryMeasurement(bool stateIn, time_t timestamp)
    : HelioMeasurement((int)Binary, timestamp), state(stateIn)
{ ; }

HelioBinaryMeasurement::HelioBinaryMeasurement(bool stateIn, time_t timestamp, hframe_t frame)
    : HelioMeasurement((int)Binary, timestamp, frame), state(stateIn)
{ ; }

HelioBinaryMeasurement::HelioBinaryMeasurement(const HelioMeasurementData *dataIn)
    : HelioMeasurement(dataIn),
      state(dataIn->measureRow == 0 && dataIn->value >= 0.5f - FLT_EPSILON)
{ ; }

void HelioBinaryMeasurement::saveToData(HelioMeasurementData *dataOut, uint8_t measureRow, unsigned int additionalDecPlaces) const
{
    HelioMeasurement::saveToData(dataOut, measureRow, additionalDecPlaces);

    dataOut->value = measureRow == 0 && state ? 1.0f : 0.0f;
    dataOut->units = measureRow == 0 ? Helio_UnitsType_Raw_1 : Helio_UnitsType_Undefined;
}


HelioSingleMeasurement::HelioSingleMeasurement()
    : HelioMeasurement((int)Single), value(0.0f), units(Helio_UnitsType_Undefined)
{ ; }

HelioSingleMeasurement::HelioSingleMeasurement(float valueIn, Helio_UnitsType unitsIn, time_t timestamp)
    : HelioMeasurement((int)Single, timestamp), value(valueIn), units(unitsIn)
{ ; }

HelioSingleMeasurement::HelioSingleMeasurement(float valueIn, Helio_UnitsType unitsIn, time_t timestamp, hframe_t frame)
    : HelioMeasurement((int)Single, timestamp, frame), value(valueIn), units(unitsIn)
{ ; }

HelioSingleMeasurement::HelioSingleMeasurement(const HelioMeasurementData *dataIn)
    : HelioMeasurement(dataIn),
      value(dataIn->measureRow == 0 ? dataIn->value : 0.0f),
      units(dataIn->measureRow == 0 ? dataIn->units : Helio_UnitsType_Undefined)
{ ; }

void HelioSingleMeasurement::saveToData(HelioMeasurementData *dataOut, uint8_t measureRow, unsigned int additionalDecPlaces) const
{
    HelioMeasurement::saveToData(dataOut, measureRow, additionalDecPlaces);

    dataOut->value = measureRow == 0 ? roundForExport(value, additionalDecPlaces) : 0.0f;
    dataOut->units = measureRow == 0 ? units : Helio_UnitsType_Undefined;
}


HelioDoubleMeasurement::HelioDoubleMeasurement()
    : HelioMeasurement((int)Double), value{0}, units{Helio_UnitsType_Undefined,Helio_UnitsType_Undefined}
{ ; }

HelioDoubleMeasurement::HelioDoubleMeasurement(float value1, Helio_UnitsType units1,
                                               float value2, Helio_UnitsType units2,
                                               time_t timestamp)
    : HelioMeasurement((int)Double, timestamp), value{value1,value2}, units{units1,units2}
{ ; }

HelioDoubleMeasurement::HelioDoubleMeasurement(float value1, Helio_UnitsType units1,
                                               float value2, Helio_UnitsType units2,
                                               time_t timestamp, hframe_t frame)
    : HelioMeasurement((int)Double, timestamp, frame), value{value1,value2}, units{units1,units2}
{ ; }

HelioDoubleMeasurement::HelioDoubleMeasurement(const HelioMeasurementData *dataIn)
    : HelioMeasurement(dataIn),
      value{dataIn->measureRow == 0 ? dataIn->value : 0.0f,
            dataIn->measureRow == 1 ? dataIn->value : 0.0f
      },
      units{dataIn->measureRow == 0 ? dataIn->units : Helio_UnitsType_Undefined,
            dataIn->measureRow == 1 ? dataIn->units : Helio_UnitsType_Undefined
      }
{ ; }

void HelioDoubleMeasurement::saveToData(HelioMeasurementData *dataOut, uint8_t measureRow, unsigned int additionalDecPlaces) const
{
    HelioMeasurement::saveToData(dataOut, measureRow, additionalDecPlaces);

    dataOut->value = measureRow >= 0 && measureRow < 2 ? roundForExport(value[measureRow], additionalDecPlaces) : 0.0f;
    dataOut->units = measureRow >= 0 && measureRow < 2 ? units[measureRow] : Helio_UnitsType_Undefined;
}


HelioTripleMeasurement::HelioTripleMeasurement()
    : HelioMeasurement((int)Triple), value{0}, units{Helio_UnitsType_Undefined,Helio_UnitsType_Undefined,Helio_UnitsType_Undefined}
{ ; }

HelioTripleMeasurement::HelioTripleMeasurement(float value1, Helio_UnitsType units1,
                                               float value2, Helio_UnitsType units2,
                                               float value3, Helio_UnitsType units3,
                                               time_t timestamp)
    : HelioMeasurement((int)Triple, timestamp), value{value1,value2,value3}, units{units1,units2,units3}
{ ; }

HelioTripleMeasurement::HelioTripleMeasurement(float value1, Helio_UnitsType units1,
                                               float value2, Helio_UnitsType units2,
                                               float value3, Helio_UnitsType units3,
                                               time_t timestamp, hframe_t frame)
    : HelioMeasurement((int)Triple, timestamp, frame), value{value1,value2,value3}, units{units1,units2,units3}
{ ; }

HelioTripleMeasurement::HelioTripleMeasurement(const HelioMeasurementData *dataIn)
    : HelioMeasurement(dataIn),
      value{dataIn->measureRow == 0 ? dataIn->value : 0.0f,
            dataIn->measureRow == 1 ? dataIn->value : 0.0f,
            dataIn->measureRow == 2 ? dataIn->value : 0.0f,
      },
      units{dataIn->measureRow == 0 ? dataIn->units : Helio_UnitsType_Undefined,
            dataIn->measureRow == 1 ? dataIn->units : Helio_UnitsType_Undefined,
            dataIn->measureRow == 2 ? dataIn->units : Helio_UnitsType_Undefined,
      }
{ ; }

void HelioTripleMeasurement::saveToData(HelioMeasurementData *dataOut, uint8_t measureRow, unsigned int additionalDecPlaces) const
{
    HelioMeasurement::saveToData(dataOut, measureRow, additionalDecPlaces);

    dataOut->value = measureRow >= 0 && measureRow < 3 ? roundForExport(value[measureRow], additionalDecPlaces) : 0.0f;
    dataOut->units = measureRow >= 0 && measureRow < 3 ? units[measureRow] : Helio_UnitsType_Undefined;
}


HelioMeasurementData::HelioMeasurementData()
    : HelioSubData(0), measureRow(0), value(0.0f), units(Helio_UnitsType_Undefined), timestamp(0)
{ ; }

void HelioMeasurementData::toJSONObject(JsonObject &objectOut) const
{
    //HelioSubData::toJSONObject(objectOut); // purposeful no call to base method (ignores type)

    objectOut[SFP(HStr_Key_MeasureRow)] = measureRow;
    objectOut[SFP(HStr_Key_Value)] = value;
    objectOut[SFP(HStr_Key_Units)] = unitsTypeToSymbol(units);
    objectOut[SFP(HStr_Key_Timestamp)] = timestamp;
}

void HelioMeasurementData::fromJSONObject(JsonObjectConst &objectIn)
{
    //HelioSubData::fromJSONObject(objectIn); // purposeful no call to base method (ignores type)

    measureRow = objectIn[SFP(HStr_Key_MeasureRow)] | measureRow;
    value = objectIn[SFP(HStr_Key_Value)] | value;
    units = unitsTypeFromSymbol(objectIn[SFP(HStr_Key_Units)]);
    timestamp = objectIn[SFP(HStr_Key_Timestamp)] | timestamp;
}

void HelioMeasurementData::fromJSONVariant(JsonVariantConst &variantIn)
{
    if (variantIn.is<JsonObjectConst>()) {
        JsonObjectConst variantObj = variantIn;
        fromJSONObject(variantObj);
    } else if (variantIn.is<float>() || variantIn.is<int>()) {
        value = variantIn.as<float>();
    } else {
        HELIO_SOFT_ASSERT(false, SFP(HStr_Err_UnsupportedOperation));
    }
}
