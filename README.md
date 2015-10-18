# ppk_usb_lufa
Palm Portable Keyboard USB conversion
=====

![keyboard](/pictures/webcam-1.jpg)

inspired by [this great project](http://www.cy384.com/projects/palm-keyboard.html) ([featured on Hackaday.com](featured on HackADay http://hackaday.com/2015/04/04/repurposing-a-palm-portable-keyboard/))

with the main goals/changes:
* keep electronics inside the fold-up case
* make it possible to use multimedia and other key-codes

the later was achieved by porting the arduino code to plain avr-c with lufa for the HID-keyboard handling and half a software serial implementation based on [application note avr309](http://www.atmel.com/Images/doc0941.pdf) (with code snippets [from here](http://www.element14.com/community/docs/DOC-56281/l/avr304-half-duplex-interrupt-driven-software-uart-on-tinyavr-and-megaavr-devices-coding))



parts list
----------
* Pololu A-Star Micro (https://www.pololu.com/product/3101) (a similar sized clone will probably do as well)
* 10k ohm 1/4W resistor
* slim tactile switch as the reset button (http://www.adafruit.com/products/1489)
* micro-usb breakout (http://www.adafruit.com/products/1833)
* wiring-pencil and wire
* 3D printed parts to replace the palm-dock

dis-assembly
---------
* remove the metal backplate on the left side (side with the palm-docking port) - take care not to cut yourself on the sharp edges
* remove the two screws connecting the fold-up stand to the keyboard followed by the two holding the docking-port
* free the flexible flat cable from the plastik parts (might need to cut some plastik away)
* desolder the spring-pins from the flat-cable

![ditched parts](/pictures/webcam-6.jpg)

* to be able to fit the Micro into the tight space its micro-usb plug needs to be desoldered (... fun)

![desoldered micro-usb](/pictures/webcam-5.jpg)

wiring
--------
since these keyboards where built for multiple kinds of PDAs the dock-adapters had to be easily exchangable - that seems to be the reason why the flat cable running from the dock itself is joined to another "commmon" 6-wire flex cable with a special conductive tape that runs the remaining length around a bend to the keyboards pcb on the other side

this common flex cable has the following pinout
viewed from above - "through" the keyboard; from left to right:
  > F1 = VCC
  > F2 = RX_PIN
  > F3 = RTS_PIN
  > F4 = probably TX_PIN, unused anyway
  > F5 = PIN_DCD
  > F6 = GND

on the Micro the following pins are used:
> reset to GND via the tactile switch
> F1 = VCC = Pin 5
> F6 = GND = Pin 6
> F2 = RX = Pin 3 (because of INT0)
> F3 = RTS = Pin 2
> F5 = DCD = Pin 4
> pulldown resistor from Pin 8 to Pin 3


assembly
-------
* wire the usb-breakout to the Micro (in the final assembly the former usb port faces the micro-usb breakout)
* wire the Micro to the flex cable where the spring-pins of the docking connector used to be
* on the Micro board Gnd and Reset pins are spaced so that the reset button fits perfectly in between (covering +3V and +5V pins which need to be insulated!) putting the button on the board so that it sits flush 
* remove the support of the usb-port in the 3d printed part
* fixing the Micro and the usb-breakout with some hot-melt glue to the 3d-printed part
* slide in the back-cover (might need to apply some heat to bend it a litte over the electronics, should close up flush with the other 3d printed part)


code compilation
---------
requires gcc-avr and avrdude to compile via the makefile
