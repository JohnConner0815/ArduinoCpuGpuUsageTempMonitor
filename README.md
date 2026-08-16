# ArduinoCpuGpuUsageTempMonitor
Combined display of CPU and GPU usage, each with an ARGB LED strip (WS2812B), as well as a temperature display using VU meters and an ARGB LED strip.
It has several color profiles stored, and you can adjust the brightness. The most recently used settings are saved in the Arduino and loaded the next time it is restarted or the USB cable is reconnected.
It also has the option for a remote control. When using your own remote control you maybe have to change the Hex-codes in the Arduino sketch file. I used the Remote that comes with an Elegoo Uno R3.
The LED strip for the temperature display uses LEDs 1–4 for the GPU and 6–9 for the CPU, while LED No. 5 is always off. Of course, this can be customized in the sketch file. These LEDs are also NOT affected by any colour profile for the usage display, only the brightness will adjusted too.

The Colours for the temperature LEDs are slightly different for the CPU and GPU.
CPU reach red at 90°C while the GPU already shows red when reaching 80°C.
You can change that at the very bottom of the sketch file.
Both starts to blink in red when reaching a "critical" temperature. For CPU it is 96°C or higher, for GPU it is 81°C or higher. You can adjust this at line 340 of the Arduino sketch file.

You need all 3 system files, as well as LibreHardwareMonitorLib.dll and ArduinoCpuGpuUsageTempMonitor.exe, in the same folder for it to work.

Im using an Intel CPU 12th Gen and an AMD Radeon 9060XT, but it should also work with AMD CPUs and NVidia graphic cards (not sure if it work with NVidias 5000 Cards cause it is more difficult to read the HotSpot there)


Individual adjustments needed in the arduino sketch file:
- Number of LEDs for the usage LEDs - Line 22
- Hex-Codes for the remote when using a different remote control: Line 32 and following
- VU-Meter adjustment for the PWM-Signal in line 380 and 381. Current setup is the following:
 - CPU 0°C to 100°C means 0% PWM to 95% PWM (242)
 - GPU 0°C to 100°C means 0% PWM to 90% PWM (229)
- Warning blinking for the temperature LEDs: Line 340 (currently 96°C for CPU and 81°C for GPU)

**Warning**: If you're using a large number of LEDs, you should not use the Arduino's 5V Pin. Depending on the brightness and color, this can damage your Arduino board or USB-Port because the current draw may be too high.
Instead, simply use the power from the 5V line of your PC power supply, as shown in the chematic.

![](https://github.com/JohnConner0815/ArduinoCpuGpuUsageTempMonitor/blob/main/CPUGPUUsageTempMonitorChematic.jpg)
1. Open ArduinoCpuGpuUsageTempMonitor.ino and adjust the number of LEDs for the Usage-LEDs in line 22 “// --- HARDWARE CONFIGURATION ---” depending on how many you want to control. The scaling is done automatically. Save this to your file. Also adjust other things as described earlier.
2. Upload ArduinoCpuGpuUsageTempMonitor.ino to your board
3. Run ArduinoCpuGpuUsageTempMonitor.exe as administrator (otherwise, the temperatures probably won't be read)- it should already show you the Usage and Temps - select the correct Com-port, the refresh rate (0,1sec up to 1sec) and press connect

![](https://github.com/JohnConner0815/ArduinoCpuGpuUsageTempMonitor/blob/main/ArduinoCpuGpuUsageTempMonitorProgram.jpg)

4. Select the colour profile for the Usage LEDs by pressing the Button which is connected to Pin 3 or use one of the numbers on the remote control
5. The following colour profiles are available: 
 - 1 White - The number of lit LEDs indicates the load
 - 2 White - All LEDs are lit, with their brightness indicating the load
 - 3 Blue - The number of lit LEDs indicates the load
 - 4 Blue - All LEDs are lit, with their brightness indicating the load
 - 5 Cyan - The number of lit LEDs indicates the load
 - 6 Cyan - All LEDs are lit, with their brightness indicating the load
 - 7 Thermometer style with green up to 50%, then yellow at 75% and red at 90% and higher
 - 8 Similar to 7 but all used LEDs lit up in the colour depending on the GPU-usage
 - 9 All LEDs lit up always, but change the colour depending on the load
 - 0 All LEDs are off
6. Select your brightness by pressing the Button connected to Pin 4 or use the up/down Button on the remote
7. Brightness adjustment is logarithmic; it is therefore more of a gamma correction, which is more easily perceived by the human eye.

Button Function:
 - Button 1 (Pin3) - Switching to next colour profile
 - Button 2 (Pin4) - Increase brightness
 - Button 3 (Pin12) - Pressing for 1 second starts a Test sequence with all Colour profiles


Remote control buttons:
 - Numbers 0-9 - Select the specific colour profile
 - Up/down - Change the brightness
 - Next/Previous - Switching between the colour profiles
 - Power Button - Turn the LEDs off/on
 - EQ-Button - Pressing at least 1 sec will start the Test sequence
![](https://github.com/JohnConner0815/ArduinoCpuGpuUsageTempMonitor/blob/main/IMG_20260815_0550492.jpg)
