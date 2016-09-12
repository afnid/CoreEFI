// Generated code, don't edit

typedef enum {
	CatVehicle,
	CatSensor,
	CatCalc,
	CatFlag,
	CatTime,
	CatTable,
	CatFunc,
	CatConst,
} CatType;

typedef struct _LookupHeader {
	uint16_t c1;
	uint16_t c2;
	uint16_t r1;
	uint16_t r2;
	uint16_t offset;
	uint8_t length;
	uint8_t rows;
	uint8_t cols;
	uint8_t cid;
	uint8_t rid;
	uint8_t interp;
	uint8_t last;
} LookupHeader;

typedef struct _ParamData {
	uint16_t value;
	uint16_t min;
	uint16_t max;
	uint16_t hdr:7;
	uint16_t neg:1;
	uint16_t cat:3;
	uint16_t div:4;
} ParamData;

