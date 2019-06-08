# FDL-2X-Blaster

To update FDL-2X firmware you need the following:

[Sparkfun Microview Programmer](https://www.sparkfun.com/products/12924)<br/>
[XLoader](http://xloader.russemotto.com/)<br/>
Windows computer. (There are ways to do this on a mac but I have not found a quick and easy utility to do so)<br/>
<br/>
#### Flashing your FDL-2X</br>
#### Caution!!! Flash your blaster at your own risk. Doing so is not covered by any sort of warranty. If you are not comfortable or 100% confident, consider sending your Microview to Project FDL to flash.</br>
-Remove the rear blaster cap and Microview. <br/>
-Insert the Microview programmer into the USB port on your Windows computer. <br/>
-Insert the Microview from your blaster into the programmer paying careful attention to orientation.<br/>
-The screen on your microview should point up with the USB connector facing left. There is a picture indication on the board as well.<br/>
-Run XLoader. <br/>
-Choose your desired firmware hex file. <br/>
-Select the Duemilanove/Nano(ATmega328) option under device.<br/>
-Choose the com port of your USB programmer.<br/>
-Set baud rate to 115200.<br/>
-Click upload.<br/>
-You should see the lights under the screen flickering. Flashing can take 30 seconds or longer.<br/>
-When flashing is complete you will see the FDL-2X splash screen then the speed menu as usual.<br/>
-Remove the Microview from the programmer and put it back in your FDL-2X.<br/>
-Replace the rear cap and you're good to go.<br/>
