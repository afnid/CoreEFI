#include "System.h"
#include "Channel.h"
#include "Pins.h"
#include "Tasks.h"

void setup()
{
	initSystem(true);
}

void loop()
{
	runTasks();
}
