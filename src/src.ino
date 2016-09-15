#include "System.h"
#include "Channel.h"
#include "Pins.h"
#include "Tasks.h"

void setup()
{
	Serial.begin(115200);
	Serial.println("CoreEFI v007a");

	initSystem(true);
}

void loop()
{
	runTasks();
}
