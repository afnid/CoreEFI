
# CoreEFI
An electronic fuel injection program written in c++ to see if sequential fuel injection with 1 us resolution could be achieved on an Arduino 16 MHZ Mega or 84 MHZ Arduino Due. This has also been run on a STM32F4 407 Cortex processor, but not fully implemented.  This has never been tested with any actual engine, so can't be considered much more than a proof of concept at this point.

Some distinguishing features:

<ol>
<li>Utilizes variable length timers to minimize event scheduling overhead.</li>
<li>Low impact metrics track performance rates and time spent in critical sections.</li>
<li>Complete console interface allows viewing stats on all major components.</li>
<li>All console output is in a json style which makes it very easy to parse.</li>
<li>Table driven architecture makes it easy to add new variables, tables, etc.</li>
<li>External xml config file used to generate the parameter lists and table data</li>
<li>Cylinders only limited by the number of events that can be processed at max RPM.</li>
<li>Contains all tables/functions from a Ford EEC-IV computer using a MAF system.</li>
<li>Most viable platform implemented right now is the Arduino Due at 84 MHZ</li>
<li>Will compile and run on a 16 MHZ Arduino Mega using about 4k of ram and 40k of code.</li>
<li>Compiles with gcc-arm-none for the STM32F4 series of ARM processors</li>
<li>Can compile and run on the Linux command-line for testing/simulation.</li>
</ol>

Initial development started on an Arduino UNO, but switched to a Mega because 2k of ram is just not enough!  Then switched to the Due because the Mega timers combined with the 16 MHZ clock rate was pathetic.

The original system I used for my model also runs at 16 MHZ, so looking forward to seeing how accurate it really is.  I would hope most of it is written in assembly and with much better timer resolution.  Still I would like to think that I can make the Arduino Mega perform better than this 25 year old efi computer.

<h2>Architecture</h2>

Here are some of the most major implementation details:

<h3>Table Driven</h3>

All of the variables that may be logged or used to calculate other values exist in a single enumeration with a table of meta data for their min/max and scale data.  All access is done through convenience routines creating a parameters database instead of spreading the fields across various data structures.  Meta-data is verified for all access to make sure the correct types are used.

The strategy queries values and returns a value for computed parameters.  This is very easy to extend by adding new parameters to the enumeration, the meta-data, and any calculations.  Any parameter can be extended to be a lookup table, function, or translation.

Caching and write-through cache flushing can be easily implemented within the parameter list.  Some of this is partially enabled, but it defeats the performance testing given many of the inputs don't change during simulation.

All of the parameters exist in a Java program and the enumeration and meta data are exported to an xml file.  The same program is used to generate the source code for the parameters and all of the data storage and various indexes.  The data structures were designed for a low memory footprint since it was originally implemented on an Arduino UNO.

The table driven approach allowed for many features to be provided with a reduced code footprint.  Even storing much of the data in program memory.  This means anything defined in the parameter list can be read/written using the console.

There is some additional overhead since the parameters are encapsulated in a simple database for the rest of the application to use.  There is additional overhead since it does range checking, type checking and storage conversions.  It is best to not access the parameters database from within an isr.

<h3>Simplified Decoder/Event Math</h3>

The decoder handling is actually the most critical piece since if there is drift/error in the decoder, the accuracy in scheduling no longer matters.  In some other implementations it appears they convert from a time basis to degrees, and appear to calculate the absolute degrees for each pulse event.  A large amount of effort goes into trying to calculate the current RPM.  The events are always calculated in degrees and then converted to time at the last.

Current RPM is something that will result in each pulse getting shorter/longer as the engine accelerates/decelerates.  The RPM at the beginning of a cycle will be different than the RPM at the end, so if RPM is calculated only once per revolution, the error will compound until the RPM is calculated again.  RPM exact degree calculations require division which can make these more expensive time-wise.

The current approach uses a cyclic buffer to store the pulses and for each event, subtract the last pulse time and add the current pulse time to a running total.  The new total represents the time in the last cycle covering 720 degrees.

A correction factor may need to be added to project current rpm. Maybe keeping the relative difference between the preceding n pulses to calculate a percentage change to the cycle time.  Not implementing anything for projecting rpm until I can be working with real data.  This may add a division operation.

The benefits of the current approach is there is 0 division and 0 floating point calculations required in the collection of pulses or in the translation from event degrees to absolute time.  So 0 division/floating point calculations in the isr's.

A downside is that the accuracy of the tdc pulse is very important since it is used as the basis to convert degrees to absolute time.  Jitter in the encoder/decoder would be difficult to compensate for.

<h3>Single Event Scheduler</h3>

