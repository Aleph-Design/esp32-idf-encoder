# ESP32-IDF implementation of a Rotary Encoder as State Machine
Marko Pinteric provided a proposal an how to implement a rotary encoder as a state machine.
For details and explanation see: https://www.pinteric.com/rotary.html

## First try
The first try was on the Arduino IDE, which was easy and without any hasle.
It just worked more or less "out of the box."

## Programming in ESD-IDF
Programming in ESD-IDF just the way I did in Arduino IDE, wasn't very successfull the way I expected.
The error bugging me was:
- Task watchdog got triggered. The following tasks/users did not reset the watchdog in time:
- - CPU 1: rotaryTask;
  -  Print CPU 1 backtrace.

When I implemented the handling of the rotary encoders shaft movement through a queue all went well.

## Output
- [0;32mI (304) main_task: Calling app_main()␛[0m
- [0;32mI (304) gpio: GPIO[26]| InputEn: 1| OutputEn: 0| OpenDrain: 0| Pullup: 1| Pulldown: 0| Intr:3 ␛[0m
- [0;32mI (304) gpio: GPIO[27]| InputEn: 1| OutputEn: 0| OpenDrain: 0| Pullup: 1| Pulldown: 0| Intr:3 ␛[0m
- Counter: 0
- [0;32mI (314) main_task: Returned from app_main()␛[0m
- Counter: 1
- Counter: 2
- Counter: 3
- Counter: 4
- Counter: 3
- Counter: 2
- Counter: 1
- Counter: 0
- Counter: 9
- Counter: 8
- Counter: 7
- Counter: 8
