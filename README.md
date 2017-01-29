**Alexa Voice Service (AVS) client for ESP32**. This is a work in progress.

ESP32 from Espressif is a tiny SoC integrated Bluetooth 4.2 and WiFi. Bluetooth can be used for communicating with mobile phone, and WiFi can be used for connecting to Internet. Why not make this device do something like ‘Alexa, let my ESP32 do some interesting things for me’?

The APIs of AVS are based on http/2. So First, it should make ESP32 to run as a http2 client to send data to AVS or get data from AVS. There are two I2S ports in ESP32, I2S port can be used as user voice input or sound/music output. The project is just launched, and much work is needed to do.

How to compile the code? Just compile it as a normal ESP32 project like this:
http://goldelec.com/install-esp32-development-tools-and-build-helloworld-project/

The http/2 tech used in this project is based on mbedtls and nghttp2. 

**mtbedtls**  
https://tls.mbed.org/

**nghttp2**  
https://nghttp2.org/

**About AVS**  
The Alexa Voice Service  (AVS) is the cloud-based service，and it allows you to integrate Alexa’s built-in voice capabilities into connected products. With AVS, users will have the ability to play music, request the weather forecast and local news, get updates on traffic conditions, ask general knowledge questions, set timers and alarms, query Wikipedia and much more, the same way they would with an Amazon Echo. Developers also have access to third-party skills developed using the Alexa Skills Kit (ASK). Using AVS and ASK together, developers can build sophisticated interactions to extend the capabilities of your Alexa-enabled product.

**Hardware**  
ESP32  
https://espressif.com/en/products/hardware/esp32/overview

WM8978  
The audio CODEC used in this project is WM8978 which integrates preamps for stereo differential mics and includes drivers for speakers, headphone and differential or stereo line output. 
http://www.cirrus.com/en/pubs/proDatasheet/WM8978_v4.5.pdf
