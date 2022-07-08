# About this project
This was a project for a course, "Programming and Implementation of Equipment for Mechatronics Applications", back then in 2015, one of the most interesting courses as a student in my career, Mechatronics Engineering.

## Platform
Code was written in C, executed in a Raspberry Pi 2 running a modified Raspbian OS with RTLinux kernel.

## Scope of the project
Sync two DC motors to make them able to couple their "gears".
The main DC motor, which has a fixed position on the rail, is setted up to rotate at a specific speed (hardcoded).
The secondary DC motor, will receive the angular position of the main DC motor as input and will try to sync using a simple PID algorithm.

You can watch the project working on Youtube: https://www.youtube.com/watch?v=_awFl0KSneI

## Technical specifications of DC Motors
Model: DSE38BE27-001
Voltage (Máx): 24V
Encoder: 90 HIGH and 90 LOW pulses per rev.

## Using wiring.pi library
Wiring Pi allows you to access Raspberry Pi's GPIO through a C/C++ interface.
To compile programs using Wiring Pi library use -lwiringPi
To use Gertboard, MaxDetect, etc. add -lwiringPiDev
Source: http://wiringpi.com/

PD: Comments in the code are in Spanish.