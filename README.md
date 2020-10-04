This project is no longer maintained.

**Alexa Voice Service (AVS) client for ESP32**. This is a work in progress.

ESP32 from Espressif is a tiny SoC integrated Bluetooth 4.2 and WiFi. Bluetooth can be used for communicating with mobile phone, and WiFi can be used for connecting to Internet. Why not make this device do something like ‘Alexa, let my ESP32 do some interesting things for me’?

The APIs of AVS are based on http/2. So First, it should make ESP32 to run as a http2 client to send data to AVS or get data from AVS. There are two I2S ports in ESP32, I2S port can be used as user voice input or sound/music output. The project is just launched, and much work is needed to do.

**Compile and developer resources**   
The core of the Huan is the ESP32, and the official development environment for the ESP32 is the ESP-IDF which includes the necessary tools to develop the ESP32 software and to push the firmware to the device, so just compile the Huan as a standard ESP-IDF project. 

[ESP-IDF Programming Guide](http://esp-idf.readthedocs.io/en/latest/index.html
) (includes setup guide)

[ESP32 developer documents](http://espressif.com/en/support/download/documents
)

**Http/2**  
The http/2 tech used in this project is based on [mbedtls](https://tls.mbed.org/) and [nghttp2](https://nghttp2.org/). 

**About AVS**  
The Alexa Voice Service  (AVS) is the cloud-based service，and it allows you to integrate Alexa’s built-in voice capabilities into connected products. With AVS, users will have the ability to play music, request the weather forecast and local news, get updates on traffic conditions, ask general knowledge questions, set timers and alarms, query Wikipedia and much more, the same way they would with an Amazon Echo. Developers also have access to third-party skills developed using the Alexa Skills Kit (ASK). Using AVS and ASK together, developers can build sophisticated interactions to extend the capabilities of your Alexa-enabled product.

**Hardware**  
ESP32  
https://espressif.com/en/products/hardware/esp32/overview

WM8978  
The audio CODEC used in this project is WM8978 which integrates preamps for stereo differential mics and includes drivers for speakers, headphone and differential or stereo line output. 
http://www.cirrus.com/en/pubs/proDatasheet/WM8978_v4.5.pdf

Schematic  
The ESP32 module and WM8978 moudle now we use in the project are just from the market, you can also them. 
Below is the shematic about how to twist those two parts into one:
![The schematic](https://github.com/GOLDELEC/GOLDELEC-Resources/blob/master/Huan_hardware_0.7.0.png)
