# dmurray_padawan360_body_mega
dmurray_padawan360_body_mega control system for R2-D2

Heavily influenced by Dan Kraus padawan 360 code 
Astromech: danomite4047
Project Site: https://github.com/dankraus/padawan360/

I think there are several edits that (if cleaned up) would be good modular pieces of the code which users
could enable/disable with a boolean setting at the top of the sketch depending if they have the hardware.

For example: 
I use two buttons in place of the blue stubs on the dome for muting him or changing his autonomy mode.
I have a hall effect sensor to return his head to the front position (as a timeout, or from the controller)

And of course the 360 keyboard accessory which everybody seems to like.

I probably wouldn't bother with the 'person effect sensor' and related mode though... 
it's worthwhile code to identify the coordinates of a face and move the dome accordingly,
but due to the short range of that type of sensor, it only works if you're <40cm from his dome... 
not really worth it

Dale Murray
