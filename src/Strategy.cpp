#include "System.h"
#include "Channel.h"
#include "Params.h"
#include "Metrics.h"

#include "Codes.h"
#include "Strategy.h"
#include "Decoder.h"
#include "Events.h"
#include "Schedule.h"
#include "efi_types.h"

static const int TimesMax = TimeMovingSeconds - TimeOnSeconds + 1;

static struct {
	uint16_t times[TimesMax];
	uint8_t run;
} strategy;

static const double CUIN_TO_CUFT = 1.0 / (12 * 12 * 12);
static const double KG_TO_LB = 1.0 / 2.20462262;

static const double STD_LB_FT3 = 0.080672; // lbm / ft^3 density of // air at 32f
static const double STD_LB_IN3 = STD_LB_FT3 * CUIN_TO_CUFT; // lbm / // in^3

static const double DIV100 = 1.0 / 100.0;
static const double DIV600US = 1.0 / 600000.0;

static inline double safediv(double v1, double v2) {
	return v1 == 0 || v2 == 0 ? 0 : v1 / v2;
}

/*
 static const double INHG_TO_G = 36.2;

 static double getCFM(double kgHr, double inHg, double degf) {
 return kgHr * getDegreesRankine(degf) / (inHg * INHG_TO_G);
 }

 static double getKgHr(double cfm, double InHg, double degf) {
 return cfm * InHg * INHG_TO_G / getDegreesRankine(degf);
 }
 */

static inline double calcMPH() {
	static const double PI = 3.14159265358979323846;
	static const double ratio = PI * 60 / (5280 * 12);

	int gear = getParamUnsigned(SensorGEAR);

	if (gear > 0) {
		double r = getParamUnsigned(SensorRPM);
		r /= (getParamDouble(FuncTransmissionRatio) * getParamDouble(ConstAxleRatio));
		return getParamDouble(ConstTireDiameter) * ratio;
	}

	return 0;
}

static inline uint16_t getTimer(uint8_t id) {
	int idx = id - TimeOnSeconds;
	assert(idx >= 0);
	assert(idx < TimesMax);

	if (!strategy.times[idx])
		return 0;

	return tdiff16(getParamUnsigned(TimeOnSeconds), strategy.times[idx]);
}

void setTimer(uint8_t id, bool enable) {
	int idx = id - TimeOnSeconds;
	assert(idx >= 0);
	assert(idx < TimesMax);

	bool isset = strategy.times[idx] != 0;

	if (enable != isset) {
		uint16_t epoch = !enable ? 0 : getParamUnsigned(TimeOnSeconds);
		strategy.times[idx] = epoch;
		setParamUnsigned(id, epoch);
	}
}

static double getDegreesRankine(double f) {
	const static double rakine = 459.67;
	return rakine + f;
}

static inline double getAirDensityRatio(double f) {
	return getDegreesRankine(32) / getDegreesRankine(f);
}

static double getFinalAirFuel() {
	double ratio = 0;
	ratio += getParamDouble(TableFuelBase);
	ratio += getParamDouble(TableFuelStartup);
	return ratio / getParamDouble(ConstLamda);
}

static uint16_t getFinalSparkWidth() {
	static const double DIV720 = 1.0 / 720.0;
	double lo = getParamDouble(FuncMinLowSpeedDwell) * DIV720;
	double hi = getParamDouble(FuncMinHighSpeedDwell) * DIV720;
	uint16_t rpm = getParamUnsigned(SensorRPM);
	uint16_t idle = getParamUnsigned(ConstIdleRPM);
	uint16_t max = getParamUnsigned(ConstMaxRPM);
	double dwell = min(hi, lo + (rpm - idle) * (hi - lo) / (max - idle));
	dwell = 5000 + dwell * 1000; // 5-6 ms, ls1 coil
	return dwell;
}

static double getFinalPulseWidth() {
	double ms = getParamDouble(TableAccelEnrichment);
	ms += getParamDouble(FuncCrankingFuelPulseWidthVsEct);

	double r = 0;

	if ((r = getParamDouble(FuncOpenLoopFuelMultiplierVsAct)) > 0 && r != 1)
		ms *= r;
	if ((r = getParamDouble(FuncOpenLoopFuelMultiplierVsRpm)) > 0 && r != 1)
		ms *= r;
	if ((r = getParamDouble(FuncCrankFuelPulseWidthMultiplier)) > 0 && r != 1)
		ms *= r;

	return ms;
}

static double getFinalSparkAdvance() {
	double deg = 0;
	deg += getParamDouble(TableSparkLimp);
	deg += getParamDouble(TableSparkAltitude);

	if (deg == 0)
		deg += getParamDouble(TableSparkBase);

	deg += getParamDouble(FuncWotSparkAdvanceVsRpm);
	deg += getParamDouble(FuncWotSparkAdvanceVsAct);
	deg += getParamDouble(FuncWotSparkAdvanceVsEct);
	deg += getParamDouble(FuncSparkAdvanceRateVsRpm);
	deg += getParamDouble(FuncSparkAdvanceVsAct);
	deg += getParamDouble(FuncSparkAdvanceVsBp);

	return deg;
}

