# Looker
## Overview
Looker is an Open Source, multi-platform solution for making existing or new embedded projects wireless. Currently Looker supports only WiFi but Bluetooth is planned to be added in the future.

The main goal of this project is to make it easy to use with maximum portability. Thanks to simple API Looker is installed by just adding a few lines of code. For a network communication it useses ESP8266 - popular and readily available WiFi module which is connected to the main board with only four wires.

The system is successfully ported to broad range of Arduino, ARM and PC projects. It is written in C so it can be further ported to any platform where C compiler is available.

Looker is divided into two parts:

1. HTTP server running on ESP8266: *src/looker_wifi*
2. Library to be imported to a custom project: *src/looker*

Looker library and HTTP server are synchronized over a serial port.

Any device with a web-browser installed: PC, smart phone, tablet can be used to control Looker-powered device. No additional application is necessary.

## How does it work
HTTP server dynamically creates a simple website that lists out some variables from the user's code. Only variables explicitly registered to Looker database will be showed on the website. Within the code those variables can be linked to an external sensors, relays, LEDs etc. From the website you can read/write the variables and see instant effect from/on the embedded device e.g. turn a LED, switch a relay, read temperature from a temp. sensor and so on.

## Where can it be used
1. Looker adds a smart and convenient user interface (UI) feature to an embedded project where often this thing is very limited. Instead of connecting a display with bunch of cables, some extra LEDs for output or buttons for input user can have this all conveniently on a remote device e.g. a tablet already equipped with high-res display. If UI requires more I/O they can be simply added in a software rather than in hardware.

2. Not only does Looker let a user control the device from local network but also from the outside World as long as the device’s IP is accessible.

3. Remote debugging. Being able to view and modify variables on fly can make debugging easier specially when a specialized debugger is not available. 

## Licensing
Looker is released under **MIT** open source license.

## Hello World
*/src/examples/arduino/helloWorld*
### This ia a simple Arduino app to read ADC and drive LED from a website:

