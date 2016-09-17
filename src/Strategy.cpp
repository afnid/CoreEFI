// copyright to me, released under GPL V3

#include "Strategy.h"
#include "GPIO.h"
#include "System.h"
#include "Buffer.h"
#include "Params.h"
#include "Metrics.h"

#include "Codes.h"
#include "Decoder.h"
#include "Events.h"
#include "Schedule.h"
#include "efi_types.h"

static const char *PATH = __FILE__;

static const int TimesMax = TimeMovingSeconds - TimeRunSeconds + 1;

static struct {
	uint16_t times[TimesMax];
	uint8_t run;
} strategy;

static const float CUIN_TO_CUFT = 1.0 / (12 * 12 * 12);
static const float KG_TO_LB = 1.0 / 2.20462262;

static const float STD_LB_FT3 = 0.080672; // lbm / ft^3 density of // air at 32f
static const float STD_LB_IN3 = STD_LB_FT3 * CUIN_TO_CUFT; // lbm / // in^3

static const float DIV100 = 1.0 / 100.0;
static const float DIV600US = 1.0 / 600000.0;

static inline float safediv(float v1, float v2) {
	return v1 == 0 || v2 == 0 ? 0 : v1 / v2;
}

/*
 static const float INHG_TO_G = 36.2;

 static float getCFM(float kgHr, float inHg, float degf) {
 return kgHr * getDegreesRankine(degf) / (inHg * INHG_TO_G);
 }

 static float getKgHr(float cfm, float InHg, float degf) {
 return cfm * InHg * INHG_TO_G / getDegreesRankine(degf);
 }
 */

static inline float calcMPH() {
	static const float PIf = 3.14159265358979323846;
	static const float ratio = PIf * 60 / (5280 * 12);

	int gear = getParamUnsigned(SensorGEAR);

	if (gear > 0) {
		float r = getParamUnsigned(SensorRPM);
		r /= (getParamFloat(FuncTransmissionRatio) * getParamFloat(ConstAxleRatio));
		return getParamFloat(ConstTireDiameter) * ratio;
	}

	return 0;
}

static inline uint16_t getTimer(uint8_t id) {
	int idx = id - TimeRunSeconds;
	assert(idx >= 0);
	assert(idx < TimesMax);

	if (!strategy.times[idx])
		return 0;

	const GPIO::PinDef *pd = GPIO::getPinDef(GPIO::IsKeyOn);
	return tdiff32(pd->changed, strategy.times[idx]);
}

void Strategy::setTimer(ParamTypeId id, bool enable) {
	int idx = id - TimeRunSeconds;
	assert(idx >= 0);
	assert(idx < TimesMax);

	bool isset = strategy.times[idx] != 0;

	if (enable != isset) {
		const GPIO::PinDef *pd = GPIO::getPinDef(GPIO::IsKeyOn);
		uint16_t epoch = !enable ? 0 : pd->changed;
		strategy.times[idx] = epoch;
		setParamUnsigned(id, epoch);
	}
}

static float getDegreesRankine(float f) {
	const static float rakine = 459.67;
	return rakine + f;
}

static inline float getAirDensityRatio(float f) {
	return getDegreesRankine(32) / getDegreesRankine(f);
}

static float getFinalAirFuel() {
	float ratio = 0;
	ratio += getParamFloat(TableFuelBase);
	ratio += getParamFloat(TableFuelStartup);
	return ratio / getParamFloat(ConstLamda);
}

static uint16_t getFinalSparkWidth() {
	static const float DIV720 = 1.0 / 720.0;
	float lo = getParamFloat(FuncMinLowSpeedDwell) * DIV720;
	float hi = getParamFloat(FuncMinHighSpeedDwell) * DIV720;
	uint16_t rpm = getParamUnsigned(SensorRPM);
	uint16_t idle = getParamUnsigned(ConstIdleRPM);
	uint16_t max = getParamUnsigned(ConstMaxRPM);
	float dwell = min(hi, lo + (rpm - idle) * (hi - lo) / (max - idle));
	dwell = 5000 + dwell * 1000; // 5-6 ms, ls1 coil
	return dwell;
}