void expireCached(uint8_t id) {
	extern void clearCached(uint8_t id);

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
		case SensorIsCranking:
			clearCached(FuncCrankFuelPulseWidthMultiplier);
			clearCached(FuncCrankingFuelPulseWidthVsEct);
			break;
		case FlagIsClosedLoop:
			clearCached(FuncOpenLoopFuelMultiplierVsAct);
			clearCached(FuncOpenLoopFuelMultiplierVsRpm);
			break;
	}
}

double getStrategyDouble(uint8_t id, ParamData *pd) {
	extern double lookupParam(uint8_t id, ParamData *pd);
	extern double dblDecode(ParamData *pd, uint16_t v);
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
				if (!isParamSet(SensorIsCranking))
					return 0;
				break;
			case FuncOpenLoopFuelMultiplierVsAct:
			case FuncOpenLoopFuelMultiplierVsRpm:
				if (isParamSet(FlagIsClosedLoop))
					return 0;
				break;
		}

		return lookupParam(id, pd);
	}

	switch (id) {
		case CalcFinalPulseWidth:
			return getFinalPulseWidth();
		case CalcFinalPulseAdvance:
			return getParamDouble(TableInjectorTiming);
		case CalcFinalSparkAdvance:
			return getFinalSparkAdvance();
		case CalcFinalSparkWidth:
			return getFinalSparkWidth();
		case CalcFinalLamda:
			return getFinalAirFuel();

		case CalcLoad:
			return !isParamSet(TimeRunSeconds) ? 0 : 100 * safediv(getParamDouble(CalcVolumetricRate), getParamDouble(CalcTheoreticalRate));
		case CalcVolumetricRate: // ratio * kg/hr * lb/kg = lb/hr
			return getAirDensityRatio(getParamDouble(SensorACT)) * getParamDouble(FuncMafTransfer) * KG_TO_LB;
		case CalcTheoreticalRate: // lb/in3 * in3 * tdc/min * 30 = lb/hr
			return getParamDouble(ConstCuIn) * getParamUnsigned(SensorRPM) * STD_LB_IN3 * 30;
		case CalcDisplacementVolume: // in3 * tdc/min * ft3/in3 = ft3/min
			return getParamDouble(ConstCuIn) * getParamUnsigned(SensorRPM) * CUIN_TO_CUFT / 2;

		case CalcDutyCycle:
			return !isParamSet(TimeRunSeconds) ? 0 : getParamDouble(CalcFinalPulseWidth) * getParamUnsigned(SensorRPM) * DIV600US;

		case CalcLbsPerHr:
			return !isParamSet(TimeRunSeconds) ? 0 : getParamUnsigned(ConstCylinders) * getParamDouble(ConstInjLoSlope) * getParamDouble(CalcDutyCycle) * DIV100;
		case CalcMPG:
			return !isParamSet(TimeRunSeconds) ? 0 : safediv(100 * getParamDouble(SensorVSS), getParamDouble(CalcGalPerHour));
		case CalcGalPerHour:
			return !isParamSet(TimeRunSeconds) ? 0 : safediv(getParamDouble(CalcLbsPerHr), getParamDouble(ConstGallonWeight));

		case CalcThrottleRate:
			return 0;
		case FlagIsWOT:
			return !isParamSet(TimeRunSeconds) ? 0 : getParamDouble(SensorTPS) >= getParamUnsigned(ConstWOT) ? 1 : 0;

		case FlagIsHighAlt:
			return getParamDouble(SensorBAR) >= getParamDouble(ConstHighAlt);

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
		case CalcEPAS:
			return uint16Decode(pd, pd->value);
	}

	if (pd->cat == CatConst || pd->cat == CatSensor)
		return dblDecode(pd, pd->value);

	if (pd->cat == CatTime) {
		if (id == TimeOnSeconds)
			return uint16Decode(pd, pd->value);
		return getTimer(id);
	}

	assert(id);

	codes.set(id);

	return pd->value;
}

uint16_t initStrategy() {
	myzero(&strategy, sizeof(strategy));

	setParamUnsigned(ConstCoils, min(MaxCoils, getParamUnsigned(ConstCoils)));
	setParamUnsigned(ConstCylinders, min(MaxCylinders, getParamUnsigned(ConstCylinders)));
	setParamUnsigned(ConstEncoderTeeth, min(MaxEncoderTeeth, getParamUnsigned(ConstEncoderTeeth)));

	return sizeof(strategy);
}
