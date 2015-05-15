#include "efi_id.h"

void setParamShort(uint8_t i, int16_t v);
void setParamUnsigned(uint8_t i, uint16_t v);
void setParamDouble(uint8_t i, double v);

int16_t getParamShort(uint8_t i);
uint16_t getParamUnsigned(uint8_t i);
double getParamDouble(uint8_t i);
bool isParamSet(uint8_t i);

void clearParamChanges();
void clearParamCache();

uint16_t initParams();
void sendParamLookups();
void sendParamValues();
void sendParamChanges();
void sendParamList();
void sendParamStats();

void setSensorParam(uint8_t id, uint16_t adc);