The event handler is designed to operate in a separate thread either from a coprocessor, or driven from an interrupt.

Each event has two parts, an absolute start which specifies degrees from 0 to 720, and then a duration part in ticks.  So for a fuel/spark on each cylinder, there are actually 4 different events.  When a schedule is computed the leading events are stored with an angle only, the follow-up event contains the same angle and the duration.  These values are stored in an unsigned 2 byte int, so 0 to 720 is stored as 0 to 65535.  This simplifies phase issues where events wrap around by leveraging the natural wrapping when working with unsigned values.

A strategy calculates the 4 events for each cylinder.  An estimated event time is calculated for sorting, but the final time is computed one or more times before it actually occurs.  For example, the strategy decides the spark should happen 20 deg btdc and the dwell should be 5 ms and the injector should open at 360btdc and stay open for 2 ms.  That information is stored in the events and can be used to calculate the absolute time relative to the latest rpm.

This allows the strategy to be completely asynchronous with the events, which is required because the strategy is compute intensive and may not be able to calculate each cycle and second the rpm is changing through the cycle, so computed times need to adjust accordingly.  For example, 20,000 rpm means the crankshaft makes a rotation in 3ms, and so a cycle will take 6 ms.  The compute time for a complete strategy on an Arduino Mega is about 6 ms, but because the cpu is saturated with event processing, it may take twice as long.  The strategy calculation time does not scale up/down with the number of cylinders.

<h2>Strategy</h2>

The initial strategy is based on extracted eeprom data from a Ford EEC-IV computer and then implemented using a best guess approach, in other words, it is incomplete and maybe not even accurate.  For example, their dwell numbers didn't make any sense at all, so used a 5-6 us dwell because that is what a ls1 coil requires.  The injector durations look a little long, and the lamda calculation is not incorporated yet, so no attempt at closed-loop operation.

The positive side is that this will flush itself out pretty quick with some empirical knowledge, and the table driven architecture makes this very easy to change/extend.  The main concern at this point was whether the values looked somewhat believable, and there was something computed in every case where it needed to be to get an accurate performance profile.

<h2>Accuracy</h2>

So how accurate does it have to be?  Of the 4 events per cylinder, the timing of the injector open, spark and dwell can probably tolerate quite a bit.  At 8000 rpm, a 0.1 degree accuracy can be maintained with 2.08us of jitter .  For pulse width 2us is 0.05% per ms of duration, 40us for 1%.  I found a ms2 reference of 0.9 deg at 7,500 and older code revs being 3x as bad.  That was on a 40 MHZ processor, but the newer ms3 uses the xgate co-processor, and should be much more consistent.

At 8000 rpm, 0.5 degree accuracy requires the jitter to be 10.42us or less.  That would be against 7500 us/rev, or 15000 us/cycle.  The issue isn't whether any given processor can run an efi system at idle, the issue is what is the maximum rpm given the number of events per cylinder?  Full sequential requires 32 events for 8 cylinders, cut that in half for semi-sequential/wasted spark.  At high rpms, I am not sure if semi would make much difference in fuel flow, especially once the duty cycle reaches 100%.

Half the events will cut isr contention in half.  It may be that a batch mode is required during cranking, then run full sequential up to a certain rpm, then switch to semi-sequential after that.  Wasted spark can also cut the coil events in half, so an 8 cylinder would require 16 injector events, and only 8 coil events.

There is a bmw v8 that can spin 20k rpm, and 0.1 degrees of error would equate to 0.8 us, but for your more standard 6k redline, you could have 2.8us of jitter and any accuracy beyond that is insignificant relative to other errors within the rest of the system.

<h3>The Problem</h3>

The issue is that with a single processor and multiple interrupts, there will always be the potential for contention.  And the maximum run-time of one isr will set the maximum error in the event scheduler isr.

For example on a 84 MHZ due, the encoder/decoder simulated timer interrupt takes between 9-32 us with 6 us required for the isr overhead, that means the scheduler isr can be 40 us late.  A correction factor can be added to try to wake early and then sleep, but that extends the isr time, hurting the decoder isr.

The sleep method works well to reduce errors at lower rpms where there is lower contention, but hurts higher rpms where it really matters and has an adverse affect on other isr routines like the decoder timing.  Corrupt the decoder timing, and all other accuracy attempts are meaningless.

The best solution so far is to have them run on a separate thread with a coprocessor or multi-core cpu or just get a processor that is so fast that everything runs under 1 us.

<h2>Platforms</h2>

