#include <Arduino.h>
#include <stdarg.h>
#include <stdio.h>

extern "C" void colorcanArduinoRun(void);

static HardwareSerial colorcanSerial(USART2);

void setup()
{
    // ColorCAN test pads: USART2 TX on PA2 and RX on PA3.
    colorcanSerial.begin(115200);
    colorcanSerial.println("ColorCAN Arduino PIOC bench");
}

void loop()
{
    colorcanArduinoRun();
}

extern "C" void colorcanArduinoPinMode(uint8_t pin, bool released)
{
    pinMode(pin, released ? INPUT : OUTPUT);
}

extern "C" void colorcanArduinoWrite(uint8_t pin, uint8_t level)
{
    digitalWrite(pin, level);
}

extern "C" uint8_t colorcanArduinoRead(uint8_t pin)
{
    return digitalRead(pin);
}

extern "C" void colorcanArduinoDelayUs(uint32_t us)
{
    delayMicroseconds(us);
}

extern "C" void colorcanArduinoDelayMs(uint32_t ms)
{
    delay(ms);
}

extern "C" uint32_t colorcanArduinoTicks()
{
    // The shared firmware uses 48 MHz ticks. PIOC generates the actual bit timing.
    return static_cast<uint32_t>(micros()) * 48u;
}

extern "C" int colorcanArduinoPrintf(const char *format, ...)
{
    char message[96];
    va_list args;
    va_start(args, format);
    int length = vsnprintf(message, sizeof(message), format, args);
    va_end(args);
    if (length > 0) colorcanSerial.print(message);
    return length;
}
