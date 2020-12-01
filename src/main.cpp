#include <Arduino.h>

#include "sensesp_app.h"
#include "sensesp_app_builder.h"
#include "transforms/linear.h"
#include "signalk/signalk_output.h"
#include "sensors/onewire_temperature.h"
#include "sensors/ultrasonic_distance.h"
#include "transforms/moving_average.h"

#define TRIGGER_PIN 15
#define INPUT_PIN 14

// SensESP builds upon the ReactESP framework. Every ReactESP application
// defines an "app" object vs defining a "main()" method.
ReactESP app([]() {

// Some initialization boilerplate when in debug mode...
#ifndef SERIAL_DEBUG_DISABLED
  Serial.begin(115200);

  // A small arbitrary delay is required to let the
  // serial port catch up
  delay(100);
  Debug.setSerialEnabled(true);
#endif

  debugI("\nSerial debug enabled\n");

  // Create a builder object
  SensESPAppBuilder builder;

  // Create the global SensESPApp() object.
  sensesp_app = builder.set_hostname("bilge-temp-monitor")
                    ->set_sk_server("192.168.0.1", 3000)
                    ->set_standard_sensors(IP_ADDRESS)
                    ->get_app();

  // The "SignalK path" identifies this sensor to the SignalK server. Leaving
  // this blank would indicate this particular sensor (or transform) does not
  // broadcast SignalK data.
  const char *sk_path = "bilge.currentLevel";

  // The "Configuration path" is combined with "/config" to formulate a URL
  // used by the RESTful API for retrieving or setting configuration data.
  // It is ALSO used to specify a path to the SPIFFS file system
  // where configuration data is saved on the MCU board. It should
  // ALWAYS start with a forward slash if specified. If left blank,
  // that indicates this sensor or transform does not have any
  // configuration to save, or that you're not interested in doing
  // run-time configuration.

  const char *ultrasonic_in_config_path = "/bilge/ultrasonic_in";
  const char *linear_config_path = "/bilge/linear";
  const char *ultrasonic_ave_samples = "/bilge/samples";

  // Create a sensor that is the source of our data, that will be read every readDelay ms.
  // It is an ultrasonic distance sensor that sends out an acoustical pulse in response
  // to a 100 micro-sec trigger pulse from the ESP. The return acoustical pulse width
  // can be converted to a distance by the formula 2*distance = pulse_width/speed_of_sound
  // With pulse_width in micro-sec and distance in cm, 2*speed_of_sound = 58

  uint read_delay = 1000;

#ifdef ESP8266
  uint8_t pin = 12;
#elif defined(ESP32)
  uint8_t = pin = x; // x is a placeholder for the real pin
#endif


  DallasTemperatureSensors *dts = new DallasTemperatureSensors(pin);

  auto *bilge_temp = new OneWireTemperature(dts, read_delay, "/bilgeWaterTemperature/oneWire");
  auto *outside_temp = new OneWireTemperature(dts, read_delay, "/outsideTemperature/oneWire");
  auto *cabin_temp = new OneWireTemperature(dts, read_delay, "/cabinTemperature/oneWire");

  bilge_temp->connect_to(new Linear(1.0, 0.0, "/bilgeWaterTemperature/linear"))
      ->connect_to(new SKOutputNumber("environment.bilge.waterTemperature"));

  outside_temp->connect_to(new Linear(1.0, 0.0, "/outsideTemperature/linear"))
      ->connect_to(new SKOutputNumber("environment.outside.temperature"));

  cabin_temp->connect_to(new Linear(1.0, 0.0, "/cabinTemperature/linear"))
      ->connect_to(new SKOutputNumber("environment.inside.temperature"));

  auto *pUltrasonicSens = new UltrasonicSens(TRIGGER_PIN, INPUT_PIN, read_delay, ultrasonic_in_config_path);

  // A Linear transform takes its input, multiplies it by the multiplier, then adds the offset,
  // to calculate its output. In this example, we will use the distance from the sensor in cm which is given by the
  // ultrasonicSensor output/58.
  const float multiplier = .01724;
  const float offset = 0.;
  float scale = 1.0;

  // Wire up the output of the analog input to the Linear transform,
  // and then output the results to the SignalK server.
  pUltrasonicSens->connect_to(new Linear(multiplier, offset, linear_config_path))
      ->connect_to(new MovingAverage(10, scale, ultrasonic_ave_samples))
      ->connect_to(new SKOutputNumber(sk_path));

  // Start the SensESP application running
  sensesp_app->enable();
});