This is called CoreEFI because the focus was on the main components of any efi system, how they interacted, and the final accuracy of the results.  Significant time has been spent trying to reduce error, and measure error accurately within the software without skewing the results.  Uncertainty principal applies here.  There could be more errors induced by the hardware when measured on a scope, but hopefully the worse case scenario is already known from inside the cpu.

There are only a handful of methods required to port this between platforms, and the entire system can be simulated by running the isr routines direct from the main scheduler.  Once it is running from the scheduler, move the encoder/event calls into timer routines and it will start showing the error rates.

Simply clone this project and execute the Makefile to run on Linux, or to run on an Arduino Mega/Due, just load the src.ino file.  Should work on cygwin, but that would require me to actually run Windows.  I could at least verify on the mac.

<h3>Linux</h3>

Values are initialized to support a running state, but any value can be overwritten using the prompt.  The events process and an encoder simulator are run from the main loop scheduler instead of from interrupts.  This mode is primarily for rapid development, and profiling or using avrgrind.  The linux process is also used to test the Java interface.

<h3>Arduino Due</h3>

The Due has 32bit timers so there is no requirement to ever use the pre-scalars in this application.  Also the interrupt overhead was optimized down to 6 us.  Still the decoder/encoder took 9-30 us, and the scheduler took closer to 40 us.  Giving a worse case of about 40 us when running 8 cylinders sequential at 22,000 rpms, with about a 50% error rate.  The worse case was around 15us at 10,000 rpms with a 33% error rate.  Still not great numbers, but far better than the 16 MHZ Mega.  These times will improve using an external interrupt for the decoder by reducing the time a little more.

Here are some stats from the program for 8 cylinders showing the min/max late and degrees of error vs rpm:

<pre>
-late=  0	+late=  0	rpm=    2194	-deg=   0.00	+deg=   0.00	late%=  0.00
-late=  0	+late=  8	rpm=    3982	-deg=   0.00	+deg=   0.19	late%=  6.24
-late=  0	+late=  13	rpm=    6750	-deg=   0.00	+deg=   0.53	late%=  6.22
-late=  0	+late=  21	rpm=    9874	-deg=   0.00	+deg=   1.24	late%=  32.57
-late=  0	+late=  39	rpm=    22300	-deg=   0.00	+deg=   5.24	late%=  51.59
</pre>

Note that this is with running an encoder simulator that runs the decoder.  The encoder has some round-off error and some jitter, thats why the rpm is not a whole number.  An external interrupt should have a little less overhead and should reduce the error some.  But this is with a 12 tooth decoder, there will be more contention with a 36 or 60 tooth decoder.

2200 rpm's appears to be the cross-over where there is an occasional late every few seconds, anything below that is 100%.

A 1/2 degree of maximum error for 6% of the time is acceptable for an 8 cylinder which would be topped out at under 7000 rpm. 

At 22k rpm the durations hit 100% duty cycle so the events get queued.  Queued isrs is what causes the late time to increase.

Here are some stats for running with 1 cylinder:

<pre>
-late=  0	+late=  17	rpm=    22484	-deg=   0.00	+deg=   2.29	late%=  50.04
-late=  0	+late=  15	rpm=    9898	-deg=   0.00	+deg=   0.89	late%=  25.05
-late=  0	+late=  0	rpm=    8326	-deg=   0.00	+deg=   0.00	late%=  0.00
</pre>

Somewhere above 8,400 rpm's, the events qet queued.  Again this is a 12 tooth decoder and simulated encoder.

And for a little fun, 16 cylinders, 60 tooth decoder:

<pre>
-late=  0	+late=  12	rpm=    3914	-deg=   0.00	+deg=   0.28
</pre>

Couldn't get reliable results above this rpm, a good stress test!

<h3>Arduino Mega</h3>

On the AVR based Arduino within the timer ISR you only get one read on the clock, any successive reads return the same value .. although the latest ide version appears to be working different, or maybe because I am enabling interrupts.  Using a regularly timed interrupt had to be at a very high frequency to maintain any accuracy and quickly saturates the cpu so a variable length timer was used to try to have one wakeup per event.

The problem is that if you don't know the time at the end of the isr, you don't know how long to sleep.  Based on being early/late a correction factor is subtracted to try to assure that in all cases the timer wakes up early, never late.  Also for jumps beyond 4096 us a prescaler is required so an intermediate sleep is used to come within 4k of the target time.  To compensate for being early, a delay was used, which extended the isr time and created additional contention.

Bottom line, simulating an 8 cylinder engine with 4 events per cylinder, open/close the injector and charge/fire the coil I was able to get 1 us resolution nearly 100% of the time at idle, high 98+ at about 2k rpm, and then it dropped dramatically after that.  That may translate to 1 cylinder at 16,000 rpm, but probably other limitations will make it far lower.  This was using the early wakeup/sleep method.  The variable timer would use as much lead-in as it could get, .5ms, or even a full ms because the worst case was so bad with the delays.