static float getFinalPulseWidth() {
	float ms = getParamFloat(TableAccelEnrichment);
	ms += getParamFloat(FuncCrankingFuelPulseWidthVsEct);

	float r = 0;

	if ((r = getParamFloat(FuncOpenLoopFuelMultiplierVsAct)) > 0 && r != 1)
		ms *= r;
	if ((r = getParamFloat(FuncOpenLoopFuelMultiplierVsRpm)) > 0 && r != 1)
		ms *= r;
	if ((r = getParamFloat(FuncCrankFuelPulseWidthMultiplier)) > 0 && r != 1)
		ms *= r;

	return ms;
}

static float getFinalSparkAdvance() {
	float deg = 0;
	deg += getParamFloat(TableSparkLimp);
	deg += getParamFloat(TableSparkAltitude);

	if (deg == 0)
		deg += getParamFloat(TableSparkBase);

	deg += getParamFloat(FuncWotSparkAdvanceVsRpm);
	deg += getParamFloat(FuncWotSparkAdvanceVsAct);
	deg += getParamFloat(FuncWotSparkAdvanceVsEct);
	deg += getParamFloat(FuncSparkAdvanceRateVsRpm);
	deg += getParamFloat(FuncSparkAdvanceVsAct);
	deg += getParamFloat(FuncSparkAdvanceVsBp);

	return deg;
}

void expireCached(ParamTypeId id) {
	extern void clearCached(ParamTypeId id);

	switch (id) {
		case FlagIsLimpMode:
			clearCached(TableSparkLimp);
			break;
		case TimeRunSeconds:
			clearCached(TableFuelStartup);
			break;
		case FlagIsHighAlt:
			clearCached(TableSparkAltitude);
			break;
		case FlagIsWOT:
			clearCached(FuncWotFuelMultiplierVsRpm);
			clearCached(FuncWotSparkAdvanceVsRpm);
			clearCached(FuncWotSparkAdvanceVsEct);
			clearCached(FuncWotSparkAdvanceVsAct);
			break;
		case FlagIsClosedLoop:
			clearCached(FuncOpenLoopFuelMultiplierVsAct);
			clearCached(FuncOpenLoopFuelMultiplierVsRpm);
			break;
		default:
			break;
	}

	if (GPIO::isPinSet(GPIO::IsCranking)) {
		clearCached(FuncCrankFuelPulseWidthMultiplier);
		clearCached(FuncCrankingFuelPulseWidthVsEct);
	}
}

