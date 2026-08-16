# ArduinoCpuGpuUsageTempMonitor
Combined display of CPU and GPU usage, each with an ARGB LED strip (WS2812B), as well as a temperature display using VU meters and an ARGB LED strip.
It has several color profiles stored, and you can adjust the brightness. The most recently used settings are saved in the Arduino and loaded the next time it is restarted or the USB cable is reconnected.
It also has the option for a remote control. When using your own remote control you maybe have to change the Hex-codes in the Arduino sketch file. I used the Remote that comes with an Elegoo Uno R3.

![](https://github.com/JohnConner0815/ArduinoCpuGpuUsageTempMonitor/blob/main/CPUGPUUsageTempMonitorChematic.jpg)
1. Open ArduinoCpuGpuUsageTempMonitor.ino and adjust the number of LEDs for the Usage-LEDs in line 22 “// --- HARDWARE CONFIGURATION ---” depending on how many you want to control. The scaling is done automatically. Save this to your file.
2. Upload ArduinoCpuGpuUsageTempMonitor.ino to your board
3. Run ArduinoCpuGpuUsageTempMonitor.exe - it should already show you the Usage and Temps - select the correct Com-port and press connect
4. Select the colour profile by pressing the Button which is connected to Pin 3 or use one of the numbers on the remote control
5. The following colour profiles are available: 
 > - 1 White - The number of lit LEDs indicates the load
 > - 2 White - All LEDs are lit, with their brightness indicating the load
 > - 3 Blue - The number of lit LEDs indicates the load
 > - 4 Blue - All LEDs are lit, with their brightness indicating the load
 > - 5 Cyan - The number of lit LEDs indicates the load
 > - 6 Cyan - All LEDs are lit, with their brightness indicating the load
 > - 7 Thermometer style with green up to 50%, then yellow at 75% and red at 90% and higher
 > - 8 Similar to 7 but all used LEDs lit up in the colour depending on the GPU-usage
 > - 9 All LEDs lit up always, but change the colour depending on the load
 > - 0 All LEDs are off
6. Select your brightness by pressing the Button connected to Pin 4 or use the up/down Button on the remote
7. Brightness adjustment is logarithmic; it is therefore more of a gamma correction, which is more easily perceived by the human eye.
![](https://github.com/JohnConner0815/ArduinoCpuGpuUsageTempMonitor/blob/main/ArduinoCpuGpuUsageTempMonitorProgram.jpg)

Button Function:
 > - Button 1 (Pin3) - Switching to next colour profile
 > - Button 2 (Pin4) - Increase brightness
 > - Button 3 (Pin12) - Pressing for 1 second starts a Test sequence with all Colour profiles


Remote control buttons:
 > - Numbers 0-9 - Select the specific colour profile
 > - Up/down - Change the brightness
 > - Next/Previous - Switching between the colour profiles
 > - Power Button - Turn the LEDs off/on
 > - EQ-Button - Pressing at least 1 sec will start the Test sequence
![](https://github.com/JohnConner0815/ArduinoCpuGpuUsageTempMonitor/blob/main/IMG_20260815_0550492.jpg)
