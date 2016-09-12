#ifndef _Display_h_
#define _Display_h_

class Display {
	char display[4][21];

	int menu;
	int row;

	void showLine(int line, char *buf);

public:

	uint16_t init();

	void showDisplay(uint32_t now);

	void menuInput(uint8_t i);
};

extern Display display;

#endif