```C
#define DOMAIN "arduino"
#include "looker.h"
#include "wifi.h"
#include "stubs/looker_stubs.c"

unsigned int adc;
unsigned char led = 0;

void setup() {
    pinMode(LED_BUILTIN, OUTPUT);
    delay(500);     //skip messages from WiFi module during boot up 
    Serial1.begin(115200);
    looker_init(SSID, PASS, DOMAIN);

    looker_reg("ADC", &adc, sizeof(adc), LOOKER_VAR_UINT, LOOKER_HTML_VIEW, NULL);
    looker_reg("LED", &led, sizeof(led), LOOKER_VAR_UINT, LOOKER_HTML_CHECKBOX, NULL);
}

void loop() {
    adc = analogRead(A0);
    digitalWrite(LED_BUILTIN, led);
    looker_update();
    delay(100);
}
```
### The device is now accessible at address: http://arduino.local
<![alt text](https://raw.githubusercontent.com/lfigiel/looker/master/src/examples/arduino/helloWorld/helloWorld.png)>

**.local** is added to the domain by multicast Domain Name System (mDNS) that Looker is using.

## API walkthrough
---
```
LOOKER_EXIT_CODE looker_init(const char *ssid, const char *pass, const char *domain)
```
Connects to the WiFi network. Usually it takes a couple of seconds to complete.
Blue LED on the WiFi module stops blinking when it gets connected.  
details:  
**LOOKER_EXIT_CODE**  
type of exit code. On success the function returns: LOOKER_EXIT_SUCCESS. Full definition is available in *looker.h*

**ssid  
pass**  
credentials to the WiFi network.
It is better to define it outside of the main source code e.g. in *wifi.h*.
In this case you don’t reveal it when sharing the code. Also helps organize when multiple networks are available.  
Example of *wifi.h*:

```
//network 1
#define SSID <ssid1>
#define PASS <pass1>

//network 2
//#define SSID <ssid2>
//#define PASS <pass2>
```
**domain**
sets domain name at which the device advertises. Using domain helps finding the device but is not necessary. If **domain** is skipped:
```
looker_init(SSID, PASS, NULL)
```

device is still present at IP address that was assigned by the access point.
Tool like “Fing” might be helpful in finding this IP address.

---
```
LOOKER_EXIT_CODE looker_reg(const char *name, volatile void *addr, int size, LOOKER_VAR_TYPE type, LOOKER_HTML_TYPE html, STYLE_TYPE style);
```
Registers the variable making it accessible from the website.

**name**  
variable will be showed under this name on the website. The name does not need to be the same as the C variable.
Max size of this name (string) is limited in *looker.h*:
```
#define LOOKER_VAR_NAME_SIZE 16
```
**addr**  
address of the variable

**size**  
size of the variable

**type**  
type of the variable. Looker needs to know what kind of variable it is. Following types are supported:  
  
*LOOKER_VAR_INT*  
all signed integers: char, short, int, long int  

*LOOKER_VAR_UINT*  
all unsigned integers  

*LOOKER_VAR_FLOAT*  
float and double  

*LOOKER_VAR_STRING*  
string

All variables except string can have size of up to 8 bytes (64-bit).
Maximum size – including string is limited in *looker.h*:
```
#define LOOKER_VAR_VALUE_SIZE 16
```
**html**  
specifies how the variable is presented on the website. Following variants are supported:  

*LOOKER_HTML_CHECKBOX*  
checkbox, suitable for bool (TRUE/FALLS) variables  

*LOOKER_HTML_CHECKBOX_INV*  
same as above but with inverted state.This is useful if the checkbox controls a LED. Depending on LED polarity having normal and inverted state allows to always have: checked – LED on, unchecked – LED off  

*LOOKER_HTML_VIEW*  
variable is read only  

LOOKER_HTML_EDIT*  
variable can be edited

**style**  
CSS enhances viewing experience.

Example:
```
#define STYLE "color:red;"	//red text
```
Style can be static or dynamic.
Unlike static dynamic style can change on fly but requires more RAM.
Dynamic style is useful for example if its value reaches some critical level and user should be alerted.

To leave static style this line in *looker.h* needs to be commented out:
```
#define LOOKER_STYLE_DYNAMIC
```
Max size of style (string) is limited in *looker.h*:
```
#define LOOKER_VAR_STYLE_SIZE 48
```

For more info refer to:  
*src/examples/arduino/style*
---
```
LOOKER_EXIT_CODE looker_update(void)  
```
Synchronizes all registered variables with the web server.
If a variable changes locally looker_update changes it also on the website and the other way around.
If same variable changes at the same time on both sides the website takes precedence (human wins over computer).

*looker_update* should be placed in main loop.
The more frequent it is called the more often the variables get updated.


## Fine-tuning
*looker.h* has some defines that help optimize Looker:
```
#define LOOKER_USE_MALLOC
```
Each variable needs a block of data that defines this variable.
This data can be allocated statically or dynamically.
If LOOKER_USE_MALLOC is commented out static data is preferred.

LOOKER_VAR_COUNT (*looker.h*) limits number of block of data to be allocated but with static they are all used. Therefore this value should be equal to the number of variables intended to use otherwise extra RAM will be wasted. With dynamic data block is allocated per variable (still up to LOOKER_VAR_COUNT) so RAM is better utilized. The trade off is that malloc function itself takes some space specially if it is not used elsewhere and it gets linked specially for Looker. Also data for static variables are guaranteed at the compilation time whereas dynamic data only during run time. Depending on resources, other prior malloc usage it might be possible that there will not be enough memory for Looker to allocate. 
```
#define LOOKER_SANITY_TEST
```
This turns on a simple test that checks if a function parameter or a variable exceeds its range. This feature can be disabled to get smaller code footprint. 