float getStrategyDouble(ParamTypeId id, ParamData *pd) {
	extern float lookupParam(ParamTypeId id, ParamData *pd);
	extern float dblDecode(ParamData *pd, uint16_t v);
	extern uint16_t uint16Decode(ParamData *pd, uint16_t v);

	if (pd->hdr) {
		switch (id) {
			case TableSparkLimp:
				if (!isParamSet(FlagIsLimpMode))
					return 0;
				break;
			case TableFuelStartup:
				if (getParamUnsigned(TimeRunSeconds) > 60)
					return 0;
				break;
			case TableSparkAltitude:
				if (!isParamSet(FlagIsHighAlt))
					return 0;
				break;
			case FuncWotFuelMultiplierVsRpm:
			case FuncWotSparkAdvanceVsRpm:
			case FuncWotSparkAdvanceVsEct:
			case FuncWotSparkAdvanceVsAct:
				if (!isParamSet(FlagIsWOT))
					return 0;
				break;
			case FuncCrankFuelPulseWidthMultiplier:
			case FuncCrankingFuelPulseWidthVsEct:
				if (!GPIO::isPinSet(GPIO::IsCranking))
					return 0;
				break;
			case FuncOpenLoopFuelMultiplierVsAct:
			case FuncOpenLoopFuelMultiplierVsRpm:
				if (isParamSet(FlagIsClosedLoop))
					return 0;
				break;
			default:
				break;
		}

		return lookupParam(id, pd);
	}

	switch (id) {
		case CalcFinalPulseWidth:
			return getFinalPulseWidth();
		case CalcFinalPulseAdvance:
			return getParamFloat(TableInjectorTiming);
		case CalcFinalSparkAdvance:
			return getFinalSparkAdvance();
		case CalcFinalSparkWidth:
			return getFinalSparkWidth();
		case CalcFinalLamda:
			return getFinalAirFuel();

		case CalcLoad:
			return !isParamSet(TimeRunSeconds) ? 0 : 100 * safediv(getParamFloat(CalcVolumetricRate), getParamFloat(CalcTheoreticalRate));
		case CalcVolumetricRate: // ratio * kg/hr * lb/kg = lb/hr
			return getAirDensityRatio(getParamFloat(SensorACT)) * getParamFloat(FuncMafTransfer) * KG_TO_LB;
		case CalcTheoreticalRate: // lb/in3 * in3 * tdc/min * 30 = lb/hr
			return getParamFloat(ConstCuIn) * getParamUnsigned(SensorRPM) * STD_LB_IN3 * 30;
		case CalcDisplacementVolume: // in3 * tdc/min * ft3/in3 = ft3/min
			return getParamFloat(ConstCuIn) * getParamUnsigned(SensorRPM) * CUIN_TO_CUFT / 2;

		case CalcDutyCycle:
			return !isParamSet(TimeRunSeconds) ? 0 : getParamFloat(CalcFinalPulseWidth) * getParamUnsigned(SensorRPM) * DIV600US;

		case CalcLbsPerHr:
			return !isParamSet(TimeRunSeconds) ? 0 : getParamUnsigned(ConstCylinders) * getParamFloat(ConstInjLoSlope) * getParamFloat(CalcDutyCycle) * DIV100;
		case CalcMPG:
			return !isParamSet(TimeRunSeconds) ? 0 : safediv(100 * getParamFloat(SensorVSS), getParamFloat(CalcGalPerHour));
		case CalcGalPerHour:
			return !isParamSet(TimeRunSeconds) ? 0 : safediv(getParamFloat(CalcLbsPerHr), getParamFloat(ConstGallonWeight));

		case CalcThrottleRate:
			return 0;
		case FlagIsWOT:
			return !isParamSet(TimeRunSeconds) ? 0 : getParamFloat(SensorTPS) >= getParamUnsigned(ConstWOT) ? 1 : 0;

		case FlagIsHighAlt:
			return getParamFloat(SensorBAR) >= getParamFloat(ConstHighAlt);

		case SensorVSS:
		case CalcRPMtoMPH:
			return calcMPH();

		case SensorRPM:
			return decoder.getRPM() * getParamUnsigned(ConstEncoderRatio);

		case FlagIsMonitoring:
		case FlagIsLimpMode:
		case FlagIsClosedLoop:

		case CalcFan1:
		case CalcFan2:
		case CalcFuelPump:
		case CalcEPAS2:
			return uint16Decode(pd, pd->value);
		default:
			break;
	}

	if (pd->cat == CatConst || pd->cat == CatSensor)
		return dblDecode(pd, pd->value);

	if (pd->cat == CatTime) {
		if (id == TimeRunSeconds)
			return uint16Decode(pd, pd->value);
		return getTimer(id);
	}

	assert(id);

	codes.set(id);

	return pd->value;
}

void Strategy::init() {
	bzero(&strategy, sizeof(strategy));

	setParamUnsigned(ConstCoils, min(MaxCoils, getParamUnsigned(ConstCoils)));
	setParamUnsigned(ConstCylinders, min(MaxCylinders, getParamUnsigned(ConstCylinders)));
	setParamUnsigned(ConstEncoderTeeth, min(MaxEncoderTeeth, getParamUnsigned(ConstEncoderTeeth)));
}

uint16_t Strategy::mem(bool alloced) {
	return alloced ? 0 : sizeof(strategy);
}