Using some timer test code that only would wakeup and immediately sleep, the timer overhead appeared to be around 40 us.  When running multiple timers, successive timers were adversely affected and the lead times required to stay on time would grow to 100-200us without doing any real work.

Given the mega and due have the same form factor and not that much difference in price, there is no reason to continue with a mega.

<h3>STM32F4</h3>

Running on a dev board to measure performance, getting a solid 10x+ performance over the mega which makes most performance issues go away.  Have not implemented the variable length timer yet, but just running from the main loop task scheduler it was performing on-time 99+% of the time at idle, which is more than the Mega could do with timers.  Makes sense since the strategy calc was still taking longer than the longest scheduler interval, but what took 6ms on an Arduino is happening in less than 600us on the 168 MHZ 407.  Using a USB serial example using the older ST libraries, trying to get the second USB to work with the CubeF4 crap.




<h2>Other Solutions</h2>

There are at least 4 other systems out there that actually have run engines, but are already heavily invested in their own approaches.  This is a quick synopsis of what I have seen so far:

<ul>
<li>diyefi/freeems - I think the oldest project and the most vehicle installs.  Uses the freescale processor, but not using xgate, so the implementation is using all of the output compare interrupts to drive the timing.  Only 6 total and one needs to be used for the decoder, so it limits the number of cylinders that can be run independently.  The code appears very convoluted and difficult to read.  Being the oldest, it is already tied to it's own legacy and not pushing the state of the art.  A positive is that they have a board design with units available to purchase.  The community is still active.</li>

<li>open5xxxecu.org - On the same or similar cpu as diyefi.  There is lots of posts in their forums, but new posts are far and few between and it does not look like their is hardware available.  The code appears to be using some embedded rtos on a freescale processor but not sure if it is the same as diyefi with xgate or not.</li>

<li>libreems - this is just a branch from diyefi which fractured the efforts there.  There is some advancement with using the xgate co-processor, but the community is also pretty quiet.  They are also on the same cpu as diyefi.</li>

<li>rusefi.org - the newest of the projects, appears to have a lot of momentum and using the same stm32f4 407 processor.  Hardware is available for sale and already on a second revision.  They are using chibios for a rtos and still not sure if that is a positive or a negative.  I see they also wrote their own display software and have a simulation mode so we were on parallel paths.  They compare themselves as the iphone to the newton, being the cortex arm solution vs the freescale processors used by all of the above and the megasquirt software.  I am still skeptical that 1us can be achieved in a uni-processor approach.</li>

</ul>




<h2>Summary</h2>

Knowing that 1 us resolution is attainable even sometimes on a 16 MHZ arduino tells me that the best case timing can be attained nearly anywhere, it is really the worse case that matters.  Any time interrupts get queued waiting for another to finish, it is going to be late in processing.  Interrupt contention will always create situations where one isr is waiting for another and that drives your worse case.

The freescale processor with the xgate co-processor running at 40 MHZ can probably attain a higher accuracy, more of the time, than the 168 MHZ arm if implemented correctly because it should have 0 interrupt contention.  Also frees up the isr for the decoder which is the most important.  I may investigate this further once the stm32 is tested.

Of the platforms above, I don't think the 16 MHZ avr is viable except for maybe low cylinder counts, and limited rpms.  The 84 MHZ Arduino Due looks like a viable platform for a v8.  The st arm is double the clock rate of the due, so should be able to cut all the error rates in half, so maybe running this on the rusefi hardware will be the final platform.

<h2>What's Next?</h2>

The 25 year old target engine is not close to running yet in the 50 year old car, so it will be awhile before I can test it with real sensors.

This started as a simple fan controller and the desire to be able to tap into all the instrumentation in the efi computer instead of creating a parallel system of sensors.  There are other desired functions like running a tft to provide cockpit feedback and controlling fans, epas, and other modern features without having to add lots of buttons/gauges to a very old car.

The plan is to eventually integrate this into the existing system passively and then later replace the EEC-IV entirely.

Next Steps:

<ul>
<li>Implement the timers on the stm32 and get performance metrics</li>
<li>Flush out the strategy computations and incorporate my fan controller</li>
<li>Get the java client program released as open source, the program is easy, it's the libraries I am using</li>
<li>Perhaps get a freescale dev board and implement an xgate scheduler to investigate the maximum rpm</li>
<li>Integrate this with an actual engine to see where I have over simplified things</li>
</ul>

