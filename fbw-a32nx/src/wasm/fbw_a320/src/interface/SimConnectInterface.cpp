#include "SimConnectInterface.h"
#include <cmath>
#include <iostream>
#include <map>
#include <vector>

// remove when aileron events can be processed via SimConnect
bool SimConnectInterface::loggingFlightControlsEnabled = false;
// remove when aileron events can be processed via SimConnect
SimInput SimConnectInterface::simInput = {};
// remove when aileron events can be processed via SimConnect
double SimConnectInterface::flightControlsKeyChangeAileron = 0.0;

bool SimConnectInterface::connect(bool clientDataEnabled,
                                  bool autopilotStateMachineEnabled,
                                  bool autopilotLawsEnabled,
                                  bool flyByWireEnabled,
                                  int elacDisabled,
                                  int secDisabled,
                                  int facDisabled,
                                  const std::vector<std::shared_ptr<ThrottleAxisMapping>>& throttleAxis,
                                  std::shared_ptr<SpoilersHandler> spoilersHandler,
                                  double keyChangeAileron,
                                  double keyChangeElevator,
                                  double keyChangeRudder,
                                  bool disableXboxCompatibilityRudderPlusMinus,
                                  bool enableRudder2AxisMode,
                                  double minSimulationRate,
                                  double maxSimulationRate,
                                  bool limitSimulationRateByPerformance) {
  // info message
  std::cout << "WASM: Connecting..." << std::endl;

  // connect
  HRESULT result = SimConnect_Open(&hSimConnect, "FlyByWire", nullptr, 0, 0, 0);

  if (S_OK == result) {
    // we are now connected
    isConnected = true;
    std::cout << "WASM: Connected" << std::endl;
    // store throttle axis handler
    this->throttleAxis = throttleAxis;
    // store spoilers handler
    this->spoilersHandler = spoilersHandler;
    // store maximum allowed simulation rate
    this->minSimulationRate = minSimulationRate;
    this->maxSimulationRate = maxSimulationRate;
    this->limitSimulationRateByPerformance = limitSimulationRateByPerformance;
    // store is client data is enabled
    this->clientDataEnabled = clientDataEnabled;
    this->elacDisabled = elacDisabled;
    this->secDisabled = secDisabled;
    this->facDisabled = facDisabled;
    // store key change value for each axis
    flightControlsKeyChangeAileron = keyChangeAileron;
    flightControlsKeyChangeElevator = keyChangeElevator;
    flightControlsKeyChangeRudder = keyChangeRudder;
    // store if XBOX compatibility should be disabled for rudder axis plus/minus
    this->disableXboxCompatibilityRudderPlusMinus = disableXboxCompatibilityRudderPlusMinus;
    this->enableRudder2AxisMode = enableRudder2AxisMode;
    // register local variables
    idFcuEventSetSPEED = std::make_unique<LocalVariable>("A320_Neo_FCU_SPEED_SET_DATA");
    idFcuEventSetHDG = std::make_unique<LocalVariable>("A320_Neo_FCU_HDG_SET_DATA");
    idFcuEventSetVS = std::make_unique<LocalVariable>("A320_Neo_FCU_VS_SET_DATA");
    // add data to definition
    bool prepareResult = prepareSimDataSimConnectDataDefinitions();
    prepareResult &= prepareSimInputSimConnectDataDefinitions();
    prepareResult &= prepareSimOutputSimConnectDataDefinitions();
    if (clientDataEnabled) {
      prepareResult &= prepareClientDataDefinitions();
    }
    // check result
    if (!prepareResult) {
      // failed to add data definition -> disconnect
      std::cout << "WASM: Failed to prepare data definitions" << std::endl;
      disconnect();
      // failed to connect
      return false;
    }
    // register key event handler
    // remove when aileron events can be processed via SimConnect
    register_key_event_handler_EX1(static_cast<GAUGE_KEY_EVENT_HANDLER_EX1>(processKeyEvent), NULL);
    // send initial event to FCU to force HDG mode
    execute_calculator_code("(>H:A320_Neo_FCU_HDG_PULL)", nullptr, nullptr, nullptr);
    // success
    return true;
  }
  // fallback -> failed
  return false;
}

void SimConnectInterface::disconnect() {
  if (isConnected) {
    // unregister key event handler
    // remove when aileron events can be processed via SimConnect
    unregister_key_event_handler_EX1(static_cast<GAUGE_KEY_EVENT_HANDLER_EX1>(processKeyEvent), NULL);
    // info message
    std::cout << "WASM: Disconnecting..." << std::endl;
    // close connection
    SimConnect_Close(hSimConnect);
    // set flag
    isConnected = false;
    // reset handle
    hSimConnect = 0;
    // info message
    std::cout << "WASM: Disconnected" << std::endl;
  }
}

void SimConnectInterface::setSampleTime(double sampleTime) {
  this->sampleTime = sampleTime;
}

void SimConnectInterface::updateSimulationRateLimits(double minSimulationRate, double maxSimulationRate) {
  this->minSimulationRate = minSimulationRate;
  this->maxSimulationRate = maxSimulationRate;
}

bool SimConnectInterface::prepareSimDataSimConnectDataDefinitions() {
  bool result = true;

  result &= addDataDefinition(hSimConnect, 0, SIMCONNECT_DATATYPE_FLOAT64, "G FORCE", "GFORCE");
  result &= addDataDefinition(hSimConnect, 0, SIMCONNECT_DATATYPE_FLOAT64, "PLANE PITCH DEGREES", "DEGREE");
  result &= addDataDefinition(hSimConnect, 0, SIMCONNECT_DATATYPE_FLOAT64, "PLANE BANK DEGREES", "DEGREE");
  result &= addDataDefinition(hSimConnect, 0, SIMCONNECT_DATATYPE_XYZ, "STRUCT BODY ROTATION VELOCITY", "STRUCT");
  result &= addDataDefinition(hSimConnect, 0, SIMCONNECT_DATATYPE_XYZ, "STRUCT BODY ROTATION ACCELERATION", "STRUCT");
  result &= addDataDefinition(hSimConnect, 0, SIMCONNECT_DATATYPE_FLOAT64, "ACCELERATION BODY Z", "METER PER SECOND SQUARED");
  result &= addDataDefinition(hSimConnect, 0, SIMCONNECT_DATATYPE_FLOAT64, "ACCELERATION BODY X", "METER PER SECOND SQUARED");
  result &= addDataDefinition(hSimConnect, 0, SIMCONNECT_DATATYPE_FLOAT64, "ACCELERATION BODY Y", "METER PER SECOND SQUARED");
  result &= addDataDefinition(hSimConnect, 0, SIMCONNECT_DATATYPE_FLOAT64, "PLANE HEADING DEGREES MAGNETIC", "DEGREES");
  result &= addDataDefinition(hSimConnect, 0, SIMCONNECT_DATATYPE_FLOAT64, "PLANE HEADING DEGREES TRUE", "DEGREES");
  result &= addDataDefinition(hSimConnect, 0, SIMCONNECT_DATATYPE_FLOAT64, "GPS GROUND MAGNETIC TRACK", "DEGREES");
  result &= addDataDefinition(hSimConnect, 0, SIMCONNECT_DATATYPE_FLOAT64, "ELEVATOR POSITION", "POSITION");
  result &= addDataDefinition(hSimConnect, 0, SIMCONNECT_DATATYPE_FLOAT64, "ELEVATOR TRIM POSITION", "DEGREE");
  result &= addDataDefinition(hSimConnect, 0, SIMCONNECT_DATATYPE_FLOAT64, "AILERON POSITION", "POSITION");
  result &= addDataDefinition(hSimConnect, 0, SIMCONNECT_DATATYPE_FLOAT64, "RUDDER POSITION", "POSITION");
  result &= addDataDefinition(hSimConnect, 0, SIMCONNECT_DATATYPE_FLOAT64, "RUDDER TRIM PCT", "PERCENT OVER 100");
  result &= addDataDefinition(hSimConnect, 0, SIMCONNECT_DATATYPE_FLOAT64, "INCIDENCE ALPHA", "DEGREE");
  result &= addDataDefinition(hSimConnect, 0, SIMCONNECT_DATATYPE_FLOAT64, "INCIDENCE BETA", "DEGREE");
  result &= addDataDefinition(hSimConnect, 0, SIMCONNECT_DATATYPE_FLOAT64, "BETA DOT", "DEGREE PER SECOND");
  result &= addDataDefinition(hSimConnect, 0, SIMCONNECT_DATATYPE_FLOAT64, "AIRSPEED INDICATED", "KNOTS");
  result &= addDataDefinition(hSimConnect, 0, SIMCONNECT_DATATYPE_FLOAT64, "AIRSPEED TRUE", "KNOTS");
  result &= addDataDefinition(hSimConnect, 0, SIMCONNECT_DATATYPE_FLOAT64, "AIRSPEED MACH", "MACH");
  result &= addDataDefinition(hSimConnect, 0, SIMCONNECT_DATATYPE_FLOAT64, "GROUND VELOCITY", "KNOTS");
  // workaround for altitude issues due to MSFS bug, needs to be changed to PRESSURE ALTITUDE again when solved
  result &= addDataDefinition(hSimConnect, 0, SIMCONNECT_DATATYPE_FLOAT64, "INDICATED ALTITUDE:3", "FEET");
  result &= addDataDefinition(hSimConnect, 0, SIMCONNECT_DATATYPE_FLOAT64, "INDICATED ALTITUDE", "FEET");
  result &= addDataDefinition(hSimConnect, 0, SIMCONNECT_DATATYPE_FLOAT64, "PLANE ALT ABOVE GROUND MINUS CG", "FEET");
  result &= addDataDefinition(hSimConnect, 0, SIMCONNECT_DATATYPE_FLOAT64, "VELOCITY WORLD Y", "FEET PER MINUTE");
  result &= addDataDefinition(hSimConnect, 0, SIMCONNECT_DATATYPE_FLOAT64, "CG PERCENT", "PERCENT OVER 100");
  result &= addDataDefinition(hSimConnect, 0, SIMCONNECT_DATATYPE_FLOAT64, "TOTAL WEIGHT", "KILOGRAMS");
  result &= addDataDefinition(hSimConnect, 0, SIMCONNECT_DATATYPE_FLOAT64, "GEAR ANIMATION POSITION:0", "NUMBER");
  result &= addDataDefinition(hSimConnect, 0, SIMCONNECT_DATATYPE_FLOAT64, "GEAR ANIMATION POSITION:1", "NUMBER");
  result &= addDataDefinition(hSimConnect, 0, SIMCONNECT_DATATYPE_FLOAT64, "GEAR ANIMATION POSITION:2", "NUMBER");
  result &= addDataDefinition(hSimConnect, 0, SIMCONNECT_DATATYPE_FLOAT64, "SPOILERS HANDLE POSITION", "POSITION");
  result &= addDataDefinition(hSimConnect, 0, SIMCONNECT_DATATYPE_FLOAT64, "SPOILERS LEFT POSITION", "PERCENT OVER 100");
  result &= addDataDefinition(hSimConnect, 0, SIMCONNECT_DATATYPE_FLOAT64, "SPOILERS RIGHT POSITION", "PERCENT OVER 100");
  result &= addDataDefinition(hSimConnect, 0, SIMCONNECT_DATATYPE_INT64, "IS SLEW ACTIVE", "BOOL");
  result &= addDataDefinition(hSimConnect, 0, SIMCONNECT_DATATYPE_INT64, "AUTOPILOT MASTER", "BOOL");
  result &= addDataDefinition(hSimConnect, 0, SIMCONNECT_DATATYPE_INT64, "AUTOPILOT FLIGHT DIRECTOR ACTIVE:1", "BOOL");
  result &= addDataDefinition(hSimConnect, 0, SIMCONNECT_DATATYPE_INT64, "AUTOPILOT FLIGHT DIRECTOR ACTIVE:2", "BOOL");
  result &= addDataDefinition(hSimConnect, 0, SIMCONNECT_DATATYPE_FLOAT64, "AUTOPILOT AIRSPEED HOLD VAR", "KNOTS");
  result &= addDataDefinition(hSimConnect, 0, SIMCONNECT_DATATYPE_FLOAT64, "AUTOPILOT ALTITUDE LOCK VAR:3", "FEET");
  result &= addDataDefinition(hSimConnect, 0, SIMCONNECT_DATATYPE_FLOAT64, "SIMULATION TIME", "NUMBER");
  result &= addDataDefinition(hSimConnect, 0, SIMCONNECT_DATATYPE_FLOAT64, "SIMULATION RATE", "NUMBER");
  result &= addDataDefinition(hSimConnect, 0, SIMCONNECT_DATATYPE_FLOAT64, "STRUCTURAL ICE PCT", "PERCENT OVER 100");
  result &= addDataDefinition(hSimConnect, 0, SIMCONNECT_DATATYPE_FLOAT64, "LINEAR CL ALPHA", "PER DEGREE");
  result &= addDataDefinition(hSimConnect, 0, SIMCONNECT_DATATYPE_FLOAT64, "STALL ALPHA", "DEGREE");
  result &= addDataDefinition(hSimConnect, 0, SIMCONNECT_DATATYPE_FLOAT64, "ZERO LIFT ALPHA", "DEGREE");
  result &= addDataDefinition(hSimConnect, 0, SIMCONNECT_DATATYPE_FLOAT64, "AMBIENT DENSITY", "KILOGRAM PER CUBIC METER");
  result &= addDataDefinition(hSimConnect, 0, SIMCONNECT_DATATYPE_FLOAT64, "AMBIENT PRESSURE", "MILLIBARS");
  result &= addDataDefinition(hSimConnect, 0, SIMCONNECT_DATATYPE_FLOAT64, "AMBIENT TEMPERATURE", "CELSIUS");
  result &= addDataDefinition(hSimConnect, 0, SIMCONNECT_DATATYPE_FLOAT64, "AMBIENT WIND X", "KNOTS");
  result &= addDataDefinition(hSimConnect, 0, SIMCONNECT_DATATYPE_FLOAT64, "AMBIENT WIND Y", "KNOTS");
  result &= addDataDefinition(hSimConnect, 0, SIMCONNECT_DATATYPE_FLOAT64, "AMBIENT WIND Z", "KNOTS");
  result &= addDataDefinition(hSimConnect, 0, SIMCONNECT_DATATYPE_FLOAT64, "AMBIENT WIND VELOCITY", "KNOTS");
  result &= addDataDefinition(hSimConnect, 0, SIMCONNECT_DATATYPE_FLOAT64, "AMBIENT WIND DIRECTION", "DEGREES");
  result &= addDataDefinition(hSimConnect, 0, SIMCONNECT_DATATYPE_FLOAT64, "TOTAL AIR TEMPERATURE", "CELSIUS");
  result &= addDataDefinition(hSimConnect, 0, SIMCONNECT_DATATYPE_FLOAT64, "PLANE LATITUDE", "DEGREES");
  result &= addDataDefinition(hSimConnect, 0, SIMCONNECT_DATATYPE_FLOAT64, "PLANE LONGITUDE", "DEGREES");
  result &= addDataDefinition(hSimConnect, 0, SIMCONNECT_DATATYPE_FLOAT64, "GENERAL ENG THROTTLE LEVER POSITION:1", "PERCENT");
  result &= addDataDefinition(hSimConnect, 0, SIMCONNECT_DATATYPE_FLOAT64, "GENERAL ENG THROTTLE LEVER POSITION:2", "PERCENT");
  result &= addDataDefinition(hSimConnect, 0, SIMCONNECT_DATATYPE_FLOAT64, "TURB ENG JET THRUST:1", "POUNDS");
  result &= addDataDefinition(hSimConnect, 0, SIMCONNECT_DATATYPE_FLOAT64, "TURB ENG JET THRUST:2", "POUNDS");
  result &= addDataDefinition(hSimConnect, 0, SIMCONNECT_DATATYPE_INT64, "NAV HAS NAV:3", "BOOL");
  result &= addDataDefinition(hSimConnect, 0, SIMCONNECT_DATATYPE_FLOAT64, "NAV LOCALIZER:3", "DEGREES");
  result &= addDataDefinition(hSimConnect, 0, SIMCONNECT_DATATYPE_FLOAT64, "NAV RAW GLIDE SLOPE:3", "DEGREES");
  result &= addDataDefinition(hSimConnect, 0, SIMCONNECT_DATATYPE_INT64, "NAV HAS DME:3", "BOOL");
  result &= addDataDefinition(hSimConnect, 0, SIMCONNECT_DATATYPE_FLOAT64, "NAV DME:3", "NAUTICAL MILES");
  result &= addDataDefinition(hSimConnect, 0, SIMCONNECT_DATATYPE_INT64, "NAV HAS LOCALIZER:3", "BOOL");
  result &= addDataDefinition(hSimConnect, 0, SIMCONNECT_DATATYPE_FLOAT64, "NAV RADIAL ERROR:3", "DEGREES");
  result &= addDataDefinition(hSimConnect, 0, SIMCONNECT_DATATYPE_INT64, "NAV HAS GLIDE SLOPE:3", "BOOL");
  result &= addDataDefinition(hSimConnect, 0, SIMCONNECT_DATATYPE_FLOAT64, "NAV GLIDE SLOPE ERROR:3", "DEGREES");
  result &= addDataDefinition(hSimConnect, 0, SIMCONNECT_DATATYPE_INT64, "AUTOTHROTTLE ACTIVE", "BOOL");
  result &= addDataDefinition(hSimConnect, 0, SIMCONNECT_DATATYPE_FLOAT64, "TURB ENG CORRECTED N1:1", "PERCENT");
  result &= addDataDefinition(hSimConnect, 0, SIMCONNECT_DATATYPE_FLOAT64, "TURB ENG CORRECTED N1:2", "PERCENT");
  result &= addDataDefinition(hSimConnect, 0, SIMCONNECT_DATATYPE_INT64, "GPS IS ACTIVE FLIGHT PLAN", "BOOL");
  result &= addDataDefinition(hSimConnect, 0, SIMCONNECT_DATATYPE_FLOAT64, "GPS WP CROSS TRK", "NAUTICAL MILES");
  result &= addDataDefinition(hSimConnect, 0, SIMCONNECT_DATATYPE_FLOAT64, "GPS WP TRACK ANGLE ERROR", "DEGREES");
  result &= addDataDefinition(hSimConnect, 0, SIMCONNECT_DATATYPE_FLOAT64, "GPS COURSE TO STEER", "DEGREES");
  result &= addDataDefinition(hSimConnect, 0, SIMCONNECT_DATATYPE_FLOAT64, "TURB ENG COMMANDED N1:1", "PERCENT");
  result &= addDataDefinition(hSimConnect, 0, SIMCONNECT_DATATYPE_FLOAT64, "TURB ENG COMMANDED N1:2", "PERCENT");
  result &= addDataDefinition(hSimConnect, 0, SIMCONNECT_DATATYPE_FLOAT64, "TURB ENG N1:1", "PERCENT");
  result &= addDataDefinition(hSimConnect, 0, SIMCONNECT_DATATYPE_FLOAT64, "TURB ENG N1:2", "PERCENT");
  result &= addDataDefinition(hSimConnect, 0, SIMCONNECT_DATATYPE_FLOAT64, "TURB ENG CORRECTED N1:1", "PERCENT");
  result &= addDataDefinition(hSimConnect, 0, SIMCONNECT_DATATYPE_FLOAT64, "TURB ENG CORRECTED N1:2", "PERCENT");
  result &= addDataDefinition(hSimConnect, 0, SIMCONNECT_DATATYPE_FLOAT64, "GENERAL ENG OIL TEMPERATURE:1", "CELSIUS");
  result &= addDataDefinition(hSimConnect, 0, SIMCONNECT_DATATYPE_FLOAT64, "GENERAL ENG OIL TEMPERATURE:2", "CELSIUS");
  result &= addDataDefinition(hSimConnect, 0, SIMCONNECT_DATATYPE_FLOAT64, "GENERAL ENG OIL PRESSURE:1", "PSI");
  result &= addDataDefinition(hSimConnect, 0, SIMCONNECT_DATATYPE_FLOAT64, "GENERAL ENG OIL PRESSURE:2", "PSI");
  result &= addDataDefinition(hSimConnect, 0, SIMCONNECT_DATATYPE_INT64, "ENG COMBUSTION:1", "BOOL");
  result &= addDataDefinition(hSimConnect, 0, SIMCONNECT_DATATYPE_INT64, "ENG COMBUSTION:2", "BOOL");
  result &= addDataDefinition(hSimConnect, 0, SIMCONNECT_DATATYPE_INT64, "AUTOPILOT MANAGED SPEED IN MACH", "BOOL");
  result &= addDataDefinition(hSimConnect, 0, SIMCONNECT_DATATYPE_INT64, "AUTOPILOT SPEED SLOT INDEX", "NUMBER");
  result &= addDataDefinition(hSimConnect, 0, SIMCONNECT_DATATYPE_INT64, "ENG ANTI ICE:1", "BOOL");
  result &= addDataDefinition(hSimConnect, 0, SIMCONNECT_DATATYPE_INT64, "ENG ANTI ICE:2", "BOOL");
  result &= addDataDefinition(hSimConnect, 0, SIMCONNECT_DATATYPE_INT64, "SIM ON GROUND", "BOOL");
  result &= addDataDefinition(hSimConnect, 0, SIMCONNECT_DATATYPE_FLOAT64, "GENERAL ENG ELAPSED TIME:1", "SECONDS");
  result &= addDataDefinition(hSimConnect, 0, SIMCONNECT_DATATYPE_FLOAT64, "GENERAL ENG ELAPSED TIME:2", "SECONDS");
  result &= addDataDefinition(hSimConnect, 0, SIMCONNECT_DATATYPE_FLOAT64, "STANDARD ATM TEMPERATURE", "CELSIUS");
  result &= addDataDefinition(hSimConnect, 0, SIMCONNECT_DATATYPE_FLOAT64, "TURB ENG CORRECTED FF:1", "POUNDS PER HOUR");
  result &= addDataDefinition(hSimConnect, 0, SIMCONNECT_DATATYPE_FLOAT64, "TURB ENG CORRECTED FF:2", "POUNDS PER HOUR");
  result &= addDataDefinition(hSimConnect, 0, SIMCONNECT_DATATYPE_FLOAT64, "FUEL TANK LEFT AUX CAPACITY", "GALLONS");
  result &= addDataDefinition(hSimConnect, 0, SIMCONNECT_DATATYPE_FLOAT64, "FUEL TANK RIGHT AUX CAPACITY", "GALLONS");
  result &= addDataDefinition(hSimConnect, 0, SIMCONNECT_DATATYPE_FLOAT64, "FUEL TANK LEFT MAIN CAPACITY", "GALLONS");
  result &= addDataDefinition(hSimConnect, 0, SIMCONNECT_DATATYPE_FLOAT64, "FUEL TANK RIGHT MAIN CAPACITY", "GALLONS");
  result &= addDataDefinition(hSimConnect, 0, SIMCONNECT_DATATYPE_FLOAT64, "FUEL TANK CENTER CAPACITY", "GALLONS");
  result &= addDataDefinition(hSimConnect, 0, SIMCONNECT_DATATYPE_FLOAT64, "FUEL TANK LEFT AUX QUANTITY", "GALLONS");
  result &= addDataDefinition(hSimConnect, 0, SIMCONNECT_DATATYPE_FLOAT64, "FUEL TANK RIGHT AUX QUANTITY", "GALLONS");
  result &= addDataDefinition(hSimConnect, 0, SIMCONNECT_DATATYPE_FLOAT64, "FUEL TANK LEFT MAIN QUANTITY", "GALLONS");
  result &= addDataDefinition(hSimConnect, 0, SIMCONNECT_DATATYPE_FLOAT64, "FUEL TANK RIGHT MAIN QUANTITY", "GALLONS");
  result &= addDataDefinition(hSimConnect, 0, SIMCONNECT_DATATYPE_FLOAT64, "FUEL TANK CENTER QUANTITY", "GALLONS");
  result &= addDataDefinition(hSimConnect, 0, SIMCONNECT_DATATYPE_FLOAT64, "FUEL TOTAL QUANTITY", "GALLONS");
  result &= addDataDefinition(hSimConnect, 0, SIMCONNECT_DATATYPE_FLOAT64, "FUEL WEIGHT PER GALLON", "POUNDS");
  result &= addDataDefinition(hSimConnect, 0, SIMCONNECT_DATATYPE_FLOAT64, "KOHLSMAN SETTING MB:0", "MBAR");
  result &= addDataDefinition(hSimConnect, 0, SIMCONNECT_DATATYPE_FLOAT64, "KOHLSMAN SETTING MB:1", "MBAR");
  result &= addDataDefinition(hSimConnect, 0, SIMCONNECT_DATATYPE_INT64, "KOHLSMAN SETTING STD:3", "BOOL");
  result &= addDataDefinition(hSimConnect, 0, SIMCONNECT_DATATYPE_INT64, "CAMERA STATE", "NUMBER");
  result &= addDataDefinition(hSimConnect, 0, SIMCONNECT_DATATYPE_FLOAT64, "PLANE ALTITUDE", "METERS");
  result &= addDataDefinition(hSimConnect, 0, SIMCONNECT_DATATYPE_FLOAT64, "NAV MAGVAR:3", "DEGREES");
  result &= addDataDefinition(hSimConnect, 0, SIMCONNECT_DATATYPE_LATLONALT, "NAV VOR LATLONALT:3", "STRUCT");
  result &= addDataDefinition(hSimConnect, 0, SIMCONNECT_DATATYPE_LATLONALT, "NAV GS LATLONALT:3", "STRUCT");
  result &= addDataDefinition(hSimConnect, 0, SIMCONNECT_DATATYPE_FLOAT64, "BRAKE LEFT POSITION", "POSITION");
  result &= addDataDefinition(hSimConnect, 0, SIMCONNECT_DATATYPE_FLOAT64, "BRAKE RIGHT POSITION", "POSITION");
  result &= addDataDefinition(hSimConnect, 0, SIMCONNECT_DATATYPE_FLOAT64, "FLAPS HANDLE INDEX", "NUMBER");
  result &= addDataDefinition(hSimConnect, 0, SIMCONNECT_DATATYPE_FLOAT64, "GEAR HANDLE POSITION", "POSITION");
  result &= addDataDefinition(hSimConnect, 0, SIMCONNECT_DATATYPE_INT64, "ASSISTANCE TAKEOFF ENABLED", "BOOL");
  result &= addDataDefinition(hSimConnect, 0, SIMCONNECT_DATATYPE_INT64, "ASSISTANCE LANDING ENABLED", "BOOL");
  result &= addDataDefinition(hSimConnect, 0, SIMCONNECT_DATATYPE_INT64, "AI AUTOTRIM ACTIVE", "BOOL");
  result &= addDataDefinition(hSimConnect, 0, SIMCONNECT_DATATYPE_INT64, "AI CONTROLS", "BOOL");
  result &= addDataDefinition(hSimConnect, 0, SIMCONNECT_DATATYPE_FLOAT64, "WHEEL RPM:1", "RPM");
  result &= addDataDefinition(hSimConnect, 0, SIMCONNECT_DATATYPE_FLOAT64, "WHEEL RPM:2", "RPM");

  // FIXME use MMR2 (NAV4) as well

  return result;
}

bool SimConnectInterface::prepareSimInputSimConnectDataDefinitions() {
  bool result = true;

  result &= addInputDataDefinition(hSimConnect, 0, Events::AXIS_ELEVATOR_SET, "AXIS_ELEVATOR_SET", true);
  result &= addInputDataDefinition(hSimConnect, 0, Events::AXIS_AILERONS_SET, "AXIS_AILERONS_SET", true);
  result &= addInputDataDefinition(hSimConnect, 0, Events::AXIS_RUDDER_SET, "AXIS_RUDDER_SET", true);

  result &= addInputDataDefinition(hSimConnect, 0, Events::RUDDER_SET, "RUDDER_SET", true);
  result &= addInputDataDefinition(hSimConnect, 0, Events::RUDDER_LEFT, "RUDDER_LEFT", true);
  result &= addInputDataDefinition(hSimConnect, 0, Events::RUDDER_AXIS_PLUS, "RUDDER_AXIS_PLUS", true);
  result &= addInputDataDefinition(hSimConnect, 0, Events::RUDDER_CENTER, "RUDDER_CENTER", true);
  result &= addInputDataDefinition(hSimConnect, 0, Events::RUDDER_RIGHT, "RUDDER_RIGHT", true);
  result &= addInputDataDefinition(hSimConnect, 0, Events::RUDDER_AXIS_MINUS, "RUDDER_AXIS_MINUS", true);

  result &= addInputDataDefinition(hSimConnect, 0, Events::RUDDER_TRIM_LEFT, "RUDDER_TRIM_LEFT", true);
  result &= addInputDataDefinition(hSimConnect, 0, Events::RUDDER_TRIM_RESET, "RUDDER_TRIM_RESET", true);
  result &= addInputDataDefinition(hSimConnect, 0, Events::RUDDER_TRIM_RIGHT, "RUDDER_TRIM_RIGHT", true);
  result &= addInputDataDefinition(hSimConnect, 0, Events::RUDDER_TRIM_SET, "RUDDER_TRIM_SET", true);
  result &= addInputDataDefinition(hSimConnect, 0, Events::RUDDER_TRIM_SET_EX1, "RUDDER_TRIM_SET_EX1", true);

  result &= addInputDataDefinition(hSimConnect, 0, Events::AILERON_SET, "AILERON_SET", true);
  result &= addInputDataDefinition(hSimConnect, 0, Events::AILERONS_LEFT, "AILERONS_LEFT", true);
  result &= addInputDataDefinition(hSimConnect, 0, Events::AILERONS_RIGHT, "AILERONS_RIGHT", true);

  result &= addInputDataDefinition(hSimConnect, 0, Events::CENTER_AILER_RUDDER, "CENTER_AILER_RUDDER", true);

  result &= addInputDataDefinition(hSimConnect, 0, Events::ELEVATOR_SET, "ELEVATOR_SET", true);
  result &= addInputDataDefinition(hSimConnect, 0, Events::ELEV_DOWN, "ELEV_DOWN", true);
  result &= addInputDataDefinition(hSimConnect, 0, Events::ELEV_UP, "ELEV_UP", true);

  result &= addInputDataDefinition(hSimConnect, 0, Events::AP_MASTER, "AP_MASTER", true);
  result &= addInputDataDefinition(hSimConnect, 0, Events::AUTOPILOT_OFF, "AUTOPILOT_OFF", false);
  result &= addInputDataDefinition(hSimConnect, 0, Events::AUTOPILOT_ON, "AUTOPILOT_ON", true);
  result &= addInputDataDefinition(hSimConnect, 0, Events::AUTOPILOT_DISENGAGE_SET, "AUTOPILOT_DISENGAGE_SET", true);
  result &= addInputDataDefinition(hSimConnect, 0, Events::AUTOPILOT_DISENGAGE_TOGGLE, "AUTOPILOT_DISENGAGE_TOGGLE", true);
  result &= addInputDataDefinition(hSimConnect, 0, Events::TOGGLE_FLIGHT_DIRECTOR, "TOGGLE_FLIGHT_DIRECTOR", false);
  result &= addInputDataDefinition(hSimConnect, 0, Events::A32NX_FCU_AP_1_PUSH, "A32NX.FCU_AP_1_PUSH", false);
  result &= addInputDataDefinition(hSimConnect, 0, Events::A32NX_FCU_AP_2_PUSH, "A32NX.FCU_AP_2_PUSH", false);
  result &= addInputDataDefinition(hSimConnect, 0, Events::A32NX_FCU_AP_DISCONNECT_PUSH, "A32NX.FCU_AP_DISCONNECT_PUSH", false);
  result &= addInputDataDefinition(hSimConnect, 0, Events::A32NX_FCU_ATHR_PUSH, "A32NX.FCU_ATHR_PUSH", false);
  result &= addInputDataDefinition(hSimConnect, 0, Events::A32NX_FCU_ATHR_DISCONNECT_PUSH, "A32NX.FCU_ATHR_DISCONNECT_PUSH", false);
  result &= addInputDataDefinition(hSimConnect, 0, Events::A32NX_FCU_SPD_INC, "A32NX.FCU_SPD_INC", false);
  result &= addInputDataDefinition(hSimConnect, 0, Events::A32NX_FCU_SPD_DEC, "A32NX.FCU_SPD_DEC", false);
  result &= addInputDataDefinition(hSimConnect, 0, Events::A32NX_FCU_SPD_SET, "A32NX.FCU_SPD_SET", false);
  result &= addInputDataDefinition(hSimConnect, 0, Events::A32NX_FCU_SPD_PUSH, "A32NX.FCU_SPD_PUSH", false);
  result &= addInputDataDefinition(hSimConnect, 0, Events::A32NX_FCU_SPD_PULL, "A32NX.FCU_SPD_PULL", false);
  result &= addInputDataDefinition(hSimConnect, 0, Events::A32NX_FCU_SPD_MACH_TOGGLE_PUSH, "A32NX.FCU_SPD_MACH_TOGGLE_PUSH", false);
  result &= addInputDataDefinition(hSimConnect, 0, Events::A32NX_FCU_HDG_INC, "A32NX.FCU_HDG_INC", false);
  result &= addInputDataDefinition(hSimConnect, 0, Events::A32NX_FCU_HDG_DEC, "A32NX.FCU_HDG_DEC", false);
  result &= addInputDataDefinition(hSimConnect, 0, Events::A32NX_FCU_HDG_SET, "A32NX.FCU_HDG_SET", false);
  result &= addInputDataDefinition(hSimConnect, 0, Events::A32NX_FCU_HDG_PUSH, "A32NX.FCU_HDG_PUSH", false);
  result &= addInputDataDefinition(hSimConnect, 0, Events::A32NX_FCU_HDG_PULL, "A32NX.FCU_HDG_PULL", false);
  result &= addInputDataDefinition(hSimConnect, 0, Events::A32NX_FCU_TRK_FPA_TOGGLE_PUSH, "A32NX.FCU_TRK_FPA_TOGGLE_PUSH", false);
  result &= addInputDataDefinition(hSimConnect, 0, Events::A32NX_FCU_TO_AP_HDG_PUSH, "A32NX.FCU_TO_AP_HDG_PUSH", false);
  result &= addInputDataDefinition(hSimConnect, 0, Events::A32NX_FCU_TO_AP_HDG_PULL, "A32NX.FCU_TO_AP_HDG_PULL", false);
  result &= addInputDataDefinition(hSimConnect, 0, Events::A32NX_FCU_ALT_INC, "A32NX.FCU_ALT_INC", false);
  result &= addInputDataDefinition(hSimConnect, 0, Events::A32NX_FCU_ALT_DEC, "A32NX.FCU_ALT_DEC", false);
  result &= addInputDataDefinition(hSimConnect, 0, Events::A32NX_FCU_ALT_SET, "A32NX.FCU_ALT_SET", false);
  result &= addInputDataDefinition(hSimConnect, 0, Events::A32NX_FCU_ALT_INCREMENT_TOGGLE, "A32NX.FCU_ALT_INCREMENT_TOGGLE", false);
  result &= addInputDataDefinition(hSimConnect, 0, Events::A32NX_FCU_ALT_INCREMENT_SET, "A32NX.FCU_ALT_INCREMENT_SET", false);
  result &= addInputDataDefinition(hSimConnect, 0, Events::A32NX_FCU_ALT_PUSH, "A32NX.FCU_ALT_PUSH", false);
  result &= addInputDataDefinition(hSimConnect, 0, Events::A32NX_FCU_ALT_PULL, "A32NX.FCU_ALT_PULL", false);
  result &= addInputDataDefinition(hSimConnect, 0, Events::A32NX_FCU_VS_INC, "A32NX.FCU_VS_INC", false);
  result &= addInputDataDefinition(hSimConnect, 0, Events::A32NX_FCU_VS_DEC, "A32NX.FCU_VS_DEC", false);
  result &= addInputDataDefinition(hSimConnect, 0, Events::A32NX_FCU_VS_SET, "A32NX.FCU_VS_SET", false);
  result &= addInputDataDefinition(hSimConnect, 0, Events::A32NX_FCU_VS_PUSH, "A32NX.FCU_VS_PUSH", false);
  result &= addInputDataDefinition(hSimConnect, 0, Events::A32NX_FCU_VS_PULL, "A32NX.FCU_VS_PULL", false);
  result &= addInputDataDefinition(hSimConnect, 0, Events::A32NX_FCU_TO_AP_VS_PUSH, "A32NX.FCU_TO_AP_VS_PUSH", false);
  result &= addInputDataDefinition(hSimConnect, 0, Events::A32NX_FCU_TO_AP_VS_PULL, "A32NX.FCU_TO_AP_VS_PULL", false);
  result &= addInputDataDefinition(hSimConnect, 0, Events::A32NX_FCU_LOC_PUSH, "A32NX.FCU_LOC_PUSH", false);
  result &= addInputDataDefinition(hSimConnect, 0, Events::A32NX_FCU_APPR_PUSH, "A32NX.FCU_APPR_PUSH", false);
  result &= addInputDataDefinition(hSimConnect, 0, Events::A32NX_FCU_EXPED_PUSH, "A32NX.FCU_EXPED_PUSH", false);
  result &= addInputDataDefinition(hSimConnect, 0, Events::A32NX_FMGC_DIR_TO_TRIGGER, "A32NX.FMGC_DIR_TO_TRIGGER", false);

  result &= addInputDataDefinition(hSimConnect, 0, Events::A32NX_EFIS_L_CHRONO_PUSHED, "A32NX.EFIS_L_CHRONO_PUSHED", false);
  result &= addInputDataDefinition(hSimConnect, 0, Events::A32NX_EFIS_R_CHRONO_PUSHED, "A32NX.EFIS_R_CHRONO_PUSHED", false);

  result &= addInputDataDefinition(hSimConnect, 0, Events::AP_SPEED_SLOT_INDEX_SET, "SPEED_SLOT_INDEX_SET", false);
  result &= addInputDataDefinition(hSimConnect, 0, Events::AP_SPD_VAR_INC, "AP_SPD_VAR_INC", true);
  result &= addInputDataDefinition(hSimConnect, 0, Events::AP_SPD_VAR_DEC, "AP_SPD_VAR_DEC", true);
  result &= addInputDataDefinition(hSimConnect, 0, Events::AP_MACH_VAR_INC, "AP_MACH_VAR_INC", true);
  result &= addInputDataDefinition(hSimConnect, 0, Events::AP_MACH_VAR_DEC, "AP_MACH_VAR_DEC", true);
  result &= addInputDataDefinition(hSimConnect, 0, Events::AP_HEADING_SLOT_INDEX_SET, "HEADING_SLOT_INDEX_SET", false);
  result &= addInputDataDefinition(hSimConnect, 0, Events::HEADING_BUG_INC, "HEADING_BUG_INC", true);
  result &= addInputDataDefinition(hSimConnect, 0, Events::HEADING_BUG_DEC, "HEADING_BUG_DEC", true);
  result &= addInputDataDefinition(hSimConnect, 0, Events::AP_ALTITUDE_SLOT_INDEX_SET, "ALTITUDE_SLOT_INDEX_SET", false);
  result &= addInputDataDefinition(hSimConnect, 0, Events::AP_ALT_VAR_INC, "AP_ALT_VAR_INC", true);
  result &= addInputDataDefinition(hSimConnect, 0, Events::AP_ALT_VAR_DEC, "AP_ALT_VAR_DEC", true);
  result &= addInputDataDefinition(hSimConnect, 0, Events::AP_VS_SLOT_INDEX_SET, "VS_SLOT_INDEX_SET", false);
  result &= addInputDataDefinition(hSimConnect, 0, Events::AP_VS_VAR_INC, "AP_VS_VAR_INC", true);
  result &= addInputDataDefinition(hSimConnect, 0, Events::AP_VS_VAR_DEC, "AP_VS_VAR_DEC", true);
  result &= addInputDataDefinition(hSimConnect, 0, Events::AP_APR_HOLD, "AP_APR_HOLD", true);
  result &= addInputDataDefinition(hSimConnect, 0, Events::AP_LOC_HOLD, "AP_LOC_HOLD", true);
  result &= addInputDataDefinition(hSimConnect, 0, Events::AP_AIRSPEED_ON, "AP_AIRSPEED_ON", true);
  result &= addInputDataDefinition(hSimConnect, 0, Events::AP_AIRSPEED_OFF, "AP_AIRSPEED_OFF", true);
  result &= addInputDataDefinition(hSimConnect, 0, Events::AP_HDG_HOLD_ON, "AP_HDG_HOLD_ON", true);
  result &= addInputDataDefinition(hSimConnect, 0, Events::AP_HDG_HOLD_OFF, "AP_HDG_HOLD_OFF", true);
  result &= addInputDataDefinition(hSimConnect, 0, Events::AP_ALT_HOLD_ON, "AP_ALT_HOLD_ON", true);
  result &= addInputDataDefinition(hSimConnect, 0, Events::AP_ALT_HOLD_OFF, "AP_ALT_HOLD_OFF", true);
  result &= addInputDataDefinition(hSimConnect, 0, Events::AP_VS_ON, "AP_VS_ON", true);
  result &= addInputDataDefinition(hSimConnect, 0, Events::AP_VS_OFF, "AP_VS_OFF", true);
  result &= addInputDataDefinition(hSimConnect, 0, Events::AP_ALT_HOLD, "AP_ALT_HOLD", true);
  result &= addInputDataDefinition(hSimConnect, 0, Events::AP_VS_HOLD, "AP_VS_HOLD", true);
  result &= addInputDataDefinition(hSimConnect, 0, Events::AP_ATT_HOLD, "AP_ATT_HOLD", true);
  result &= addInputDataDefinition(hSimConnect, 0, Events::AP_MACH_HOLD, "AP_MACH_HOLD", true);

  result &= addInputDataDefinition(hSimConnect, 0, Events::AUTO_THROTTLE_ARM, "AUTO_THROTTLE_ARM", true);
  result &= addInputDataDefinition(hSimConnect, 0, Events::AUTO_THROTTLE_DISCONNECT, "AUTO_THROTTLE_DISCONNECT", true);
  result &= addInputDataDefinition(hSimConnect, 0, Events::AUTO_THROTTLE_TO_GA, "AUTO_THROTTLE_TO_GA", true);
  result &= addInputDataDefinition(hSimConnect, 0, Events::A32NX_ATHR_RESET_DISABLE, "A32NX.ATHR_RESET_DISABLE", false);

  result &=
      addInputDataDefinition(hSimConnect, 0, Events::A32NX_THROTTLE_MAPPING_SET_DEFAULTS, "A32NX.THROTTLE_MAPPING_SET_DEFAULTS", false);
  result &=
      addInputDataDefinition(hSimConnect, 0, Events::A32NX_THROTTLE_MAPPING_LOAD_FROM_FILE, "A32NX.THROTTLE_MAPPING_LOAD_FROM_FILE", false);
  result &= addInputDataDefinition(hSimConnect, 0, Events::A32NX_THROTTLE_MAPPING_LOAD_FROM_LOCAL_VARIABLES,
                                   "A32NX.THROTTLE_MAPPING_LOAD_FROM_LOCAL_VARIABLES", false);
  result &=
      addInputDataDefinition(hSimConnect, 0, Events::A32NX_THROTTLE_MAPPING_SAVE_TO_FILE, "A32NX.THROTTLE_MAPPING_SAVE_TO_FILE", false);

  result &= addInputDataDefinition(hSimConnect, 0, Events::THROTTLE_SET, "THROTTLE_SET", true);
  result &= addInputDataDefinition(hSimConnect, 0, Events::THROTTLE1_SET, "THROTTLE1_SET", true);
  result &= addInputDataDefinition(hSimConnect, 0, Events::THROTTLE2_SET, "THROTTLE2_SET", true);

  result &= addInputDataDefinition(hSimConnect, 0, Events::THROTTLE_AXIS_SET_EX1, "THROTTLE_AXIS_SET_EX1", true);
  result &= addInputDataDefinition(hSimConnect, 0, Events::THROTTLE1_AXIS_SET_EX1, "THROTTLE1_AXIS_SET_EX1", true);
  result &= addInputDataDefinition(hSimConnect, 0, Events::THROTTLE2_AXIS_SET_EX1, "THROTTLE2_AXIS_SET_EX1", true);

  result &= addInputDataDefinition(hSimConnect, 0, Events::THROTTLE_FULL, "THROTTLE_FULL", true);
  result &= addInputDataDefinition(hSimConnect, 0, Events::THROTTLE_CUT, "THROTTLE_CUT", true);
  result &= addInputDataDefinition(hSimConnect, 0, Events::THROTTLE_INCR, "THROTTLE_INCR", true);
  result &= addInputDataDefinition(hSimConnect, 0, Events::THROTTLE_DECR, "THROTTLE_DECR", true);
  result &= addInputDataDefinition(hSimConnect, 0, Events::THROTTLE_INCR_SMALL, "THROTTLE_INCR_SMALL", true);
  result &= addInputDataDefinition(hSimConnect, 0, Events::THROTTLE_DECR_SMALL, "THROTTLE_DECR_SMALL", true);

  result &= addInputDataDefinition(hSimConnect, 0, Events::THROTTLE_10, "THROTTLE_10", true);
  result &= addInputDataDefinition(hSimConnect, 0, Events::THROTTLE_20, "THROTTLE_20", true);
  result &= addInputDataDefinition(hSimConnect, 0, Events::THROTTLE_30, "THROTTLE_30", true);
  result &= addInputDataDefinition(hSimConnect, 0, Events::THROTTLE_40, "THROTTLE_40", true);
  result &= addInputDataDefinition(hSimConnect, 0, Events::THROTTLE_50, "THROTTLE_50", true);
  result &= addInputDataDefinition(hSimConnect, 0, Events::THROTTLE_60, "THROTTLE_60", true);
  result &= addInputDataDefinition(hSimConnect, 0, Events::THROTTLE_70, "THROTTLE_70", true);
  result &= addInputDataDefinition(hSimConnect, 0, Events::THROTTLE_80, "THROTTLE_80", true);
  result &= addInputDataDefinition(hSimConnect, 0, Events::THROTTLE_90, "THROTTLE_90", true);

  result &= addInputDataDefinition(hSimConnect, 0, Events::THROTTLE1_FULL, "THROTTLE1_FULL", true);
  result &= addInputDataDefinition(hSimConnect, 0, Events::THROTTLE1_CUT, "THROTTLE1_CUT", true);
  result &= addInputDataDefinition(hSimConnect, 0, Events::THROTTLE1_INCR, "THROTTLE1_INCR", true);
  result &= addInputDataDefinition(hSimConnect, 0, Events::THROTTLE1_DECR, "THROTTLE1_DECR", true);
  result &= addInputDataDefinition(hSimConnect, 0, Events::THROTTLE1_INCR_SMALL, "THROTTLE1_INCR_SMALL", true);
  result &= addInputDataDefinition(hSimConnect, 0, Events::THROTTLE1_DECR_SMALL, "THROTTLE1_DECR_SMALL", true);

  result &= addInputDataDefinition(hSimConnect, 0, Events::THROTTLE2_FULL, "THROTTLE2_FULL", true);
  result &= addInputDataDefinition(hSimConnect, 0, Events::THROTTLE2_CUT, "THROTTLE2_CUT", true);
  result &= addInputDataDefinition(hSimConnect, 0, Events::THROTTLE2_INCR, "THROTTLE2_INCR", true);
  result &= addInputDataDefinition(hSimConnect, 0, Events::THROTTLE2_DECR, "THROTTLE2_DECR", true);
  result &= addInputDataDefinition(hSimConnect, 0, Events::THROTTLE2_INCR_SMALL, "THROTTLE2_INCR_SMALL", true);
  result &= addInputDataDefinition(hSimConnect, 0, Events::THROTTLE2_DECR_SMALL, "THROTTLE2_DECR_SMALL", true);

  result &= addInputDataDefinition(hSimConnect, 0, Events::THROTTLE_REVERSE_THRUST_TOGGLE, "THROTTLE_REVERSE_THRUST_TOGGLE", true);
  result &= addInputDataDefinition(hSimConnect, 0, Events::THROTTLE_REVERSE_THRUST_HOLD, "THROTTLE_REVERSE_THRUST_HOLD", true);

  result &= addInputDataDefinition(hSimConnect, 0, Events::SPOILERS_ON, "SPOILERS_ON", true);
  result &= addInputDataDefinition(hSimConnect, 0, Events::SPOILERS_OFF, "SPOILERS_OFF", true);
  result &= addInputDataDefinition(hSimConnect, 0, Events::SPOILERS_TOGGLE, "SPOILERS_TOGGLE", true);
  result &= addInputDataDefinition(hSimConnect, 0, Events::SPOILERS_SET, "SPOILERS_SET", true);
  result &= addInputDataDefinition(hSimConnect, 0, Events::AXIS_SPOILER_SET, "AXIS_SPOILER_SET", true);
  result &= addInputDataDefinition(hSimConnect, 0, Events::SPOILERS_ARM_ON, "SPOILERS_ARM_ON", true);
  result &= addInputDataDefinition(hSimConnect, 0, Events::SPOILERS_ARM_OFF, "SPOILERS_ARM_OFF", true);
  result &= addInputDataDefinition(hSimConnect, 0, Events::SPOILERS_ARM_TOGGLE, "SPOILERS_ARM_TOGGLE", true);
  result &= addInputDataDefinition(hSimConnect, 0, Events::SPOILERS_ARM_SET, "SPOILERS_ARM_SET", true);

  result &= addInputDataDefinition(hSimConnect, 0, Events::SIM_RATE_INCR, "SIM_RATE_INCR", true);
  result &= addInputDataDefinition(hSimConnect, 0, Events::SIM_RATE_DECR, "SIM_RATE_DECR", true);
  result &= addInputDataDefinition(hSimConnect, 0, Events::SIM_RATE_SET, "SIM_RATE_SET", true);

  return result;
}

bool SimConnectInterface::prepareSimOutputSimConnectDataDefinitions() {
  bool result = true;

  result &= addDataDefinition(hSimConnect, 2, SIMCONNECT_DATATYPE_FLOAT64, "ELEVATOR TRIM POSITION", "DEGREE");

  result &= addDataDefinition(hSimConnect, 3, SIMCONNECT_DATATYPE_FLOAT64, "RUDDER TRIM PCT", "PERCENT OVER 100");

  result &= addDataDefinition(hSimConnect, 4, SIMCONNECT_DATATYPE_FLOAT64, "GENERAL ENG THROTTLE LEVER POSITION:1", "PERCENT");
  result &= addDataDefinition(hSimConnect, 4, SIMCONNECT_DATATYPE_FLOAT64, "GENERAL ENG THROTTLE LEVER POSITION:2", "PERCENT");
  result &= addDataDefinition(hSimConnect, 4, SIMCONNECT_DATATYPE_FLOAT64, "GENERAL ENG THROTTLE MANAGED MODE:1", "NUMBER");
  result &= addDataDefinition(hSimConnect, 4, SIMCONNECT_DATATYPE_FLOAT64, "GENERAL ENG THROTTLE MANAGED MODE:2", "NUMBER");

  result &= addDataDefinition(hSimConnect, 6, SIMCONNECT_DATATYPE_FLOAT64, "SPOILERS HANDLE POSITION", "POSITION");

  result &= addDataDefinition(hSimConnect, 7, SIMCONNECT_DATATYPE_INT64, "KOHLSMAN SETTING STD:3", "BOOL");

  return result;
}

bool SimConnectInterface::prepareClientDataDefinitions() {
  // variable for result
  HRESULT result;

  // ------------------------------------------------------------------------------------------------------------------

  // map client id
  result = SimConnect_MapClientDataNameToID(hSimConnect, "A32NX_CLIENT_DATA_AUTOPILOT_STATE_MACHINE", ClientData::AUTOPILOT_STATE_MACHINE);
  // create client data
  result &= SimConnect_CreateClientData(hSimConnect, ClientData::AUTOPILOT_STATE_MACHINE, sizeof(ClientDataAutopilotStateMachine),
                                        SIMCONNECT_CREATE_CLIENT_DATA_FLAG_DEFAULT);
  // add data definitions
  result &= SimConnect_AddToClientDataDefinition(hSimConnect, ClientData::AUTOPILOT_STATE_MACHINE, SIMCONNECT_CLIENTDATAOFFSET_AUTO,
                                                 SIMCONNECT_CLIENTDATATYPE_INT64);
  result &= SimConnect_AddToClientDataDefinition(hSimConnect, ClientData::AUTOPILOT_STATE_MACHINE, SIMCONNECT_CLIENTDATAOFFSET_AUTO,
                                                 SIMCONNECT_CLIENTDATATYPE_INT64);
  result &= SimConnect_AddToClientDataDefinition(hSimConnect, ClientData::AUTOPILOT_STATE_MACHINE, SIMCONNECT_CLIENTDATAOFFSET_AUTO,
                                                 SIMCONNECT_CLIENTDATATYPE_FLOAT64);
  result &= SimConnect_AddToClientDataDefinition(hSimConnect, ClientData::AUTOPILOT_STATE_MACHINE, SIMCONNECT_CLIENTDATAOFFSET_AUTO,
                                                 SIMCONNECT_CLIENTDATATYPE_FLOAT64);
  result &= SimConnect_AddToClientDataDefinition(hSimConnect, ClientData::AUTOPILOT_STATE_MACHINE, SIMCONNECT_CLIENTDATAOFFSET_AUTO,
                                                 SIMCONNECT_CLIENTDATATYPE_FLOAT64);
  result &= SimConnect_AddToClientDataDefinition(hSimConnect, ClientData::AUTOPILOT_STATE_MACHINE, SIMCONNECT_CLIENTDATAOFFSET_AUTO,
                                                 SIMCONNECT_CLIENTDATATYPE_FLOAT64);
  result &= SimConnect_AddToClientDataDefinition(hSimConnect, ClientData::AUTOPILOT_STATE_MACHINE, SIMCONNECT_CLIENTDATAOFFSET_AUTO,
                                                 SIMCONNECT_CLIENTDATATYPE_FLOAT64);
  result &= SimConnect_AddToClientDataDefinition(hSimConnect, ClientData::AUTOPILOT_STATE_MACHINE, SIMCONNECT_CLIENTDATAOFFSET_AUTO,
                                                 SIMCONNECT_CLIENTDATATYPE_FLOAT64);
  result &= SimConnect_AddToClientDataDefinition(hSimConnect, ClientData::AUTOPILOT_STATE_MACHINE, SIMCONNECT_CLIENTDATAOFFSET_AUTO,
                                                 SIMCONNECT_CLIENTDATATYPE_FLOAT64);
  result &= SimConnect_AddToClientDataDefinition(hSimConnect, ClientData::AUTOPILOT_STATE_MACHINE, SIMCONNECT_CLIENTDATAOFFSET_AUTO,
                                                 SIMCONNECT_CLIENTDATATYPE_FLOAT64);
  result &= SimConnect_AddToClientDataDefinition(hSimConnect, ClientData::AUTOPILOT_STATE_MACHINE, SIMCONNECT_CLIENTDATAOFFSET_AUTO,
                                                 SIMCONNECT_CLIENTDATATYPE_FLOAT64);
  result &= SimConnect_AddToClientDataDefinition(hSimConnect, ClientData::AUTOPILOT_STATE_MACHINE, SIMCONNECT_CLIENTDATAOFFSET_AUTO,
                                                 SIMCONNECT_CLIENTDATATYPE_FLOAT64);
  result &= SimConnect_AddToClientDataDefinition(hSimConnect, ClientData::AUTOPILOT_STATE_MACHINE, SIMCONNECT_CLIENTDATAOFFSET_AUTO,
                                                 SIMCONNECT_CLIENTDATATYPE_FLOAT64);
  result &= SimConnect_AddToClientDataDefinition(hSimConnect, ClientData::AUTOPILOT_STATE_MACHINE, SIMCONNECT_CLIENTDATAOFFSET_AUTO,
                                                 SIMCONNECT_CLIENTDATATYPE_FLOAT64);
  result &= SimConnect_AddToClientDataDefinition(hSimConnect, ClientData::AUTOPILOT_STATE_MACHINE, SIMCONNECT_CLIENTDATAOFFSET_AUTO,
                                                 SIMCONNECT_CLIENTDATATYPE_FLOAT64);
  result &= SimConnect_AddToClientDataDefinition(hSimConnect, ClientData::AUTOPILOT_STATE_MACHINE, SIMCONNECT_CLIENTDATAOFFSET_AUTO,
                                                 SIMCONNECT_CLIENTDATATYPE_FLOAT64);
  result &= SimConnect_AddToClientDataDefinition(hSimConnect, ClientData::AUTOPILOT_STATE_MACHINE, SIMCONNECT_CLIENTDATAOFFSET_AUTO,
                                                 SIMCONNECT_CLIENTDATATYPE_FLOAT64);
  result &= SimConnect_AddToClientDataDefinition(hSimConnect, ClientData::AUTOPILOT_STATE_MACHINE, SIMCONNECT_CLIENTDATAOFFSET_AUTO,
                                                 SIMCONNECT_CLIENTDATATYPE_FLOAT64);
  result &= SimConnect_AddToClientDataDefinition(hSimConnect, ClientData::AUTOPILOT_STATE_MACHINE, SIMCONNECT_CLIENTDATAOFFSET_AUTO,
                                                 SIMCONNECT_CLIENTDATATYPE_FLOAT64);
  result &= SimConnect_AddToClientDataDefinition(hSimConnect, ClientData::AUTOPILOT_STATE_MACHINE, SIMCONNECT_CLIENTDATAOFFSET_AUTO,
                                                 SIMCONNECT_CLIENTDATATYPE_FLOAT64);
  result &= SimConnect_AddToClientDataDefinition(hSimConnect, ClientData::AUTOPILOT_STATE_MACHINE, SIMCONNECT_CLIENTDATAOFFSET_AUTO,
                                                 SIMCONNECT_CLIENTDATATYPE_FLOAT64);
  result &= SimConnect_AddToClientDataDefinition(hSimConnect, ClientData::AUTOPILOT_STATE_MACHINE, SIMCONNECT_CLIENTDATAOFFSET_AUTO,
                                                 SIMCONNECT_CLIENTDATATYPE_FLOAT64);
  result &= SimConnect_AddToClientDataDefinition(hSimConnect, ClientData::AUTOPILOT_STATE_MACHINE, SIMCONNECT_CLIENTDATAOFFSET_AUTO,
                                                 SIMCONNECT_CLIENTDATATYPE_FLOAT64);
  result &= SimConnect_AddToClientDataDefinition(hSimConnect, ClientData::AUTOPILOT_STATE_MACHINE, SIMCONNECT_CLIENTDATAOFFSET_AUTO,
                                                 SIMCONNECT_CLIENTDATATYPE_FLOAT64);
  result &= SimConnect_AddToClientDataDefinition(hSimConnect, ClientData::AUTOPILOT_STATE_MACHINE, SIMCONNECT_CLIENTDATAOFFSET_AUTO,
                                                 SIMCONNECT_CLIENTDATATYPE_FLOAT64);
  result &= SimConnect_AddToClientDataDefinition(hSimConnect, ClientData::AUTOPILOT_STATE_MACHINE, SIMCONNECT_CLIENTDATAOFFSET_AUTO,
                                                 SIMCONNECT_CLIENTDATATYPE_FLOAT64);
  result &= SimConnect_AddToClientDataDefinition(hSimConnect, ClientData::AUTOPILOT_STATE_MACHINE, SIMCONNECT_CLIENTDATAOFFSET_AUTO,
                                                 SIMCONNECT_CLIENTDATATYPE_FLOAT64);
  result &= SimConnect_AddToClientDataDefinition(hSimConnect, ClientData::AUTOPILOT_STATE_MACHINE, SIMCONNECT_CLIENTDATAOFFSET_AUTO,
                                                 SIMCONNECT_CLIENTDATATYPE_FLOAT64);
  result &= SimConnect_AddToClientDataDefinition(hSimConnect, ClientData::AUTOPILOT_STATE_MACHINE, SIMCONNECT_CLIENTDATAOFFSET_AUTO,
                                                 SIMCONNECT_CLIENTDATATYPE_FLOAT64);
  result &= SimConnect_AddToClientDataDefinition(hSimConnect, ClientData::AUTOPILOT_STATE_MACHINE, SIMCONNECT_CLIENTDATAOFFSET_AUTO,
                                                 SIMCONNECT_CLIENTDATATYPE_FLOAT64);
  result &= SimConnect_AddToClientDataDefinition(hSimConnect, ClientData::AUTOPILOT_STATE_MACHINE, SIMCONNECT_CLIENTDATAOFFSET_AUTO,
                                                 SIMCONNECT_CLIENTDATATYPE_INT64);
  result &= SimConnect_AddToClientDataDefinition(hSimConnect, ClientData::AUTOPILOT_STATE_MACHINE, SIMCONNECT_CLIENTDATAOFFSET_AUTO,
                                                 SIMCONNECT_CLIENTDATATYPE_INT64);
  result &= SimConnect_AddToClientDataDefinition(hSimConnect, ClientData::AUTOPILOT_STATE_MACHINE, SIMCONNECT_CLIENTDATAOFFSET_AUTO,
                                                 SIMCONNECT_CLIENTDATATYPE_INT64);

  // request data to be updated when set
  result &= SimConnect_RequestClientData(hSimConnect, ClientData::AUTOPILOT_STATE_MACHINE, ClientData::AUTOPILOT_STATE_MACHINE,
                                         ClientData::AUTOPILOT_STATE_MACHINE, SIMCONNECT_CLIENT_DATA_PERIOD_ON_SET);

  // ------------------------------------------------------------------------------------------------------------------

  // map client id
  result &= SimConnect_MapClientDataNameToID(hSimConnect, "A32NX_CLIENT_DATA_AUTOPILOT_LAWS", ClientData::AUTOPILOT_LAWS);
  // create client data
  result &= SimConnect_CreateClientData(hSimConnect, ClientData::AUTOPILOT_LAWS, sizeof(ClientDataAutopilotLaws),
                                        SIMCONNECT_CREATE_CLIENT_DATA_FLAG_DEFAULT);
  // add data definitions
  result &= SimConnect_AddToClientDataDefinition(hSimConnect, ClientData::AUTOPILOT_LAWS, SIMCONNECT_CLIENTDATAOFFSET_AUTO,
                                                 SIMCONNECT_CLIENTDATATYPE_INT64);
  result &= SimConnect_AddToClientDataDefinition(hSimConnect, ClientData::AUTOPILOT_LAWS, SIMCONNECT_CLIENTDATAOFFSET_AUTO,
                                                 SIMCONNECT_CLIENTDATATYPE_FLOAT64);
  result &= SimConnect_AddToClientDataDefinition(hSimConnect, ClientData::AUTOPILOT_LAWS, SIMCONNECT_CLIENTDATAOFFSET_AUTO,
                                                 SIMCONNECT_CLIENTDATATYPE_FLOAT64);
  result &= SimConnect_AddToClientDataDefinition(hSimConnect, ClientData::AUTOPILOT_LAWS, SIMCONNECT_CLIENTDATAOFFSET_AUTO,
                                                 SIMCONNECT_CLIENTDATATYPE_FLOAT64);
  result &= SimConnect_AddToClientDataDefinition(hSimConnect, ClientData::AUTOPILOT_LAWS, SIMCONNECT_CLIENTDATAOFFSET_AUTO,
                                                 SIMCONNECT_CLIENTDATATYPE_FLOAT64);
  result &= SimConnect_AddToClientDataDefinition(hSimConnect, ClientData::AUTOPILOT_LAWS, SIMCONNECT_CLIENTDATAOFFSET_AUTO,
                                                 SIMCONNECT_CLIENTDATATYPE_FLOAT64);
  result &= SimConnect_AddToClientDataDefinition(hSimConnect, ClientData::AUTOPILOT_LAWS, SIMCONNECT_CLIENTDATAOFFSET_AUTO,
                                                 SIMCONNECT_CLIENTDATATYPE_FLOAT64);
  result &= SimConnect_AddToClientDataDefinition(hSimConnect, ClientData::AUTOPILOT_LAWS, SIMCONNECT_CLIENTDATAOFFSET_AUTO,
                                                 SIMCONNECT_CLIENTDATATYPE_FLOAT64);
  result &= SimConnect_AddToClientDataDefinition(hSimConnect, ClientData::AUTOPILOT_LAWS, SIMCONNECT_CLIENTDATAOFFSET_AUTO,
                                                 SIMCONNECT_CLIENTDATATYPE_INT64);

  // request data to be updated when set
  result &= SimConnect_RequestClientData(hSimConnect, ClientData::AUTOPILOT_LAWS, ClientData::AUTOPILOT_LAWS, ClientData::AUTOPILOT_LAWS,
                                         SIMCONNECT_CLIENT_DATA_PERIOD_ON_SET);

  // ------------------------------------------------------------------------------------------------------------------

  // map client id
  result &= SimConnect_MapClientDataNameToID(hSimConnect, "A32NX_CLIENT_DATA_AUTOTHRUST", ClientData::AUTOTHRUST);
  // create client data
  result &= SimConnect_CreateClientData(hSimConnect, ClientData::AUTOTHRUST, sizeof(ClientDataAutothrust),
                                        SIMCONNECT_CREATE_CLIENT_DATA_FLAG_DEFAULT);
  // add data definitions
  result &= SimConnect_AddToClientDataDefinition(hSimConnect, ClientData::AUTOTHRUST, SIMCONNECT_CLIENTDATAOFFSET_AUTO,
                                                 SIMCONNECT_CLIENTDATATYPE_FLOAT64);
  result &= SimConnect_AddToClientDataDefinition(hSimConnect, ClientData::AUTOTHRUST, SIMCONNECT_CLIENTDATAOFFSET_AUTO,
                                                 SIMCONNECT_CLIENTDATATYPE_FLOAT64);
  result &= SimConnect_AddToClientDataDefinition(hSimConnect, ClientData::AUTOTHRUST, SIMCONNECT_CLIENTDATAOFFSET_AUTO,
                                                 SIMCONNECT_CLIENTDATATYPE_FLOAT64);
  result &= SimConnect_AddToClientDataDefinition(hSimConnect, ClientData::AUTOTHRUST, SIMCONNECT_CLIENTDATAOFFSET_AUTO,
                                                 SIMCONNECT_CLIENTDATATYPE_FLOAT64);
  result &= SimConnect_AddToClientDataDefinition(hSimConnect, ClientData::AUTOTHRUST, SIMCONNECT_CLIENTDATAOFFSET_AUTO,
                                                 SIMCONNECT_CLIENTDATATYPE_FLOAT64);
  result &= SimConnect_AddToClientDataDefinition(hSimConnect, ClientData::AUTOTHRUST, SIMCONNECT_CLIENTDATAOFFSET_AUTO,
                                                 SIMCONNECT_CLIENTDATATYPE_FLOAT64);
  result &= SimConnect_AddToClientDataDefinition(hSimConnect, ClientData::AUTOTHRUST, SIMCONNECT_CLIENTDATAOFFSET_AUTO,
                                                 SIMCONNECT_CLIENTDATATYPE_FLOAT64);
  result &= SimConnect_AddToClientDataDefinition(hSimConnect, ClientData::AUTOTHRUST, SIMCONNECT_CLIENTDATAOFFSET_AUTO,
                                                 SIMCONNECT_CLIENTDATATYPE_FLOAT64);
  result &= SimConnect_AddToClientDataDefinition(hSimConnect, ClientData::AUTOTHRUST, SIMCONNECT_CLIENTDATAOFFSET_AUTO,
                                                 SIMCONNECT_CLIENTDATATYPE_FLOAT64);
  result &= SimConnect_AddToClientDataDefinition(hSimConnect, ClientData::AUTOTHRUST, SIMCONNECT_CLIENTDATAOFFSET_AUTO,
                                                 SIMCONNECT_CLIENTDATATYPE_FLOAT64);
  result &= SimConnect_AddToClientDataDefinition(hSimConnect, ClientData::AUTOTHRUST, SIMCONNECT_CLIENTDATAOFFSET_AUTO,
                                                 SIMCONNECT_CLIENTDATATYPE_FLOAT64);

  // request data to be updated when set
  result &= SimConnect_RequestClientData(hSimConnect, ClientData::AUTOTHRUST, ClientData::AUTOTHRUST, ClientData::AUTOTHRUST,
                                         SIMCONNECT_CLIENT_DATA_PERIOD_ON_SET);

  // ------------------------------------------------------------------------------------------------------------------

  // map client id
  result &= SimConnect_MapClientDataNameToID(hSimConnect, "A32NX_CLIENT_DATA_ELAC_DISCRETE_INPUT", ClientData::ELAC_DISCRETE_INPUTS);
  // create client data
  result &= SimConnect_CreateClientData(hSimConnect, ClientData::ELAC_DISCRETE_INPUTS, sizeof(base_elac_discrete_inputs),
                                        SIMCONNECT_CREATE_CLIENT_DATA_FLAG_DEFAULT);
  // add data definitions
  for (int i = 0; i < 32; i++) {
    result &= SimConnect_AddToClientDataDefinition(hSimConnect, ClientData::ELAC_DISCRETE_INPUTS, SIMCONNECT_CLIENTDATAOFFSET_AUTO,
                                                   SIMCONNECT_CLIENTDATATYPE_INT8);
  }

  // ------------------------------------------------------------------------------------------------------------------

  // map client id
  result &= SimConnect_MapClientDataNameToID(hSimConnect, "A32NX_CLIENT_DATA_ELAC_ANALOG_INPUT", ClientData::ELAC_ANALOG_INPUTS);
  // create client data
  result &= SimConnect_CreateClientData(hSimConnect, ClientData::ELAC_ANALOG_INPUTS, sizeof(base_elac_analog_inputs),
                                        SIMCONNECT_CREATE_CLIENT_DATA_FLAG_DEFAULT);
  // add data definitions
  for (int i = 0; i < 15; i++) {
    result &= SimConnect_AddToClientDataDefinition(hSimConnect, ClientData::ELAC_ANALOG_INPUTS, SIMCONNECT_CLIENTDATAOFFSET_AUTO,
                                                   SIMCONNECT_CLIENTDATATYPE_FLOAT64);
  }

  // ------------------------------------------------------------------------------------------------------------------

  // map client id
  result &= SimConnect_MapClientDataNameToID(hSimConnect, "A32NX_CLIENT_DATA_ELAC_DISCRETES_OUTPUT", ClientData::ELAC_DISCRETE_OUTPUTS);
  // create client data
  result &= SimConnect_CreateClientData(hSimConnect, ClientData::ELAC_DISCRETE_OUTPUTS, sizeof(base_elac_discrete_outputs),
                                        SIMCONNECT_CREATE_CLIENT_DATA_FLAG_DEFAULT);
  // add data definitions
  for (int i = 0; i < 12; i++) {
    result &= SimConnect_AddToClientDataDefinition(hSimConnect, ClientData::ELAC_DISCRETE_OUTPUTS, SIMCONNECT_CLIENTDATAOFFSET_AUTO,
                                                   SIMCONNECT_CLIENTDATATYPE_INT8);
  }

  // request data to be updated when set
  result &= SimConnect_RequestClientData(hSimConnect, ClientData::ELAC_DISCRETE_OUTPUTS, ClientData::ELAC_DISCRETE_OUTPUTS,
                                         ClientData::ELAC_DISCRETE_OUTPUTS, SIMCONNECT_CLIENT_DATA_PERIOD_ON_SET);

  // ------------------------------------------------------------------------------------------------------------------

  // map client id
  result &= SimConnect_MapClientDataNameToID(hSimConnect, "A32NX_CLIENT_DATA_ELAC_ANALOGS_OUTPUT", ClientData::ELAC_ANALOG_OUTPUTS);
  // create client data
  result &= SimConnect_CreateClientData(hSimConnect, ClientData::ELAC_ANALOG_OUTPUTS, sizeof(base_elac_analog_outputs),
                                        SIMCONNECT_CREATE_CLIENT_DATA_FLAG_DEFAULT);
  // add data definitions
  for (int i = 0; i < 5; i++) {
    result &= SimConnect_AddToClientDataDefinition(hSimConnect, ClientData::ELAC_ANALOG_OUTPUTS, SIMCONNECT_CLIENTDATAOFFSET_AUTO,
                                                   SIMCONNECT_CLIENTDATATYPE_FLOAT64);
  }

  // request data to be updated when set
  result &= SimConnect_RequestClientData(hSimConnect, ClientData::ELAC_ANALOG_OUTPUTS, ClientData::ELAC_ANALOG_OUTPUTS,
                                         ClientData::ELAC_ANALOG_OUTPUTS, SIMCONNECT_CLIENT_DATA_PERIOD_ON_SET);

  // ------------------------------------------------------------------------------------------------------------------

  for (int i = 0; i < 2; i++) {
    auto defineId = ClientData::ELAC_1_BUS_OUTPUT + i;

    // map client id
    result &= SimConnect_MapClientDataNameToID(hSimConnect, ("A32NX_CLIENT_DATA_ELAC_" + std::to_string(i + 1) + "_BUS").c_str(), defineId);
    // create client data
    result &= SimConnect_CreateClientData(hSimConnect, defineId, sizeof(base_elac_out_bus), SIMCONNECT_CREATE_CLIENT_DATA_FLAG_DEFAULT);
    // add data definitions
    for (int i = 0; i < 17; i++) {
      result &=
          SimConnect_AddToClientDataDefinition(hSimConnect, defineId, SIMCONNECT_CLIENTDATAOFFSET_AUTO, SIMCONNECT_CLIENTDATATYPE_FLOAT64);
    }

    // request data to be updated when set
    if (i == elacDisabled) {
      result &= SimConnect_RequestClientData(hSimConnect, defineId, defineId, defineId, SIMCONNECT_CLIENT_DATA_PERIOD_ON_SET);
    }
  }

  // ------------------------------------------------------------------------------------------------------------------

  for (int i = 0; i < 3; i++) {
    auto defineId = ClientData::ADR_1_INPUTS + i;

    // map client id
    result &=
        SimConnect_MapClientDataNameToID(hSimConnect, ("A32NX_CLIENT_DATA_ADR_" + std::to_string(i + 1) + "_INPUT").c_str(), defineId);
    // create client data
    result &= SimConnect_CreateClientData(hSimConnect, defineId, sizeof(base_adr_bus), SIMCONNECT_CREATE_CLIENT_DATA_FLAG_DEFAULT);
    // add data definitions
    for (int i = 0; i < 8; i++) {
      result &=
          SimConnect_AddToClientDataDefinition(hSimConnect, defineId, SIMCONNECT_CLIENTDATAOFFSET_AUTO, SIMCONNECT_CLIENTDATATYPE_FLOAT64);
    }
  }

  // ------------------------------------------------------------------------------------------------------------------

  for (int i = 0; i < 3; i++) {
    auto defineId = ClientData::IR_1_INPUTS + i;

    // map client id
    result &= SimConnect_MapClientDataNameToID(hSimConnect, ("A32NX_CLIENT_DATA_IR_" + std::to_string(i + 1) + "_INPUT").c_str(), defineId);
    // create client data
    result &= SimConnect_CreateClientData(hSimConnect, defineId, sizeof(base_ir_bus), SIMCONNECT_CREATE_CLIENT_DATA_FLAG_DEFAULT);
    // add data definitions
    for (int i = 0; i < 31; i++) {
      result &=
          SimConnect_AddToClientDataDefinition(hSimConnect, defineId, SIMCONNECT_CLIENTDATAOFFSET_AUTO, SIMCONNECT_CLIENTDATATYPE_FLOAT64);
    }
  }

  // ------------------------------------------------------------------------------------------------------------------

  for (int i = 0; i < 2; i++) {
    auto defineId = ClientData::RA_1_BUS + i;
    // map client id
    result &= SimConnect_MapClientDataNameToID(hSimConnect, ("A32NX_CLIENT_DATA_RA_" + std::to_string(i + 1) + "_BUS").c_str(), defineId);
    // create client data
    result &= SimConnect_CreateClientData(hSimConnect, defineId, sizeof(base_ra_bus), SIMCONNECT_CREATE_CLIENT_DATA_FLAG_DEFAULT);
    // add data definitions
    result &=
        SimConnect_AddToClientDataDefinition(hSimConnect, defineId, SIMCONNECT_CLIENTDATAOFFSET_AUTO, SIMCONNECT_CLIENTDATATYPE_FLOAT64);
  }

  // ------------------------------------------------------------------------------------------------------------------

  for (int i = 0; i < 2; i++) {
    auto defineId = ClientData::LGCIU_1_BUS + i;
    // map client id
    result &=
        SimConnect_MapClientDataNameToID(hSimConnect, ("A32NX_CLIENT_DATA_LGCIU_" + std::to_string(i + 1) + "_BUS").c_str(), defineId);
    // create client data
    result &= SimConnect_CreateClientData(hSimConnect, defineId, sizeof(base_lgciu_bus), SIMCONNECT_CREATE_CLIENT_DATA_FLAG_DEFAULT);
    // add data definitions
    for (int i = 0; i < 4; i++) {
      result &=
          SimConnect_AddToClientDataDefinition(hSimConnect, defineId, SIMCONNECT_CLIENTDATAOFFSET_AUTO, SIMCONNECT_CLIENTDATATYPE_FLOAT64);
    }
  }

  // ------------------------------------------------------------------------------------------------------------------

  for (int i = 0; i < 2; i++) {
    auto defineId = ClientData::SFCC_1_BUS + i;
    // map client id
    result &= SimConnect_MapClientDataNameToID(hSimConnect, ("A32NX_CLIENT_DATA_SFCC_" + std::to_string(i + 1) + "_BUS").c_str(), defineId);
    // create client data
    result &= SimConnect_CreateClientData(hSimConnect, defineId, sizeof(base_sfcc_bus), SIMCONNECT_CREATE_CLIENT_DATA_FLAG_DEFAULT);
    // add data definitions
    for (int i = 0; i < 5; i++) {
      result &=
          SimConnect_AddToClientDataDefinition(hSimConnect, defineId, SIMCONNECT_CLIENTDATAOFFSET_AUTO, SIMCONNECT_CLIENTDATATYPE_FLOAT64);
    }
  }

  // ------------------------------------------------------------------------------------------------------------------

  for (int i = 0; i < 2; i++) {
    auto defineId = ClientData::FMGC_1_B_BUS + i;
    // map client id
    result &=
        SimConnect_MapClientDataNameToID(hSimConnect, ("A32NX_CLIENT_DATA_FMGC_" + std::to_string(i + 1) + "_B_BUS").c_str(), defineId);
    // create client data
    result &= SimConnect_CreateClientData(hSimConnect, defineId, sizeof(base_fmgc_b_bus), SIMCONNECT_CREATE_CLIENT_DATA_FLAG_DEFAULT);
    // add data definitions
    for (int i = 0; i < 18; i++) {
      result &=
          SimConnect_AddToClientDataDefinition(hSimConnect, defineId, SIMCONNECT_CLIENTDATAOFFSET_AUTO, SIMCONNECT_CLIENTDATATYPE_FLOAT64);
    }
  }

  // ------------------------------------------------------------------------------------------------------------------

  // map client id
  result &= SimConnect_MapClientDataNameToID(hSimConnect, "A32NX_CLIENT_DATA_SEC_DISCRETE_INPUT", ClientData::SEC_DISCRETE_INPUTS);
  // create client data
  result &= SimConnect_CreateClientData(hSimConnect, ClientData::SEC_DISCRETE_INPUTS, sizeof(base_sec_discrete_inputs),
                                        SIMCONNECT_CREATE_CLIENT_DATA_FLAG_DEFAULT);
  // add data definitions
  for (int i = 0; i < 26; i++) {
    result &= SimConnect_AddToClientDataDefinition(hSimConnect, ClientData::SEC_DISCRETE_INPUTS, SIMCONNECT_CLIENTDATAOFFSET_AUTO,
                                                   SIMCONNECT_CLIENTDATATYPE_INT8);
  }

  // ------------------------------------------------------------------------------------------------------------------

  // map client id
  result &= SimConnect_MapClientDataNameToID(hSimConnect, "A32NX_CLIENT_DATA_SEC_ANALOG_INPUT", ClientData::SEC_ANALOG_INPUTS);
  // create client data
  result &= SimConnect_CreateClientData(hSimConnect, ClientData::SEC_ANALOG_INPUTS, sizeof(base_sec_analog_inputs),
                                        SIMCONNECT_CREATE_CLIENT_DATA_FLAG_DEFAULT);
  // add data definitions
  for (int i = 0; i < 18; i++) {
    result &= SimConnect_AddToClientDataDefinition(hSimConnect, ClientData::SEC_ANALOG_INPUTS, SIMCONNECT_CLIENTDATAOFFSET_AUTO,
                                                   SIMCONNECT_CLIENTDATATYPE_FLOAT64);
  }

  // ------------------------------------------------------------------------------------------------------------------

  // map client id
  result &= SimConnect_MapClientDataNameToID(hSimConnect, "A32NX_CLIENT_DATA_SEC_DISCRETES_OUTPUT", ClientData::SEC_DISCRETE_OUTPUTS);
  // create client data
  result &= SimConnect_CreateClientData(hSimConnect, ClientData::SEC_DISCRETE_OUTPUTS, sizeof(base_sec_discrete_outputs),
                                        SIMCONNECT_CREATE_CLIENT_DATA_FLAG_DEFAULT);
  // add data definitions
  for (int i = 0; i < 9; i++) {
    result &= SimConnect_AddToClientDataDefinition(hSimConnect, ClientData::SEC_DISCRETE_OUTPUTS, SIMCONNECT_CLIENTDATAOFFSET_AUTO,
                                                   SIMCONNECT_CLIENTDATATYPE_INT8);
  }

  // request data to be updated when set
  result &= SimConnect_RequestClientData(hSimConnect, ClientData::SEC_DISCRETE_OUTPUTS, ClientData::SEC_DISCRETE_OUTPUTS,
                                         ClientData::SEC_DISCRETE_OUTPUTS, SIMCONNECT_CLIENT_DATA_PERIOD_ON_SET);

  // ------------------------------------------------------------------------------------------------------------------

  // map client id
  result &= SimConnect_MapClientDataNameToID(hSimConnect, "A32NX_CLIENT_DATA_SEC_ANALOGS_OUTPUT", ClientData::SEC_ANALOG_OUTPUTS);
  // create client data
  result &= SimConnect_CreateClientData(hSimConnect, ClientData::SEC_ANALOG_OUTPUTS, sizeof(base_sec_analog_outputs),
                                        SIMCONNECT_CREATE_CLIENT_DATA_FLAG_DEFAULT);
  // add data definitions
  for (int i = 0; i < 7; i++) {
    result &= SimConnect_AddToClientDataDefinition(hSimConnect, ClientData::SEC_ANALOG_OUTPUTS, SIMCONNECT_CLIENTDATAOFFSET_AUTO,
                                                   SIMCONNECT_CLIENTDATATYPE_FLOAT64);
  }

  // request data to be updated when set
  result &= SimConnect_RequestClientData(hSimConnect, ClientData::SEC_ANALOG_OUTPUTS, ClientData::SEC_ANALOG_OUTPUTS,
                                         ClientData::SEC_ANALOG_OUTPUTS, SIMCONNECT_CLIENT_DATA_PERIOD_ON_SET);

  // ------------------------------------------------------------------------------------------------------------------

  for (int i = 0; i < 2; i++) {
    auto defineId = ClientData::SEC_1_BUS_OUTPUT + i;

    // map client id
    result &= SimConnect_MapClientDataNameToID(hSimConnect, ("A32NX_CLIENT_DATA_SEC_" + std::to_string(i + 1) + "_BUS").c_str(), defineId);
    // create client data
    result &= SimConnect_CreateClientData(hSimConnect, defineId, sizeof(base_sec_out_bus), SIMCONNECT_CREATE_CLIENT_DATA_FLAG_DEFAULT);
    // add data definitions
    for (int i = 0; i < 17; i++) {
      result &=
          SimConnect_AddToClientDataDefinition(hSimConnect, defineId, SIMCONNECT_CLIENTDATAOFFSET_AUTO, SIMCONNECT_CLIENTDATATYPE_FLOAT64);
    }

    // request data to be updated when set
    if (i == secDisabled) {
      result &= SimConnect_RequestClientData(hSimConnect, defineId, defineId, defineId, SIMCONNECT_CLIENT_DATA_PERIOD_ON_SET);
    }
  }

  // ------------------------------------------------------------------------------------------------------------------

  // map client id
  result &= SimConnect_MapClientDataNameToID(hSimConnect, "A32NX_CLIENT_DATA_FAC_DISCRETE_INPUT", ClientData::FAC_DISCRETE_INPUTS);
  // create client data
  result &= SimConnect_CreateClientData(hSimConnect, ClientData::FAC_DISCRETE_INPUTS, sizeof(base_fac_discrete_inputs),
                                        SIMCONNECT_CREATE_CLIENT_DATA_FLAG_DEFAULT);
  // add data definitions
  for (int i = 0; i < 22; i++) {
    result &= SimConnect_AddToClientDataDefinition(hSimConnect, ClientData::FAC_DISCRETE_INPUTS, SIMCONNECT_CLIENTDATAOFFSET_AUTO,
                                                   SIMCONNECT_CLIENTDATATYPE_INT8);
  }

  // ------------------------------------------------------------------------------------------------------------------

  // map client id
  result &= SimConnect_MapClientDataNameToID(hSimConnect, "A32NX_CLIENT_DATA_FAC_ANALOG_INPUT", ClientData::FAC_ANALOG_INPUTS);
  // create client data
  result &= SimConnect_CreateClientData(hSimConnect, ClientData::FAC_ANALOG_INPUTS, sizeof(base_fac_analog_inputs),
                                        SIMCONNECT_CREATE_CLIENT_DATA_FLAG_DEFAULT);
  // add data definitions
  for (int i = 0; i < 3; i++) {
    result &= SimConnect_AddToClientDataDefinition(hSimConnect, ClientData::FAC_ANALOG_INPUTS, SIMCONNECT_CLIENTDATAOFFSET_AUTO,
                                                   SIMCONNECT_CLIENTDATATYPE_FLOAT64);
  }

  // ------------------------------------------------------------------------------------------------------------------

  // map client id
  result &= SimConnect_MapClientDataNameToID(hSimConnect, "A32NX_CLIENT_DATA_FAC_DISCRETES_OUTPUT", ClientData::FAC_DISCRETE_OUTPUTS);
  // create client data
  result &= SimConnect_CreateClientData(hSimConnect, ClientData::FAC_DISCRETE_OUTPUTS, sizeof(base_fac_discrete_outputs),
                                        SIMCONNECT_CREATE_CLIENT_DATA_FLAG_DEFAULT);
  // add data definitions
  for (int i = 0; i < 6; i++) {
    result &= SimConnect_AddToClientDataDefinition(hSimConnect, ClientData::FAC_DISCRETE_OUTPUTS, SIMCONNECT_CLIENTDATAOFFSET_AUTO,
                                                   SIMCONNECT_CLIENTDATATYPE_INT8);
  }

  // request data to be updated when set
  result &= SimConnect_RequestClientData(hSimConnect, ClientData::FAC_DISCRETE_OUTPUTS, ClientData::FAC_DISCRETE_OUTPUTS,
                                         ClientData::FAC_DISCRETE_OUTPUTS, SIMCONNECT_CLIENT_DATA_PERIOD_ON_SET);

  // ------------------------------------------------------------------------------------------------------------------

  // map client id
  result &= SimConnect_MapClientDataNameToID(hSimConnect, "A32NX_CLIENT_DATA_FAC_ANALOGS_OUTPUT", ClientData::FAC_ANALOG_OUTPUTS);
  // create client data
  result &= SimConnect_CreateClientData(hSimConnect, ClientData::FAC_ANALOG_OUTPUTS, sizeof(base_fac_analog_outputs),
                                        SIMCONNECT_CREATE_CLIENT_DATA_FLAG_DEFAULT);
  // add data definitions
  for (int i = 0; i < 3; i++) {
    result &= SimConnect_AddToClientDataDefinition(hSimConnect, ClientData::FAC_ANALOG_OUTPUTS, SIMCONNECT_CLIENTDATAOFFSET_AUTO,
                                                   SIMCONNECT_CLIENTDATATYPE_FLOAT64);
  }

  // request data to be updated when set
  result &= SimConnect_RequestClientData(hSimConnect, ClientData::FAC_ANALOG_OUTPUTS, ClientData::FAC_ANALOG_OUTPUTS,
                                         ClientData::FAC_ANALOG_OUTPUTS, SIMCONNECT_CLIENT_DATA_PERIOD_ON_SET);

  // ------------------------------------------------------------------------------------------------------------------

  for (int i = 0; i < 2; i++) {
    auto defineId = ClientData::FAC_1_BUS_OUTPUT + i;

    // map client id
    result &= SimConnect_MapClientDataNameToID(hSimConnect, ("A32NX_CLIENT_DATA_FAC_" + std::to_string(i + 1) + "_BUS").c_str(), defineId);
    // create client data
    result &= SimConnect_CreateClientData(hSimConnect, defineId, sizeof(base_fac_bus), SIMCONNECT_CREATE_CLIENT_DATA_FLAG_DEFAULT);
    // add data definitions
    for (int i = 0; i < 28; i++) {
      result &=
          SimConnect_AddToClientDataDefinition(hSimConnect, defineId, SIMCONNECT_CLIENTDATAOFFSET_AUTO, SIMCONNECT_CLIENTDATATYPE_FLOAT64);
    }

    // request data to be updated when set
    if (i == facDisabled) {
      result &= SimConnect_RequestClientData(hSimConnect, defineId, defineId, defineId, SIMCONNECT_CLIENT_DATA_PERIOD_ON_SET);
    }
  }

  // ------------------------------------------------------------------------------------------------------------------

  // map client id
  result &= SimConnect_MapClientDataNameToID(hSimConnect, "A32NX_CLIENT_DATA_LOCAL_VARIABLES", ClientData::LOCAL_VARIABLES);
  // create client data
  result &= SimConnect_CreateClientData(hSimConnect, ClientData::LOCAL_VARIABLES, sizeof(ClientDataLocalVariables),
                                        SIMCONNECT_CREATE_CLIENT_DATA_FLAG_DEFAULT);
  // add data definitions
  result &= SimConnect_AddToClientDataDefinition(hSimConnect, ClientData::LOCAL_VARIABLES, SIMCONNECT_CLIENTDATAOFFSET_AUTO,
                                                 SIMCONNECT_CLIENTDATATYPE_FLOAT64);
  result &= SimConnect_AddToClientDataDefinition(hSimConnect, ClientData::LOCAL_VARIABLES, SIMCONNECT_CLIENTDATAOFFSET_AUTO,
                                                 SIMCONNECT_CLIENTDATATYPE_FLOAT64);
  result &= SimConnect_AddToClientDataDefinition(hSimConnect, ClientData::LOCAL_VARIABLES, SIMCONNECT_CLIENTDATAOFFSET_AUTO,
                                                 SIMCONNECT_CLIENTDATATYPE_FLOAT64);
  result &= SimConnect_AddToClientDataDefinition(hSimConnect, ClientData::LOCAL_VARIABLES, SIMCONNECT_CLIENTDATAOFFSET_AUTO,
                                                 SIMCONNECT_CLIENTDATATYPE_FLOAT64);
  result &= SimConnect_AddToClientDataDefinition(hSimConnect, ClientData::LOCAL_VARIABLES, SIMCONNECT_CLIENTDATAOFFSET_AUTO,
                                                 SIMCONNECT_CLIENTDATATYPE_FLOAT64);
  result &= SimConnect_AddToClientDataDefinition(hSimConnect, ClientData::LOCAL_VARIABLES, SIMCONNECT_CLIENTDATAOFFSET_AUTO,
                                                 SIMCONNECT_CLIENTDATATYPE_FLOAT64);
  result &= SimConnect_AddToClientDataDefinition(hSimConnect, ClientData::LOCAL_VARIABLES, SIMCONNECT_CLIENTDATAOFFSET_AUTO,
                                                 SIMCONNECT_CLIENTDATATYPE_FLOAT64);
  result &= SimConnect_AddToClientDataDefinition(hSimConnect, ClientData::LOCAL_VARIABLES, SIMCONNECT_CLIENTDATAOFFSET_AUTO,
                                                 SIMCONNECT_CLIENTDATATYPE_FLOAT64);
  result &= SimConnect_AddToClientDataDefinition(hSimConnect, ClientData::LOCAL_VARIABLES, SIMCONNECT_CLIENTDATAOFFSET_AUTO,
                                                 SIMCONNECT_CLIENTDATATYPE_FLOAT64);
  result &= SimConnect_AddToClientDataDefinition(hSimConnect, ClientData::LOCAL_VARIABLES, SIMCONNECT_CLIENTDATAOFFSET_AUTO,
                                                 SIMCONNECT_CLIENTDATATYPE_FLOAT64);
  result &= SimConnect_AddToClientDataDefinition(hSimConnect, ClientData::LOCAL_VARIABLES, SIMCONNECT_CLIENTDATAOFFSET_AUTO,
                                                 SIMCONNECT_CLIENTDATATYPE_FLOAT64);
  result &= SimConnect_AddToClientDataDefinition(hSimConnect, ClientData::LOCAL_VARIABLES, SIMCONNECT_CLIENTDATAOFFSET_AUTO,
                                                 SIMCONNECT_CLIENTDATATYPE_FLOAT64);
  result &= SimConnect_AddToClientDataDefinition(hSimConnect, ClientData::LOCAL_VARIABLES, SIMCONNECT_CLIENTDATAOFFSET_AUTO,
                                                 SIMCONNECT_CLIENTDATATYPE_FLOAT64);
  result &= SimConnect_AddToClientDataDefinition(hSimConnect, ClientData::LOCAL_VARIABLES, SIMCONNECT_CLIENTDATAOFFSET_AUTO,
                                                 SIMCONNECT_CLIENTDATATYPE_FLOAT64);
  result &= SimConnect_AddToClientDataDefinition(hSimConnect, ClientData::LOCAL_VARIABLES, SIMCONNECT_CLIENTDATAOFFSET_AUTO,
                                                 SIMCONNECT_CLIENTDATATYPE_FLOAT64);
  result &= SimConnect_AddToClientDataDefinition(hSimConnect, ClientData::LOCAL_VARIABLES, SIMCONNECT_CLIENTDATAOFFSET_AUTO,
                                                 SIMCONNECT_CLIENTDATATYPE_FLOAT64);
  result &= SimConnect_AddToClientDataDefinition(hSimConnect, ClientData::LOCAL_VARIABLES, SIMCONNECT_CLIENTDATAOFFSET_AUTO,
                                                 SIMCONNECT_CLIENTDATATYPE_FLOAT64);
  result &= SimConnect_AddToClientDataDefinition(hSimConnect, ClientData::LOCAL_VARIABLES, SIMCONNECT_CLIENTDATAOFFSET_AUTO,
                                                 SIMCONNECT_CLIENTDATATYPE_FLOAT64);
  result &= SimConnect_AddToClientDataDefinition(hSimConnect, ClientData::LOCAL_VARIABLES, SIMCONNECT_CLIENTDATAOFFSET_AUTO,
                                                 SIMCONNECT_CLIENTDATATYPE_FLOAT64);
  result &= SimConnect_AddToClientDataDefinition(hSimConnect, ClientData::LOCAL_VARIABLES, SIMCONNECT_CLIENTDATAOFFSET_AUTO,
                                                 SIMCONNECT_CLIENTDATATYPE_FLOAT64);
  result &= SimConnect_AddToClientDataDefinition(hSimConnect, ClientData::LOCAL_VARIABLES, SIMCONNECT_CLIENTDATAOFFSET_AUTO,
                                                 SIMCONNECT_CLIENTDATATYPE_FLOAT64);
  result &= SimConnect_AddToClientDataDefinition(hSimConnect, ClientData::LOCAL_VARIABLES, SIMCONNECT_CLIENTDATAOFFSET_AUTO,
                                                 SIMCONNECT_CLIENTDATATYPE_FLOAT64);
  result &= SimConnect_AddToClientDataDefinition(hSimConnect, ClientData::LOCAL_VARIABLES, SIMCONNECT_CLIENTDATAOFFSET_AUTO,
                                                 SIMCONNECT_CLIENTDATATYPE_FLOAT64);
  result &= SimConnect_AddToClientDataDefinition(hSimConnect, ClientData::LOCAL_VARIABLES, SIMCONNECT_CLIENTDATAOFFSET_AUTO,
                                                 SIMCONNECT_CLIENTDATATYPE_INT64);
  result &= SimConnect_AddToClientDataDefinition(hSimConnect, ClientData::LOCAL_VARIABLES, SIMCONNECT_CLIENTDATAOFFSET_AUTO,
                                                 SIMCONNECT_CLIENTDATATYPE_FLOAT64);
  result &= SimConnect_AddToClientDataDefinition(hSimConnect, ClientData::LOCAL_VARIABLES, SIMCONNECT_CLIENTDATAOFFSET_AUTO,
                                                 SIMCONNECT_CLIENTDATATYPE_FLOAT64);
  result &= SimConnect_AddToClientDataDefinition(hSimConnect, ClientData::LOCAL_VARIABLES, SIMCONNECT_CLIENTDATAOFFSET_AUTO,
                                                 SIMCONNECT_CLIENTDATATYPE_INT64);
  result &= SimConnect_AddToClientDataDefinition(hSimConnect, ClientData::LOCAL_VARIABLES, SIMCONNECT_CLIENTDATAOFFSET_AUTO,
                                                 SIMCONNECT_CLIENTDATATYPE_INT64);
  result &= SimConnect_AddToClientDataDefinition(hSimConnect, ClientData::LOCAL_VARIABLES, SIMCONNECT_CLIENTDATAOFFSET_AUTO,
                                                 SIMCONNECT_CLIENTDATATYPE_FLOAT64);
  result &= SimConnect_AddToClientDataDefinition(hSimConnect, ClientData::LOCAL_VARIABLES, SIMCONNECT_CLIENTDATAOFFSET_AUTO,
                                                 SIMCONNECT_CLIENTDATATYPE_FLOAT64);
  result &= SimConnect_AddToClientDataDefinition(hSimConnect, ClientData::LOCAL_VARIABLES, SIMCONNECT_CLIENTDATAOFFSET_AUTO,
                                                 SIMCONNECT_CLIENTDATATYPE_INT64);
  result &= SimConnect_AddToClientDataDefinition(hSimConnect, ClientData::LOCAL_VARIABLES, SIMCONNECT_CLIENTDATAOFFSET_AUTO,
                                                 SIMCONNECT_CLIENTDATATYPE_INT64);
  result &= SimConnect_AddToClientDataDefinition(hSimConnect, ClientData::LOCAL_VARIABLES, SIMCONNECT_CLIENTDATAOFFSET_AUTO,
                                                 SIMCONNECT_CLIENTDATATYPE_FLOAT64);
  result &= SimConnect_AddToClientDataDefinition(hSimConnect, ClientData::LOCAL_VARIABLES, SIMCONNECT_CLIENTDATAOFFSET_AUTO,
                                                 SIMCONNECT_CLIENTDATATYPE_FLOAT64);
  result &= SimConnect_AddToClientDataDefinition(hSimConnect, ClientData::LOCAL_VARIABLES, SIMCONNECT_CLIENTDATAOFFSET_AUTO,
                                                 SIMCONNECT_CLIENTDATATYPE_FLOAT64);
  result &= SimConnect_AddToClientDataDefinition(hSimConnect, ClientData::LOCAL_VARIABLES, SIMCONNECT_CLIENTDATAOFFSET_AUTO,
                                                 SIMCONNECT_CLIENTDATATYPE_INT64);

  // ------------------------------------------------------------------------------------------------------------------

  // map client id
  result &=
      SimConnect_MapClientDataNameToID(hSimConnect, "A32NX_CLIENT_DATA_LOCAL_VARIABLES_AUTOTHRUST", ClientData::LOCAL_VARIABLES_AUTOTHRUST);
  // create client
  result &= SimConnect_CreateClientData(hSimConnect, ClientData::LOCAL_VARIABLES_AUTOTHRUST, sizeof(ClientDataLocalVariablesAutothrust),
                                        SIMCONNECT_CREATE_CLIENT_DATA_FLAG_DEFAULT);
  // add data definitions
  result &= SimConnect_AddToClientDataDefinition(hSimConnect, ClientData::LOCAL_VARIABLES_AUTOTHRUST, SIMCONNECT_CLIENTDATAOFFSET_AUTO,
                                                 SIMCONNECT_CLIENTDATATYPE_FLOAT64);
  result &= SimConnect_AddToClientDataDefinition(hSimConnect, ClientData::LOCAL_VARIABLES_AUTOTHRUST, SIMCONNECT_CLIENTDATAOFFSET_AUTO,
                                                 SIMCONNECT_CLIENTDATATYPE_FLOAT64);
  result &= SimConnect_AddToClientDataDefinition(hSimConnect, ClientData::LOCAL_VARIABLES_AUTOTHRUST, SIMCONNECT_CLIENTDATAOFFSET_AUTO,
                                                 SIMCONNECT_CLIENTDATATYPE_FLOAT64);
  result &= SimConnect_AddToClientDataDefinition(hSimConnect, ClientData::LOCAL_VARIABLES_AUTOTHRUST, SIMCONNECT_CLIENTDATAOFFSET_AUTO,
                                                 SIMCONNECT_CLIENTDATATYPE_FLOAT64);
  result &= SimConnect_AddToClientDataDefinition(hSimConnect, ClientData::LOCAL_VARIABLES_AUTOTHRUST, SIMCONNECT_CLIENTDATAOFFSET_AUTO,
                                                 SIMCONNECT_CLIENTDATATYPE_FLOAT64);
  result &= SimConnect_AddToClientDataDefinition(hSimConnect, ClientData::LOCAL_VARIABLES_AUTOTHRUST, SIMCONNECT_CLIENTDATAOFFSET_AUTO,
                                                 SIMCONNECT_CLIENTDATATYPE_FLOAT64);
  result &= SimConnect_AddToClientDataDefinition(hSimConnect, ClientData::LOCAL_VARIABLES_AUTOTHRUST, SIMCONNECT_CLIENTDATAOFFSET_AUTO,
                                                 SIMCONNECT_CLIENTDATATYPE_FLOAT64);
  result &= SimConnect_AddToClientDataDefinition(hSimConnect, ClientData::LOCAL_VARIABLES_AUTOTHRUST, SIMCONNECT_CLIENTDATAOFFSET_AUTO,
                                                 SIMCONNECT_CLIENTDATATYPE_FLOAT64);
  result &= SimConnect_AddToClientDataDefinition(hSimConnect, ClientData::LOCAL_VARIABLES_AUTOTHRUST, SIMCONNECT_CLIENTDATAOFFSET_AUTO,
                                                 SIMCONNECT_CLIENTDATATYPE_FLOAT64);
  result &= SimConnect_AddToClientDataDefinition(hSimConnect, ClientData::LOCAL_VARIABLES_AUTOTHRUST, SIMCONNECT_CLIENTDATAOFFSET_AUTO,
                                                 SIMCONNECT_CLIENTDATATYPE_FLOAT64);
  result &= SimConnect_AddToClientDataDefinition(hSimConnect, ClientData::LOCAL_VARIABLES_AUTOTHRUST, SIMCONNECT_CLIENTDATAOFFSET_AUTO,
                                                 SIMCONNECT_CLIENTDATATYPE_FLOAT64);
  result &= SimConnect_AddToClientDataDefinition(hSimConnect, ClientData::LOCAL_VARIABLES_AUTOTHRUST, SIMCONNECT_CLIENTDATAOFFSET_AUTO,
                                                 SIMCONNECT_CLIENTDATATYPE_FLOAT64);
  result &= SimConnect_AddToClientDataDefinition(hSimConnect, ClientData::LOCAL_VARIABLES_AUTOTHRUST, SIMCONNECT_CLIENTDATAOFFSET_AUTO,
                                                 SIMCONNECT_CLIENTDATATYPE_FLOAT64);
  result &= SimConnect_AddToClientDataDefinition(hSimConnect, ClientData::LOCAL_VARIABLES_AUTOTHRUST, SIMCONNECT_CLIENTDATAOFFSET_AUTO,
                                                 SIMCONNECT_CLIENTDATATYPE_FLOAT64);
  result &= SimConnect_AddToClientDataDefinition(hSimConnect, ClientData::LOCAL_VARIABLES_AUTOTHRUST, SIMCONNECT_CLIENTDATAOFFSET_AUTO,
                                                 SIMCONNECT_CLIENTDATATYPE_FLOAT64);
  result &= SimConnect_AddToClientDataDefinition(hSimConnect, ClientData::LOCAL_VARIABLES_AUTOTHRUST, SIMCONNECT_CLIENTDATAOFFSET_AUTO,
                                                 SIMCONNECT_CLIENTDATATYPE_FLOAT64);
  result &= SimConnect_AddToClientDataDefinition(hSimConnect, ClientData::LOCAL_VARIABLES_AUTOTHRUST, SIMCONNECT_CLIENTDATAOFFSET_AUTO,
                                                 SIMCONNECT_CLIENTDATATYPE_FLOAT64);
  result &= SimConnect_AddToClientDataDefinition(hSimConnect, ClientData::LOCAL_VARIABLES_AUTOTHRUST, SIMCONNECT_CLIENTDATAOFFSET_AUTO,
                                                 SIMCONNECT_CLIENTDATATYPE_FLOAT64);
  result &= SimConnect_AddToClientDataDefinition(hSimConnect, ClientData::LOCAL_VARIABLES_AUTOTHRUST, SIMCONNECT_CLIENTDATAOFFSET_AUTO,
                                                 SIMCONNECT_CLIENTDATATYPE_FLOAT64);
  result &= SimConnect_AddToClientDataDefinition(hSimConnect, ClientData::LOCAL_VARIABLES_AUTOTHRUST, SIMCONNECT_CLIENTDATAOFFSET_AUTO,
                                                 SIMCONNECT_CLIENTDATATYPE_FLOAT64);
  result &= SimConnect_AddToClientDataDefinition(hSimConnect, ClientData::LOCAL_VARIABLES_AUTOTHRUST, SIMCONNECT_CLIENTDATAOFFSET_AUTO,
                                                 SIMCONNECT_CLIENTDATATYPE_FLOAT64);
  result &= SimConnect_AddToClientDataDefinition(hSimConnect, ClientData::LOCAL_VARIABLES_AUTOTHRUST, SIMCONNECT_CLIENTDATAOFFSET_AUTO,
                                                 SIMCONNECT_CLIENTDATATYPE_FLOAT64);
  result &= SimConnect_AddToClientDataDefinition(hSimConnect, ClientData::LOCAL_VARIABLES_AUTOTHRUST, SIMCONNECT_CLIENTDATAOFFSET_AUTO,
                                                 SIMCONNECT_CLIENTDATATYPE_FLOAT64);
  result &= SimConnect_AddToClientDataDefinition(hSimConnect, ClientData::LOCAL_VARIABLES_AUTOTHRUST, SIMCONNECT_CLIENTDATAOFFSET_AUTO,
                                                 SIMCONNECT_CLIENTDATATYPE_FLOAT64);
  result &= SimConnect_AddToClientDataDefinition(hSimConnect, ClientData::LOCAL_VARIABLES_AUTOTHRUST, SIMCONNECT_CLIENTDATAOFFSET_AUTO,
                                                 SIMCONNECT_CLIENTDATATYPE_FLOAT64);
  result &= SimConnect_AddToClientDataDefinition(hSimConnect, ClientData::LOCAL_VARIABLES_AUTOTHRUST, SIMCONNECT_CLIENTDATAOFFSET_AUTO,
                                                 SIMCONNECT_CLIENTDATATYPE_FLOAT64);
  result &= SimConnect_AddToClientDataDefinition(hSimConnect, ClientData::LOCAL_VARIABLES_AUTOTHRUST, SIMCONNECT_CLIENTDATAOFFSET_AUTO,
                                                 SIMCONNECT_CLIENTDATATYPE_FLOAT64);

  // ------------------------------------------------------------------------------------------------------------------

  // return result
  return SUCCEEDED(result);
}

bool SimConnectInterface::requestData() {
  // check if we are connected
  if (!isConnected) {
    return false;
  }

  // request data
  HRESULT result = SimConnect_RequestDataOnSimObject(hSimConnect, 0, 0, SIMCONNECT_OBJECT_ID_USER, SIMCONNECT_PERIOD_VISUAL_FRAME);

  // check result of data request
  if (result != S_OK) {
    // request failed
    return false;
  }

  // success
  return true;
}

bool SimConnectInterface::readData() {
  // check if we are connected
  if (!isConnected) {
    return false;
  }

  // get next dispatch message(s) and process them
  DWORD cbData;
  SIMCONNECT_RECV* pData;
  while (SUCCEEDED(SimConnect_GetNextDispatch(hSimConnect, &pData, &cbData))) {
    simConnectProcessDispatchMessage(pData, &cbData);
  }

  // success
  return true;
}

bool SimConnectInterface::sendData(SimOutputZetaTrim output) {
  // write data and return result
  return sendData(3, sizeof(output), &output);
}

bool SimConnectInterface::sendData(SimOutputThrottles output) {
  // write data and return result
  return sendData(4, sizeof(output), &output);
}

bool SimConnectInterface::sendData(SimOutputSpoilers output) {
  // write data and return result
  return sendData(6, sizeof(output), &output);
}

bool SimConnectInterface::sendData(SimOutputAltimeter output) {
  // write data and return result
  return sendData(7, sizeof(output), &output);
}

bool SimConnectInterface::sendEvent(Events eventId) {
  return sendEvent(eventId, 0, SIMCONNECT_GROUP_PRIORITY_HIGHEST);
}

bool SimConnectInterface::sendEvent(Events eventId, DWORD data) {
  return sendEvent(eventId, data, SIMCONNECT_GROUP_PRIORITY_HIGHEST);
}

bool SimConnectInterface::sendEvent(Events eventId, DWORD data, DWORD priority) {
  // check if we are connected
  if (!isConnected) {
    return false;
  }

  // send event
  HRESULT result = SimConnect_TransmitClientEvent(hSimConnect, 0, eventId, data, priority, SIMCONNECT_EVENT_FLAG_GROUPID_IS_PRIORITY);

  // check result of data request
  if (result != S_OK) {
    // request failed
    return false;
  }

  // success
  return true;
}

bool SimConnectInterface::setClientDataLocalVariables(ClientDataLocalVariables output) {
  // write data and return result
  return sendClientData(ClientData::LOCAL_VARIABLES, sizeof(output), &output);
}

bool SimConnectInterface::setClientDataLocalVariablesAutothrust(ClientDataLocalVariablesAutothrust output) {
  // write data and return result
  return sendClientData(ClientData::LOCAL_VARIABLES_AUTOTHRUST, sizeof(output), &output);
}

SimData SimConnectInterface::getSimData() {
  return simData;
}

SimInput SimConnectInterface::getSimInput() {
  return simInput;
}

SimInputAutopilot SimConnectInterface::getSimInputAutopilot() {
  return simInputAutopilot;
}

SimInputRudderTrim SimConnectInterface::getSimInputRudderTrim() {
  return simInputRudderTrim;
}

SimInputThrottles SimConnectInterface::getSimInputThrottles() {
  return simInputThrottles;
}

void SimConnectInterface::resetSimInputAutopilot() {
  simInputAutopilot.AP_engage = 0;
  simInputAutopilot.AP_1_push = 0;
  simInputAutopilot.AP_2_push = 0;
  simInputAutopilot.AP_disconnect = 0;
  simInputAutopilot.HDG_push = 0;
  simInputAutopilot.HDG_pull = 0;
  simInputAutopilot.ALT_push = 0;
  simInputAutopilot.ALT_pull = 0;
  simInputAutopilot.VS_push = 0;
  simInputAutopilot.VS_pull = 0;
  simInputAutopilot.LOC_push = 0;
  simInputAutopilot.APPR_push = 0;
  simInputAutopilot.EXPED_push = 0;
  simInputAutopilot.DIR_TO_trigger = 0;
}

void SimConnectInterface::resetSimInputRudderTrim() {
  simInputRudderTrim.rudderTrimSwitchLeft = false;
  simInputRudderTrim.rudderTrimSwitchRight = false;
  simInputRudderTrim.rudderTrimReset = false;
}

void SimConnectInterface::resetSimInputThrottles() {
  simInputThrottles.ATHR_push = 0;
  simInputThrottles.ATHR_disconnect = 0;
  simInputThrottles.ATHR_reset_disable = 0;
}

bool SimConnectInterface::setClientDataAutopilotLaws(ClientDataAutopilotLaws output) {
  // write data and return result
  return sendClientData(ClientData::AUTOPILOT_LAWS, sizeof(output), &output);
}

ClientDataAutopilotLaws SimConnectInterface::getClientDataAutopilotLaws() {
  return clientDataAutopilotLaws;
}

bool SimConnectInterface::setClientDataAutopilotStateMachine(ClientDataAutopilotStateMachine output) {
  // write data and return result
  return sendClientData(ClientData::AUTOPILOT_STATE_MACHINE, sizeof(output), &output);
}

ClientDataAutopilotStateMachine SimConnectInterface::getClientDataAutopilotStateMachine() {
  return clientDataAutopilotStateMachine;
}

ClientDataAutothrust SimConnectInterface::getClientDataAutothrust() {
  return clientDataAutothrust;
}

ClientDataFlyByWire SimConnectInterface::getClientDataFlyByWire() {
  return clientDataFlyByWire;
}

bool SimConnectInterface::setClientDataElacDiscretes(base_elac_discrete_inputs output) {
  return sendClientData(ClientData::ELAC_DISCRETE_INPUTS, sizeof(output), &output);
}

bool SimConnectInterface::setClientDataElacAnalog(base_elac_analog_inputs output) {
  return sendClientData(ClientData::ELAC_ANALOG_INPUTS, sizeof(output), &output);
}

bool SimConnectInterface::setClientDataElacBusInput(base_elac_out_bus output, int elacIndex) {
  return sendClientData(ClientData::ELAC_1_BUS_OUTPUT + elacIndex, sizeof(output), &output);
}

bool SimConnectInterface::setClientDataSecDiscretes(base_sec_discrete_inputs output) {
  return sendClientData(ClientData::SEC_DISCRETE_INPUTS, sizeof(output), &output);
}

bool SimConnectInterface::setClientDataSecAnalog(base_sec_analog_inputs output) {
  return sendClientData(ClientData::SEC_ANALOG_INPUTS, sizeof(output), &output);
}

bool SimConnectInterface::setClientDataSecBus(base_sec_out_bus output, int secIndex) {
  if (secIndex < 2) {
    return sendClientData(ClientData::SEC_1_BUS_OUTPUT + secIndex, sizeof(output), &output);
  } else {
    return false;
  }
}

bool SimConnectInterface::setClientDataFacDiscretes(base_fac_discrete_inputs output) {
  return sendClientData(ClientData::FAC_DISCRETE_INPUTS, sizeof(output), &output);
}

bool SimConnectInterface::setClientDataFacAnalog(base_fac_analog_inputs output) {
  return sendClientData(ClientData::FAC_ANALOG_INPUTS, sizeof(output), &output);
}

bool SimConnectInterface::setClientDataFacBus(base_fac_bus output, int facIndex) {
  return sendClientData(ClientData::FAC_1_BUS_OUTPUT + facIndex, sizeof(output), &output);
}

base_elac_discrete_outputs SimConnectInterface::getClientDataElacDiscretesOutput() {
  return clientDataElacDiscreteOutputs;
}

base_elac_analog_outputs SimConnectInterface::getClientDataElacAnalogsOutput() {
  return clientDataElacAnalogOutputs;
}

base_elac_out_bus SimConnectInterface::getClientDataElacBusOutput() {
  return clientDataElacBusOutputs;
}

base_sec_discrete_outputs SimConnectInterface::getClientDataSecDiscretesOutput() {
  return clientDataSecDiscreteOutputs;
}

base_sec_analog_outputs SimConnectInterface::getClientDataSecAnalogsOutput() {
  return clientDataSecAnalogOutputs;
}

base_sec_out_bus SimConnectInterface::getClientDataSecBusOutput() {
  return clientDataSecBusOutputs;
}

base_fac_discrete_outputs SimConnectInterface::getClientDataFacDiscretesOutput() {
  return clientDataFacDiscreteOutputs;
}

base_fac_analog_outputs SimConnectInterface::getClientDataFacAnalogsOutput() {
  return clientDataFacAnalogOutputs;
}

base_fac_bus SimConnectInterface::getClientDataFacBusOutput() {
  return clientDataFacBusOutputs;
}

bool SimConnectInterface::setClientDataAdr(base_adr_bus output, int adrIndex) {
  return sendClientData(ClientData::ADR_1_INPUTS + adrIndex, sizeof(output), &output);
}

bool SimConnectInterface::setClientDataIr(base_ir_bus output, int irIndex) {
  return sendClientData(ClientData::IR_1_INPUTS + irIndex, sizeof(output), &output);
}

bool SimConnectInterface::setClientDataRa(base_ra_bus output, int raIndex) {
  return sendClientData(ClientData::RA_1_BUS + raIndex, sizeof(output), &output);
}

bool SimConnectInterface::setClientDataLgciu(base_lgciu_bus output, int lgciuIndex) {
  return sendClientData(ClientData::LGCIU_1_BUS + lgciuIndex, sizeof(output), &output);
}

bool SimConnectInterface::setClientDataSfcc(base_sfcc_bus output, int sfccIndex) {
  return sendClientData(ClientData::SFCC_1_BUS + sfccIndex, sizeof(output), &output);
}

bool SimConnectInterface::setClientDataFmgcB(base_fmgc_b_bus output, int fmgcIndex) {
  return sendClientData(ClientData::FMGC_1_B_BUS + fmgcIndex, sizeof(output), &output);
}

void SimConnectInterface::setLoggingFlightControlsEnabled(bool enabled) {
  loggingFlightControlsEnabled = enabled;
}

bool SimConnectInterface::getLoggingFlightControlsEnabled() {
  return loggingFlightControlsEnabled;
}

void SimConnectInterface::setLoggingThrottlesEnabled(bool enabled) {
  loggingThrottlesEnabled = enabled;
}

bool SimConnectInterface::getLoggingThrottlesEnabled() {
  return loggingThrottlesEnabled;
}

// remove when aileron events can be processed via SimConnect (which also allows to mask the events)
void SimConnectInterface::processKeyEvent(ID32 event,
                                          UINT32 evdata0,
                                          UINT32 evdata1,
                                          UINT32 evdata2,
                                          UINT32 evdata3,
                                          UINT32 evdata4,
                                          PVOID userdata) {
  switch (event) {
    case KEY_AILERON_LEFT: {
      simInput.inputs[AXIS_AILERONS_SET] = std::fmin(1.0, simInput.inputs[AXIS_AILERONS_SET] + flightControlsKeyChangeAileron);
      if (loggingFlightControlsEnabled) {
        std::cout << "WASM: AILERONS_LEFT: ";
        std::cout << "(no data)";
        std::cout << " -> ";
        std::cout << simInput.inputs[AXIS_AILERONS_SET];
        std::cout << std::endl;
      }
      break;
    }
    case KEY_AILERON_RIGHT: {
      simInput.inputs[AXIS_AILERONS_SET] = std::fmax(-1.0, simInput.inputs[AXIS_AILERONS_SET] - flightControlsKeyChangeAileron);
      if (loggingFlightControlsEnabled) {
        std::cout << "WASM: AILERONS_RIGHT: ";
        std::cout << "(no data)";
        std::cout << " -> ";
        std::cout << simInput.inputs[AXIS_AILERONS_SET];
        std::cout << std::endl;
      }
      break;
    }
    default: {
      return;
    }
  }
}

void SimConnectInterface::simConnectProcessDispatchMessage(SIMCONNECT_RECV* pData, DWORD* cbData) {
  switch (pData->dwID) {
    case SIMCONNECT_RECV_ID_OPEN:
      // connection established
      std::cout << "WASM: SimConnect connection established" << std::endl;
      break;

    case SIMCONNECT_RECV_ID_QUIT:
      // connection lost
      std::cout << "WASM: Received SimConnect connection quit message" << std::endl;
      disconnect();
      break;

    case SIMCONNECT_RECV_ID_EVENT:
      simConnectProcessEvent(static_cast<SIMCONNECT_RECV_EVENT*>(pData));
      break;

    case SIMCONNECT_RECV_ID_EVENT_EX1:
      simConnectProcessEvent_EX1(static_cast<SIMCONNECT_RECV_EVENT_EX1*>(pData));
      break;

    case SIMCONNECT_RECV_ID_SIMOBJECT_DATA:
      // process data
      simConnectProcessSimObjectData(static_cast<SIMCONNECT_RECV_SIMOBJECT_DATA*>(pData));
      break;

    case SIMCONNECT_RECV_ID_CLIENT_DATA:
      // process data
      simConnectProcessClientData(static_cast<SIMCONNECT_RECV_CLIENT_DATA*>(pData));
      break;

    case SIMCONNECT_RECV_ID_EXCEPTION:
      // exception
      std::cout << "WASM: Exception in SimConnect connection: ";
      std::cout << getSimConnectExceptionString(
          static_cast<SIMCONNECT_EXCEPTION>(static_cast<SIMCONNECT_RECV_EXCEPTION*>(pData)->dwException));
      std::cout << std::endl;
      break;

    default:
      break;
  }
}

void SimConnectInterface::simConnectProcessEvent(const SIMCONNECT_RECV_EVENT* event) {
  const DWORD eventId = event->uEventID;
  const DWORD data0 = event->dwData;
  processEventWithOneParam(eventId, data0);
}

void SimConnectInterface::simConnectProcessEvent_EX1(const SIMCONNECT_RECV_EVENT_EX1* event) {
  const DWORD eventId = event->uEventID;
  const DWORD data0 = event->dwData0;
  processEventWithOneParam(eventId, data0);
}

void SimConnectInterface::processEventWithOneParam(const DWORD eventId, const DWORD data0) {
  // process depending on event id
  switch (eventId) {
    case AXIS_ELEVATOR_SET: {
      simInput.inputs[AXIS_ELEVATOR_SET] = static_cast<long>(data0) / 16384.0;
      if (loggingFlightControlsEnabled) {
        std::cout << "WASM: AXIS_ELEVATOR_SET: ";
        std::cout << static_cast<long>(data0);
        std::cout << " -> ";
        std::cout << simInput.inputs[AXIS_ELEVATOR_SET];
        std::cout << std::endl;
      }
      break;
    }

    case AXIS_AILERONS_SET: {
      simInput.inputs[AXIS_AILERONS_SET] = static_cast<long>(data0) / 16384.0;
      if (loggingFlightControlsEnabled) {
        std::cout << "WASM: AXIS_AILERONS_SET: ";
        std::cout << static_cast<long>(data0);
        std::cout << " -> ";
        std::cout << simInput.inputs[AXIS_AILERONS_SET];
        std::cout << std::endl;
      }
      break;
    }

    case AXIS_RUDDER_SET: {
      simInput.inputs[AXIS_RUDDER_SET] = static_cast<long>(data0) / 16384.0;
      if (loggingFlightControlsEnabled) {
        std::cout << "WASM: AXIS_RUDDER_SET: ";
        std::cout << static_cast<long>(data0);
        std::cout << " -> ";
        std::cout << simInput.inputs[AXIS_RUDDER_SET];
        std::cout << std::endl;
      }
      break;
    }

    case RUDDER_SET: {
      simInput.inputs[AXIS_RUDDER_SET] = static_cast<long>(data0) / 16384.0;
      if (loggingFlightControlsEnabled) {
        std::cout << "WASM: RUDDER_SET: ";
        std::cout << static_cast<long>(data0);
        std::cout << " -> ";
        std::cout << simInput.inputs[AXIS_RUDDER_SET];
        std::cout << std::endl;
      }
      break;
    }

    case RUDDER_LEFT: {
      simInput.inputs[AXIS_RUDDER_SET] = fmin(1.0, simInput.inputs[AXIS_RUDDER_SET] + flightControlsKeyChangeRudder);
      if (loggingFlightControlsEnabled) {
        std::cout << "WASM: RUDDER_LEFT: ";
        std::cout << "(no data)";
        std::cout << " -> ";
        std::cout << simInput.inputs[AXIS_RUDDER_SET];
        std::cout << std::endl;
      }
      break;
    }

    case RUDDER_CENTER: {
      simInput.inputs[AXIS_RUDDER_SET] = 0.0;
      if (loggingFlightControlsEnabled) {
        std::cout << "WASM: RUDDER_CENTER: ";
        std::cout << "(no data)";
        std::cout << " -> ";
        std::cout << simInput.inputs[AXIS_RUDDER_SET];
        std::cout << std::endl;
      }
      break;
    }

    case RUDDER_RIGHT: {
      simInput.inputs[AXIS_RUDDER_SET] = fmax(-1.0, simInput.inputs[AXIS_RUDDER_SET] - flightControlsKeyChangeRudder);
      if (loggingFlightControlsEnabled) {
        std::cout << "WASM: RUDDER_RIGHT: ";
        std::cout << "(no data)";
        std::cout << " -> ";
        std::cout << simInput.inputs[AXIS_RUDDER_SET];
        std::cout << std::endl;
      }
      break;
    }

    case RUDDER_AXIS_MINUS: {
      double tmpValue = 0;
      if (disableXboxCompatibilityRudderPlusMinus) {
        // normal axis
        tmpValue = +1.0 * ((static_cast<long>(data0) + 16384.0) / 32768.0);
      } else {
        // xbox controller
        tmpValue = +1.0 * (static_cast<long>(data0) / 16384.0);
      }

      // This allows using two independent axis for rudder which are mapped to RUDDER AXIS LEFT and RUDDER AXIS RIGHT
      // As it might be incompatible with some controllers, it is configurable
      if (enableRudder2AxisMode) {
        rudderLeftAxis = tmpValue;
        tmpValue = -1 * ((rudderRightAxis - rudderLeftAxis) / 2.0);
      }

      simInput.inputs[AXIS_RUDDER_SET] = tmpValue;
      if (loggingFlightControlsEnabled) {
        std::cout << "WASM: RUDDER_AXIS_MINUS: ";
        std::cout << static_cast<long>(data0);
        std::cout << " -> ";
        std::cout << simInput.inputs[AXIS_RUDDER_SET];
        if (enableRudder2AxisMode) {
          std::cout << " (left: " << rudderLeftAxis << ", right: " << rudderRightAxis << ")";
        }
        std::cout << std::endl;
      }
      break;
    }

    case RUDDER_AXIS_PLUS: {
      double tmpValue = 0;
      if (disableXboxCompatibilityRudderPlusMinus) {
        // normal axis
        tmpValue = -1.0 * ((static_cast<long>(data0) + 16384.0) / 32768.0);
      } else {
        // xbox controller
        tmpValue = -1.0 * (static_cast<long>(data0) / 16384.0);
      }

      // This allows using two independent axis for rudder which are mapped to RUDDER AXIS LEFT and RUDDER AXIS RIGHT
      // As it might be incompatible with some controllers, it is configurable
      if (enableRudder2AxisMode) {
        rudderRightAxis = -tmpValue;
        tmpValue = -1 * ((rudderRightAxis - rudderLeftAxis) / 2.0);
      }

      simInput.inputs[AXIS_RUDDER_SET] = tmpValue;
      if (loggingFlightControlsEnabled) {
        std::cout << "WASM: RUDDER_AXIS_PLUS: ";
        std::cout << static_cast<long>(data0);
        std::cout << " -> ";
        std::cout << simInput.inputs[AXIS_RUDDER_SET];
        if (enableRudder2AxisMode) {
          std::cout << " (left: " << rudderLeftAxis << ", right: " << rudderRightAxis << ")";
        }
        std::cout << std::endl;
      }
      break;
    }

    case RUDDER_TRIM_LEFT: {
      simInputRudderTrim.rudderTrimSwitchLeft = true;
      if (loggingFlightControlsEnabled) {
        std::cout << "WASM: RUDDER_TRIM_LEFT: ";
        std::cout << "(no data)";
        std::cout << std::endl;
      }
      break;
    }

    case RUDDER_TRIM_RESET: {
      simInputRudderTrim.rudderTrimReset = true;
      if (loggingFlightControlsEnabled) {
        std::cout << "WASM: RUDDER_TRIM_RESET: ";
        std::cout << "(no data)";
        std::cout << std::endl;
      }
      break;
    }

    case RUDDER_TRIM_RIGHT: {
      simInputRudderTrim.rudderTrimSwitchRight = true;
      if (loggingFlightControlsEnabled) {
        std::cout << "WASM: RUDDER_TRIM_RIGHT: ";
        std::cout << "(no data)";
        std::cout << std::endl;
      }
      break;
    }

    case RUDDER_TRIM_SET: {
      if (loggingFlightControlsEnabled) {
        std::cout << "WASM: RUDDER_TRIM_SET: ";
        std::cout << static_cast<long>(data0);
        std::cout << std::endl;
      }
      break;
    }

    case RUDDER_TRIM_SET_EX1: {
      if (loggingFlightControlsEnabled) {
        std::cout << "WASM: RUDDER_TRIM_SET_EX1: ";
        std::cout << static_cast<long>(data0);
        std::cout << std::endl;
      }
      break;
    }

    case AILERON_SET: {
      simInput.inputs[AXIS_AILERONS_SET] = static_cast<long>(data0) / 16384.0;
      if (loggingFlightControlsEnabled) {
        std::cout << "WASM: AILERON_SET: ";
        std::cout << static_cast<long>(data0);
        std::cout << " -> ";
        std::cout << simInput.inputs[AXIS_AILERONS_SET];
        std::cout << std::endl;
      }
      break;
    }

    case AILERONS_LEFT: {
      simInput.inputs[AXIS_AILERONS_SET] = fmin(1.0, simInput.inputs[AXIS_AILERONS_SET] + flightControlsKeyChangeAileron);
      if (loggingFlightControlsEnabled) {
        std::cout << "WASM: AILERONS_LEFT: ";
        std::cout << "(no data)";
        std::cout << " -> ";
        std::cout << simInput.inputs[AXIS_AILERONS_SET];
        std::cout << std::endl;
      }
      break;
    }

    case AILERONS_RIGHT: {
      simInput.inputs[AXIS_AILERONS_SET] = fmax(-1.0, simInput.inputs[AXIS_AILERONS_SET] - flightControlsKeyChangeAileron);
      if (loggingFlightControlsEnabled) {
        std::cout << "WASM: AILERONS_RIGHT: ";
        std::cout << "(no data)";
        std::cout << " -> ";
        std::cout << simInput.inputs[AXIS_AILERONS_SET];
        std::cout << std::endl;
      }
      break;
    }

    case CENTER_AILER_RUDDER: {
      simInput.inputs[AXIS_RUDDER_SET] = 0.0;
      simInput.inputs[AXIS_AILERONS_SET] = 0.0;
      if (loggingFlightControlsEnabled) {
        std::cout << "WASM: CENTER_AILER_RUDDER: ";
        std::cout << "(no data)";
        std::cout << " -> ";
        std::cout << simInput.inputs[AXIS_AILERONS_SET];
        std::cout << " / ";
        std::cout << simInput.inputs[AXIS_RUDDER_SET];
        std::cout << std::endl;
      }
      break;
    }

    case ELEVATOR_SET: {
      simInput.inputs[AXIS_ELEVATOR_SET] = static_cast<long>(data0) / 16384.0;
      if (loggingFlightControlsEnabled) {
        std::cout << "WASM: ELEVATOR_SET: ";
        std::cout << static_cast<long>(data0);
        std::cout << " -> ";
        std::cout << simInput.inputs[AXIS_ELEVATOR_SET];
        std::cout << std::endl;
      }
      break;
    }

    case ELEV_DOWN: {
      simInput.inputs[AXIS_ELEVATOR_SET] = fmin(1.0, simInput.inputs[AXIS_ELEVATOR_SET] + flightControlsKeyChangeElevator);
      if (loggingFlightControlsEnabled) {
        std::cout << "WASM: ELEV_DOWN: ";
        std::cout << "(no data)";
        std::cout << " -> ";
        std::cout << simInput.inputs[AXIS_ELEVATOR_SET];
        std::cout << std::endl;
      }
      break;
    }

    case ELEV_UP: {
      simInput.inputs[AXIS_ELEVATOR_SET] = fmax(-1.0, simInput.inputs[AXIS_ELEVATOR_SET] - flightControlsKeyChangeElevator);
      if (loggingFlightControlsEnabled) {
        std::cout << "WASM: ELEV_UP: ";
        std::cout << "(no data)";
        std::cout << " -> ";
        std::cout << simInput.inputs[AXIS_ELEVATOR_SET];
        std::cout << std::endl;
      }
      break;
    }

    case AUTOPILOT_OFF: {
      simInputAutopilot.AP_disconnect = 1;
      std::cout << "WASM: event triggered: AUTOPILOT_OFF" << std::endl;
      break;
    }

    case AUTOPILOT_ON: {
      simInputAutopilot.AP_engage = 1;
      std::cout << "WASM: event triggered: AUTOPILOT_ON" << std::endl;
      break;
    }

    case TOGGLE_FLIGHT_DIRECTOR: {
      std::cout << "WASM: event triggered: TOGGLE_FLIGHT_DIRECTOR:" << static_cast<long>(data0) << std::endl;
      break;
    }

    case AP_MASTER: {
      simInputAutopilot.AP_1_push = 1;
      std::cout << "WASM: event triggered: AP_MASTER" << std::endl;
      break;
    }

    case AUTOPILOT_DISENGAGE_SET: {
      if (static_cast<long>(data0) == 1) {
        simInputAutopilot.AP_disconnect = 1;
        std::cout << "WASM: event triggered: AUTOPILOT_DISENGAGE_SET" << std::endl;
      }
      break;
    }

    case AUTOPILOT_DISENGAGE_TOGGLE: {
      simInputAutopilot.AP_1_push = 1;
      std::cout << "WASM: event triggered: AUTOPILOT_DISENGAGE_TOGGLE" << std::endl;
      break;
    }

    case A32NX_FCU_AP_1_PUSH: {
      simInputAutopilot.AP_1_push = 1;
      std::cout << "WASM: event triggered: A32NX_FCU_AP_1_PUSH" << std::endl;
      break;
    }

    case A32NX_FCU_AP_2_PUSH: {
      simInputAutopilot.AP_2_push = 1;
      std::cout << "WASM: event triggered: A32NX_FCU_AP_2_PUSH" << std::endl;
      break;
    }

    case A32NX_FCU_AP_DISCONNECT_PUSH: {
      simInputAutopilot.AP_disconnect = 1;
      std::cout << "WASM: event triggered: A32NX_FCU_AP_DISCONNECT_PUSH" << std::endl;
      break;
    }

    case A32NX_FCU_ATHR_PUSH: {
      simInputThrottles.ATHR_push = 1;
      std::cout << "WASM: event triggered: A32NX_FCU_ATHR_PUSH" << std::endl;
      break;
    }

    case A32NX_FCU_ATHR_DISCONNECT_PUSH: {
      simInputThrottles.ATHR_disconnect = 1;
      std::cout << "WASM: event triggered: A32NX_FCU_ATHR_DISCONNECT_PUSH" << std::endl;
      break;
    }

    case A32NX_FCU_SPD_INC: {
      execute_calculator_code("(>H:A320_Neo_FCU_SPEED_INC)", nullptr, nullptr, nullptr);
      std::cout << "WASM: event triggered: A32NX_FCU_SPD_INC" << std::endl;
      break;
    }

    case A32NX_FCU_SPD_DEC: {
      execute_calculator_code("(>H:A320_Neo_FCU_SPEED_DEC)", nullptr, nullptr, nullptr);
      std::cout << "WASM: event triggered: A32NX_FCU_SPD_DEC" << std::endl;
      break;
    }

    case A32NX_FCU_SPD_SET: {
      idFcuEventSetSPEED->set(static_cast<long>(data0));
      execute_calculator_code("(>H:A320_Neo_FCU_SPEED_SET)", nullptr, nullptr, nullptr);
      std::cout << "WASM: event triggered: A32NX_FCU_SPD_SET: " << static_cast<long>(data0) << std::endl;
      break;
    }

    case A32NX_FCU_SPD_PUSH:
    case AP_AIRSPEED_ON: {
      execute_calculator_code("(>H:A320_Neo_FCU_SPEED_PUSH)", nullptr, nullptr, nullptr);
      std::cout << "WASM: event triggered: A32NX_FCU_SPD_PUSH" << std::endl;
      break;
    }

    case A32NX_FCU_SPD_PULL:
    case AP_AIRSPEED_OFF: {
      execute_calculator_code("(>H:A320_Neo_FCU_SPEED_PULL)", nullptr, nullptr, nullptr);
      std::cout << "WASM: event triggered: A32NX_FCU_SPD_PULL" << std::endl;
      break;
    }

    case A32NX_FCU_SPD_MACH_TOGGLE_PUSH:
    case AP_MACH_HOLD: {
      execute_calculator_code("(>H:A320_Neo_FCU_SPEED_TOGGLE_SPEED_MACH)", nullptr, nullptr, nullptr);
      std::cout << "WASM: event triggered: A32NX_FCU_SPD_MACH_TOGGLE_PUSH" << std::endl;
      break;
    }

    case A32NX_FCU_HDG_INC: {
      execute_calculator_code(
          "(L:A32NX_TRK_FPA_MODE_ACTIVE, bool) 1 == if{ (>H:A320_Neo_FCU_HDG_INC_TRACK) } els{ (>H:A320_Neo_FCU_HDG_INC_HEADING) }",
          nullptr, nullptr, nullptr);
      std::cout << "WASM: event triggered: A32NX_FCU_HDG_INC" << std::endl;
      break;
    }

    case A32NX_FCU_HDG_DEC: {
      execute_calculator_code(
          "(L:A32NX_TRK_FPA_MODE_ACTIVE, bool) 1 == if{ (>H:A320_Neo_FCU_HDG_DEC_TRACK) } els{ (>H:A320_Neo_FCU_HDG_DEC_HEADING) }",
          nullptr, nullptr, nullptr);
      std::cout << "WASM: event triggered: A32NX_FCU_HDG_DEC" << std::endl;
      break;
    }

    case A32NX_FCU_HDG_SET: {
      idFcuEventSetHDG->set(static_cast<long>(data0));
      execute_calculator_code("(>H:A320_Neo_FCU_HDG_SET)", nullptr, nullptr, nullptr);
      std::cout << "WASM: event triggered: A32NX_FCU_HDG_SET: " << static_cast<long>(data0) << std::endl;
      break;
    }

    case A32NX_FCU_HDG_PUSH:
    case AP_HDG_HOLD_ON: {
      execute_calculator_code("(>H:A320_Neo_FCU_HDG_PUSH)", nullptr, nullptr, nullptr);
      std::cout << "WASM: event triggered: A32NX_FCU_HDG_PUSH" << std::endl;
      break;
    }

    case A32NX_FCU_HDG_PULL:
    case AP_HDG_HOLD_OFF: {
      execute_calculator_code("(>H:A320_Neo_FCU_HDG_PULL)", nullptr, nullptr, nullptr);
      std::cout << "WASM: event triggered: A32NX_FCU_HDG_PULL" << std::endl;
      break;
    }

    case A32NX_FCU_TRK_FPA_TOGGLE_PUSH:
    case AP_VS_HOLD: {
      execute_calculator_code("(L:A32NX_TRK_FPA_MODE_ACTIVE) ! (>L:A32NX_TRK_FPA_MODE_ACTIVE)", nullptr, nullptr, nullptr);
      std::cout << "WASM: event triggered: A32NX_FCU_TRK_FPA_TOGGLE_PUSH" << std::endl;
      break;
    }

    case A32NX_FCU_TO_AP_HDG_PUSH: {
      simInputAutopilot.HDG_push = 1;
      std::cout << "WASM: event triggered: A32NX_FCU_TO_AP_HDG_PUSH" << std::endl;
      break;
    }

    case A32NX_FCU_TO_AP_HDG_PULL: {
      simInputAutopilot.HDG_pull = 1;
      std::cout << "WASM: event triggered: A32NX_FCU_TO_AP_HDG_PULL" << std::endl;
      break;
    }

    case A32NX_FCU_ALT_INC: {
      long increment = static_cast<long>(data0);
      if (increment == 100) {
        execute_calculator_code(
            "3 (A:AUTOPILOT ALTITUDE LOCK VAR:3, feet) 100 + (A:AUTOPILOT ALTITUDE LOCK VAR:3, feet) "
            "100 % - 49000 min (>K:2:AP_ALT_VAR_SET_ENGLISH) (>H:AP_KNOB_Up) "
            "(>H:A320_Neo_CDU_AP_INC_ALT)",
            nullptr, nullptr, nullptr);
      } else if (increment == 1000) {
        execute_calculator_code(
            "3 (A:AUTOPILOT ALTITUDE LOCK VAR:3, feet) 1000 + (A:AUTOPILOT ALTITUDE LOCK VAR:3, feet) "
            "1000 % - 49000 min (>K:2:AP_ALT_VAR_SET_ENGLISH) (>H:AP_KNOB_Up) "
            "(>H:A320_Neo_CDU_AP_INC_ALT)",
            nullptr, nullptr, nullptr);
      } else {
        execute_calculator_code(
            "3 (A:AUTOPILOT ALTITUDE LOCK VAR:3, feet) (L:XMLVAR_Autopilot_Altitude_Increment) + (A:AUTOPILOT ALTITUDE LOCK VAR:3, feet) "
            "(L:XMLVAR_Autopilot_Altitude_Increment) % - 49000 min (>K:2:AP_ALT_VAR_SET_ENGLISH) (>H:AP_KNOB_Up) "
            "(>H:A320_Neo_CDU_AP_INC_ALT)",
            nullptr, nullptr, nullptr);
      }
      std::cout << "WASM: event triggered: A32NX_FCU_ALT_INC" << std::endl;
      break;
    }

    case A32NX_FCU_ALT_DEC: {
      long increment = static_cast<long>(data0);
      if (increment == 100) {
        execute_calculator_code(
            "3 (A:AUTOPILOT ALTITUDE LOCK VAR:3, feet) 100 - 100 "
            "(A:AUTOPILOT ALTITUDE LOCK VAR:3, feet) 100 % - 100 % "
            "+ 100 max (>K:2:AP_ALT_VAR_SET_ENGLISH) (>H:AP_KNOB_Down) (>H:A320_Neo_CDU_AP_DEC_ALT)",
            nullptr, nullptr, nullptr);
      } else if (increment == 1000) {
        execute_calculator_code(
            "3 (A:AUTOPILOT ALTITUDE LOCK VAR:3, feet) 1000 - 1000 "
            "(A:AUTOPILOT ALTITUDE LOCK VAR:3, feet) 1000 % - 1000 % "
            "+ 100 max (>K:2:AP_ALT_VAR_SET_ENGLISH) (>H:AP_KNOB_Down) (>H:A320_Neo_CDU_AP_DEC_ALT)",
            nullptr, nullptr, nullptr);
      } else {
        execute_calculator_code(
            "3 (A:AUTOPILOT ALTITUDE LOCK VAR:3, feet) (L:XMLVAR_Autopilot_Altitude_Increment) - (L:XMLVAR_Autopilot_Altitude_Increment) "
            "(A:AUTOPILOT ALTITUDE LOCK VAR:3, feet) (L:XMLVAR_Autopilot_Altitude_Increment) % - (L:XMLVAR_Autopilot_Altitude_Increment) "
            "% "
            "+ 100 max (>K:2:AP_ALT_VAR_SET_ENGLISH) (>H:AP_KNOB_Down) (>H:A320_Neo_CDU_AP_DEC_ALT)",
            nullptr, nullptr, nullptr);
      }
      std::cout << "WASM: event triggered: A32NX_FCU_ALT_DEC" << std::endl;
      break;
    }

    case A32NX_FCU_ALT_SET: {
      long value = 100 * (static_cast<long>(data0) / 100);
      std::ostringstream stringStream;
      stringStream << value;
      stringStream << " (>K:3:AP_ALT_VAR_SET_ENGLISH)";
      execute_calculator_code(stringStream.str().c_str(), nullptr, nullptr, nullptr);
      std::cout << "WASM: event triggered: A32NX_FCU_ALT_SET: " << value << std::endl;
      break;
    }

    case A32NX_FCU_ALT_INCREMENT_TOGGLE:
    case AP_ALT_HOLD: {
      execute_calculator_code(
          "(L:XMLVAR_Autopilot_Altitude_Increment, number) 100 == "
          "if{ 1000 (>L:XMLVAR_Autopilot_Altitude_Increment) } "
          "els{ 100 (>L:XMLVAR_Autopilot_Altitude_Increment) }",
          nullptr, nullptr, nullptr);
      std::cout << "WASM: event triggered: A32NX_FCU_ALT_INCREMENT_TOGGLE" << std::endl;
      break;
    }

    case A32NX_FCU_ALT_INCREMENT_SET: {
      long value = static_cast<long>(data0);
      if (value == 100 || value == 1000) {
        std::ostringstream stringStream;
        stringStream << value;
        stringStream << " (>L:XMLVAR_Autopilot_Altitude_Increment)";
        execute_calculator_code(stringStream.str().c_str(), nullptr, nullptr, nullptr);
        std::cout << "WASM: event triggered: A32NX_FCU_ALT_INCREMENT_SET: " << value << std::endl;
      }
      break;
    }

    case A32NX_FCU_ALT_PUSH:
    case AP_ALT_HOLD_ON: {
      simInputAutopilot.ALT_push = 1;
      execute_calculator_code("(>H:A320_Neo_CDU_MODE_MANAGED_ALTITUDE)", nullptr, nullptr, nullptr);
      std::cout << "WASM: event triggered: A32NX_FCU_ALT_PUSH" << std::endl;
      break;
    }

    case A32NX_FCU_ALT_PULL:
    case AP_ALT_HOLD_OFF: {
      simInputAutopilot.ALT_pull = 1;
      execute_calculator_code("(>H:A320_Neo_CDU_MODE_SELECTED_ALTITUDE)", nullptr, nullptr, nullptr);
      std::cout << "WASM: event triggered: A32NX_FCU_ALT_PULL" << std::endl;
      break;
    }

    case A32NX_FCU_VS_INC: {
      execute_calculator_code(
          "(L:A32NX_TRK_FPA_MODE_ACTIVE, bool) 1 == if{ (>H:A320_Neo_FCU_VS_INC_FPA) } els{ (>H:A320_Neo_FCU_VS_INC_VS) } "
          "(>H:A320_Neo_CDU_VS)",
          nullptr, nullptr, nullptr);
      std::cout << "WASM: event triggered: A32NX_FCU_VS_INC" << std::endl;
      break;
    }

    case A32NX_FCU_VS_DEC: {
      execute_calculator_code(
          "(L:A32NX_TRK_FPA_MODE_ACTIVE, bool) 1 == if{ (>H:A320_Neo_FCU_VS_DEC_FPA) } els{ (>H:A320_Neo_FCU_VS_DEC_VS) } "
          "(>H:A320_Neo_CDU_VS)",
          nullptr, nullptr, nullptr);
      std::cout << "WASM: event triggered: A32NX_FCU_VS_DEC" << std::endl;
      break;
    }

    case A32NX_FCU_VS_SET: {
      idFcuEventSetVS->set(static_cast<long>(data0));
      execute_calculator_code("(>H:A320_Neo_FCU_VS_SET) (>H:A320_Neo_CDU_VS)", nullptr, nullptr, nullptr);
      std::cout << "WASM: event triggered: A32NX_FCU_VS_SET: " << static_cast<long>(data0) << std::endl;
      break;
    }

    case A32NX_FCU_VS_PUSH:
    case AP_VS_ON: {
      execute_calculator_code("(>H:A320_Neo_FCU_VS_PUSH) (>H:A320_Neo_CDU_VS)", nullptr, nullptr, nullptr);
      std::cout << "WASM: event triggered: A32NX_FCU_VS_PUSH" << std::endl;
      break;
    }

    case A32NX_FCU_VS_PULL:
    case AP_VS_OFF: {
      execute_calculator_code("(>H:A320_Neo_FCU_VS_PULL) (>H:A320_Neo_CDU_VS)", nullptr, nullptr, nullptr);
      std::cout << "WASM: event triggered: A32NX_FCU_VS_PULL" << std::endl;
      break;
    }

    case A32NX_FCU_TO_AP_VS_PUSH: {
      simInputAutopilot.VS_push = 1;
      std::cout << "WASM: event triggered: A32NX_FCU_TO_AP_VS_PUSH" << std::endl;
      break;
    }
    case A32NX_FCU_TO_AP_VS_PULL: {
      simInputAutopilot.VS_pull = 1;
      std::cout << "WASM: event triggered: A32NX_FCU_TO_AP_VS_PULL" << std::endl;
      break;
    }

    case A32NX_FCU_LOC_PUSH: {
      simInputAutopilot.LOC_push = 1;
      std::cout << "WASM: event triggered: A32NX_FCU_LOC_PUSH" << std::endl;
      break;
    }

    case A32NX_FCU_APPR_PUSH: {
      simInputAutopilot.APPR_push = 1;
      std::cout << "WASM: event triggered: A32NX_FCU_APPR_PUSH" << std::endl;
      break;
    }

    case A32NX_FCU_EXPED_PUSH:
    case AP_ATT_HOLD: {
      simInputAutopilot.EXPED_push = 1;
      std::cout << "WASM: event triggered: A32NX_FCU_EXPED_PUSH" << std::endl;
      break;
    }

    case A32NX_FMGC_DIR_TO_TRIGGER: {
      simInputAutopilot.DIR_TO_trigger = 1;
      std::cout << "WASM: event triggered: A32NX_FMGC_DIR_TO_TRIGGER" << std::endl;
      break;
    }

    case A32NX_EFIS_L_CHRONO_PUSHED: {
      execute_calculator_code("(>H:A32NX_EFIS_L_CHRONO_PUSHED)", nullptr, nullptr, nullptr);
      std::cout << "WASM: event triggered: A32NX_EFIS_L_CHRONO_PUSHED" << std::endl;
      break;
    }

    case A32NX_EFIS_R_CHRONO_PUSHED: {
      execute_calculator_code("(>H:A32NX_EFIS_R_CHRONO_PUSHED)", nullptr, nullptr, nullptr);
      std::cout << "WASM: event triggered: A32NX_EFIS_R_CHRONO_PUSHED" << std::endl;
      break;
    }

    case AP_SPEED_SLOT_INDEX_SET: {
      // for the time being do not activate, it ends in a loop. more work has to be done to support this
      // if (static_cast<long>(event->data0) == 2) {
      //   execute_calculator_code("(>H:A320_Neo_FCU_SPEED_PUSH)", nullptr, nullptr, nullptr);
      // } else {
      //   execute_calculator_code("(>H:A320_Neo_FCU_SPEED_PULL)", nullptr, nullptr, nullptr);
      // }
      std::cout << "WASM: event triggered: SPEED_SLOT_INDEX_SET: " << static_cast<long>(data0) << std::endl;
      break;
    }

    case AP_SPD_VAR_INC: {
      execute_calculator_code("(>H:A320_Neo_FCU_SPEED_INC)", nullptr, nullptr, nullptr);
      std::cout << "WASM: event triggered: AP_SPD_VAR_INC" << std::endl;
      break;
    }

    case AP_SPD_VAR_DEC: {
      execute_calculator_code("(>H:A320_Neo_FCU_SPEED_DEC)", nullptr, nullptr, nullptr);
      std::cout << "WASM: event triggered: AP_SPD_VAR_DEC" << std::endl;
      break;
    }

    case AP_MACH_VAR_INC: {
      execute_calculator_code("(>H:A320_Neo_FCU_SPEED_INC)", nullptr, nullptr, nullptr);
      std::cout << "WASM: event triggered: AP_MACH_VAR_INC" << std::endl;
      break;
    }

    case AP_MACH_VAR_DEC: {
      execute_calculator_code("(>H:A320_Neo_FCU_SPEED_DEC)", nullptr, nullptr, nullptr);
      std::cout << "WASM: event triggered: AP_MACH_VAR_DEC" << std::endl;
      break;
    }

    case AP_HEADING_SLOT_INDEX_SET: {
      // for the time being do not activate, it ends in a loop. more work has to be done to support this
      // if (static_cast<long>(event->data0) == 2) {
      //   execute_calculator_code("(>H:A320_Neo_FCU_VS_PUSH)", nullptr, nullptr, nullptr);
      // } else {
      //   execute_calculator_code("(>H:A320_Neo_FCU_VS_PULL)", nullptr, nullptr, nullptr);
      // }
      std::cout << "WASM: event triggered: HEADING_SLOT_INDEX_SET: " << static_cast<long>(data0) << std::endl;
      break;
    }

    case HEADING_BUG_INC: {
      execute_calculator_code(
          "(L:A32NX_TRK_FPA_MODE_ACTIVE, bool) 1 == if{ (>H:A320_Neo_FCU_HDG_INC_TRACK) } els{ (>H:A320_Neo_FCU_HDG_INC_HEADING) }",
          nullptr, nullptr, nullptr);
      std::cout << "WASM: event triggered: HEADING_BUG_INC" << std::endl;
      break;
    }

    case HEADING_BUG_DEC: {
      execute_calculator_code(
          "(L:A32NX_TRK_FPA_MODE_ACTIVE, bool) 1 == if{ (>H:A320_Neo_FCU_HDG_DEC_TRACK) } els{ (>H:A320_Neo_FCU_HDG_DEC_HEADING) }",
          nullptr, nullptr, nullptr);
      std::cout << "WASM: event triggered: HEADING_BUG_DEC" << std::endl;
      break;
    }

    case AP_ALTITUDE_SLOT_INDEX_SET: {
      // for the time being do not activate, it ends in a loop. more work has to be done to support this
      // if (static_cast<long>(event->data0) == 2) {
      //   execute_calculator_code("(>H:A320_Neo_FCU_ALT_PUSH) (>H:A320_Neo_CDU_MODE_MANAGED_ALTITUDE)", nullptr, nullptr, nullptr);
      // } else {
      //   execute_calculator_code("(>H:A320_Neo_FCU_ALT_PULL) (>H:A320_Neo_CDU_MODE_SELECTED_ALTITUDE)", nullptr, nullptr, nullptr);
      // }
      std::cout << "WASM: event triggered: ALTITUDE_SLOT_INDEX_SET: " << static_cast<long>(data0) << std::endl;
      break;
    }

    case AP_ALT_VAR_INC: {
      execute_calculator_code(
          "3 (A:AUTOPILOT ALTITUDE LOCK VAR:3, feet) (L:XMLVAR_Autopilot_Altitude_Increment) + (A:AUTOPILOT ALTITUDE LOCK VAR:3, feet) "
          "(L:XMLVAR_Autopilot_Altitude_Increment) % - 49000 min (>K:2:AP_ALT_VAR_SET_ENGLISH) (>H:AP_KNOB_Up) "
          "(>H:A320_Neo_CDU_AP_INC_ALT)",
          nullptr, nullptr, nullptr);
      std::cout << "WASM: event triggered: AP_ALT_VAR_INC" << std::endl;
      break;
    }

    case AP_ALT_VAR_DEC: {
      execute_calculator_code(
          "3 (A:AUTOPILOT ALTITUDE LOCK VAR:3, feet) (L:XMLVAR_Autopilot_Altitude_Increment) - (L:XMLVAR_Autopilot_Altitude_Increment) "
          "(A:AUTOPILOT ALTITUDE LOCK VAR:3, feet) (L:XMLVAR_Autopilot_Altitude_Increment) % - (L:XMLVAR_Autopilot_Altitude_Increment) % "
          "+ 100 max (>K:2:AP_ALT_VAR_SET_ENGLISH) (>H:AP_KNOB_Down) (>H:A320_Neo_CDU_AP_DEC_ALT)",
          nullptr, nullptr, nullptr);
      std::cout << "WASM: event triggered: AP_ALT_VAR_DEC" << std::endl;
      break;
    }

    case AP_VS_SLOT_INDEX_SET: {
      // for the time being do not activate, it ends in a loop. more work has to be done to support this
      // if (static_cast<long>(event->data0) == 2) {
      //   execute_calculator_code("(>H:A320_Neo_FCU_VS_PUSH)", nullptr, nullptr, nullptr);
      // } else {
      //   execute_calculator_code("(>H:A320_Neo_FCU_VS_PULL)", nullptr, nullptr, nullptr);
      // }
      std::cout << "WASM: event triggered: VS_SLOT_INDEX_SET: " << static_cast<long>(data0) << std::endl;
      break;
    }

    case AP_VS_VAR_INC: {
      execute_calculator_code(
          "(L:A32NX_TRK_FPA_MODE_ACTIVE, bool) 1 == if{ (>H:A320_Neo_FCU_VS_INC_FPA) } els{ (>H:A320_Neo_FCU_VS_INC_VS) }", nullptr,
          nullptr, nullptr);
      std::cout << "WASM: event triggered: AP_VS_VAR_INC" << std::endl;
      break;
    }

    case AP_VS_VAR_DEC: {
      execute_calculator_code(
          "(L:A32NX_TRK_FPA_MODE_ACTIVE, bool) 1 == if{ (>H:A320_Neo_FCU_VS_DEC_FPA) } els{ (>H:A320_Neo_FCU_VS_DEC_VS) }", nullptr,
          nullptr, nullptr);
      std::cout << "WASM: event triggered: AP_VS_VAR_DEC" << std::endl;
      break;
    }

    case AP_APR_HOLD: {
      simInputAutopilot.APPR_push = 1;
      std::cout << "WASM: event triggered: AP_APR_HOLD" << std::endl;
      break;
    }

    case AP_LOC_HOLD: {
      simInputAutopilot.LOC_push = 1;
      std::cout << "WASM: event triggered: AP_LOC_HOLD" << std::endl;
      break;
    }

    case AUTO_THROTTLE_ARM: {
      simInputThrottles.ATHR_push = 1;
      std::cout << "WASM: event triggered: AUTO_THROTTLE_ARM" << std::endl;
      break;
    }

    case AUTO_THROTTLE_DISCONNECT: {
      simInputThrottles.ATHR_disconnect = 1;
      std::cout << "WASM: event triggered: AUTO_THROTTLE_DISCONNECT" << std::endl;
      break;
    }

    case A32NX_ATHR_RESET_DISABLE: {
      simInputThrottles.ATHR_reset_disable = 1;
      std::cout << "WASM: event triggered: ATHR_RESET_DISABLE" << std::endl;
      break;
    }

    case AUTO_THROTTLE_TO_GA: {
      throttleAxis[0]->onEventThrottleFull();
      throttleAxis[1]->onEventThrottleFull();
      std::cout << "WASM: event triggered: AUTO_THROTTLE_TO_GA (treated like THROTTLE_FULL)" << std::endl;
      break;
    }

    case A32NX_THROTTLE_MAPPING_SET_DEFAULTS: {
      std::cout << "WASM: event triggered: THROTTLE_MAPPING_SET_DEFAULTS" << std::endl;
      throttleAxis[0]->applyDefaults();
      throttleAxis[1]->applyDefaults();
      break;
    }

    case A32NX_THROTTLE_MAPPING_LOAD_FROM_FILE: {
      std::cout << "WASM: event triggered: THROTTLE_MAPPING_LOAD_FROM_FILE" << std::endl;
      throttleAxis[0]->loadFromFile();
      throttleAxis[1]->loadFromFile();
      break;
    }

    case A32NX_THROTTLE_MAPPING_LOAD_FROM_LOCAL_VARIABLES: {
      std::cout << "WASM: event triggered: THROTTLE_MAPPING_LOAD_FROM_LOCAL_VARIABLES" << std::endl;
      throttleAxis[0]->loadFromLocalVariables();
      throttleAxis[1]->loadFromLocalVariables();
      break;
    }

    case A32NX_THROTTLE_MAPPING_SAVE_TO_FILE: {
      std::cout << "WASM: event triggered: THROTTLE_MAPPING_SAVE_TO_FILE" << std::endl;
      throttleAxis[0]->saveToFile();
      throttleAxis[1]->saveToFile();
      break;
    }

    case THROTTLE_SET: {
      throttleAxis[0]->onEventThrottleSet(static_cast<long>(data0));
      throttleAxis[1]->onEventThrottleSet(static_cast<long>(data0));
      if (loggingThrottlesEnabled) {
        std::cout << "WASM: THROTTLE_SET: " << static_cast<long>(data0) << std::endl;
      }
      break;
    }

    case THROTTLE1_SET: {
      throttleAxis[0]->onEventThrottleSet(static_cast<long>(data0));
      if (loggingThrottlesEnabled) {
        std::cout << "WASM: THROTTLE1_SET: " << static_cast<long>(data0) << std::endl;
      }
      break;
    }

    case THROTTLE2_SET: {
      throttleAxis[1]->onEventThrottleSet(static_cast<long>(data0));
      if (loggingThrottlesEnabled) {
        std::cout << "WASM: THROTTLE2_SET: " << static_cast<long>(data0) << std::endl;
      }
      break;
    }

    case THROTTLE_AXIS_SET_EX1: {
      throttleAxis[0]->onEventThrottleSet(static_cast<long>(data0));
      throttleAxis[1]->onEventThrottleSet(static_cast<long>(data0));
      if (loggingThrottlesEnabled) {
        std::cout << "WASM: THROTTLE_AXIS_SET_EX1: " << static_cast<long>(data0) << std::endl;
      }
      break;
    }

    case THROTTLE1_AXIS_SET_EX1: {
      throttleAxis[0]->onEventThrottleSet(static_cast<long>(data0));
      if (loggingThrottlesEnabled) {
        std::cout << "WASM: THROTTLE1_AXIS_SET_EX1: " << static_cast<long>(data0) << std::endl;
      }
      break;
    }

    case THROTTLE2_AXIS_SET_EX1: {
      throttleAxis[1]->onEventThrottleSet(static_cast<long>(data0));
      if (loggingThrottlesEnabled) {
        std::cout << "WASM: THROTTLE2_AXIS_SET_EX1: " << static_cast<long>(data0) << std::endl;
      }
      break;
    }

    case THROTTLE_FULL: {
      throttleAxis[0]->onEventThrottleFull();
      throttleAxis[1]->onEventThrottleFull();
      if (loggingThrottlesEnabled) {
        std::cout << "WASM: THROTTLE_FULL" << std::endl;
      }
      break;
    }

    case THROTTLE_CUT: {
      throttleAxis[0]->onEventThrottleCut();
      throttleAxis[1]->onEventThrottleCut();
      if (loggingThrottlesEnabled) {
        std::cout << "WASM: THROTTLE_CUT" << std::endl;
      }
      break;
    }

    case THROTTLE_INCR: {
      throttleAxis[0]->onEventThrottleIncrease();
      throttleAxis[1]->onEventThrottleIncrease();
      if (loggingThrottlesEnabled) {
        std::cout << "WASM: THROTTLE_INCR" << std::endl;
      }
      break;
    }

    case THROTTLE_DECR: {
      throttleAxis[0]->onEventThrottleDecrease();
      throttleAxis[1]->onEventThrottleDecrease();
      if (loggingThrottlesEnabled) {
        std::cout << "WASM: THROTTLE_DECR" << std::endl;
      }
      break;
    }

    case THROTTLE_INCR_SMALL: {
      throttleAxis[0]->onEventThrottleIncreaseSmall();
      throttleAxis[1]->onEventThrottleIncreaseSmall();
      if (loggingThrottlesEnabled) {
        std::cout << "WASM: THROTTLE_INCR_SMALL" << std::endl;
      }
      break;
    }

    case THROTTLE_DECR_SMALL: {
      throttleAxis[0]->onEventThrottleDecreaseSmall();
      throttleAxis[1]->onEventThrottleDecreaseSmall();
      if (loggingThrottlesEnabled) {
        std::cout << "WASM: THROTTLE_DECR_SMALL" << std::endl;
      }
      break;
    }

    case THROTTLE_10: {
      throttleAxis[0]->onEventThrottleSet_10();
      throttleAxis[1]->onEventThrottleSet_10();
      if (loggingThrottlesEnabled) {
        std::cout << "WASM: THROTTLE_10" << std::endl;
      }
      break;
    }

    case THROTTLE_20: {
      throttleAxis[0]->onEventThrottleSet_20();
      throttleAxis[1]->onEventThrottleSet_20();
      if (loggingThrottlesEnabled) {
        std::cout << "WASM: THROTTLE_20" << std::endl;
      }
      break;
    }

    case THROTTLE_30: {
      throttleAxis[0]->onEventThrottleSet_30();
      throttleAxis[1]->onEventThrottleSet_30();
      if (loggingThrottlesEnabled) {
        std::cout << "WASM: THROTTLE_30" << std::endl;
      }
      break;
    }

    case THROTTLE_40: {
      throttleAxis[0]->onEventThrottleSet_40();
      throttleAxis[1]->onEventThrottleSet_40();
      if (loggingThrottlesEnabled) {
        std::cout << "WASM: THROTTLE_40" << std::endl;
      }
      break;
    }

    case THROTTLE_50: {
      throttleAxis[0]->onEventThrottleSet_50();
      throttleAxis[1]->onEventThrottleSet_50();
      if (loggingThrottlesEnabled) {
        std::cout << "WASM: THROTTLE_50" << std::endl;
      }
      break;
    }

    case THROTTLE_60: {
      throttleAxis[0]->onEventThrottleSet_50();
      throttleAxis[1]->onEventThrottleSet_60();
      if (loggingThrottlesEnabled) {
        std::cout << "WASM: THROTTLE_60" << std::endl;
      }
      break;
    }

    case THROTTLE_70: {
      throttleAxis[0]->onEventThrottleSet_70();
      throttleAxis[1]->onEventThrottleSet_70();
      if (loggingThrottlesEnabled) {
        std::cout << "WASM: THROTTLE_70" << std::endl;
      }
      break;
    }

    case THROTTLE_80: {
      throttleAxis[0]->onEventThrottleSet_80();
      throttleAxis[1]->onEventThrottleSet_80();
      if (loggingThrottlesEnabled) {
        std::cout << "WASM: THROTTLE_80" << std::endl;
      }
      break;
    }

    case THROTTLE_90: {
      throttleAxis[0]->onEventThrottleSet_90();
      throttleAxis[1]->onEventThrottleSet_90();
      if (loggingThrottlesEnabled) {
        std::cout << "WASM: THROTTLE_90" << std::endl;
      }
      break;
    }

    case THROTTLE1_FULL: {
      throttleAxis[0]->onEventThrottleFull();
      if (loggingThrottlesEnabled) {
        std::cout << "WASM: THROTTLE1_FULL" << std::endl;
      }
      break;
    }

    case THROTTLE1_CUT: {
      throttleAxis[0]->onEventThrottleCut();
      if (loggingThrottlesEnabled) {
        std::cout << "WASM: THROTTLE1_CUT" << std::endl;
      }
      break;
    }

    case THROTTLE1_INCR: {
      throttleAxis[0]->onEventThrottleIncrease();
      if (loggingThrottlesEnabled) {
        std::cout << "WASM: THROTTLE1_INCR" << std::endl;
      }
      break;
    }

    case THROTTLE1_DECR: {
      throttleAxis[0]->onEventThrottleDecrease();
      if (loggingThrottlesEnabled) {
        std::cout << "WASM: THROTTLE1_DECR" << std::endl;
      }
      break;
    }

    case THROTTLE1_INCR_SMALL: {
      throttleAxis[0]->onEventThrottleIncreaseSmall();
      if (loggingThrottlesEnabled) {
        std::cout << "WASM: THROTTLE1_INCR_SMALL" << std::endl;
      }
      break;
    }

    case THROTTLE1_DECR_SMALL: {
      throttleAxis[0]->onEventThrottleDecreaseSmall();
      if (loggingThrottlesEnabled) {
        std::cout << "WASM: THROTTLE1_DECR_SMALL" << std::endl;
      }
      break;
    }

    case THROTTLE2_FULL: {
      throttleAxis[1]->onEventThrottleFull();
      if (loggingThrottlesEnabled) {
        std::cout << "WASM: THROTTLE2_FULL" << std::endl;
      }
      break;
    }

    case THROTTLE2_CUT: {
      throttleAxis[1]->onEventThrottleCut();
      if (loggingThrottlesEnabled) {
        std::cout << "WASM: THROTTLE2_CUT" << std::endl;
      }
      break;
    }

    case THROTTLE2_INCR: {
      throttleAxis[1]->onEventThrottleIncrease();
      if (loggingThrottlesEnabled) {
        std::cout << "WASM: THROTTLE2_INCR" << std::endl;
      }
      break;
    }

    case THROTTLE2_DECR: {
      throttleAxis[1]->onEventThrottleDecrease();
      if (loggingThrottlesEnabled) {
        std::cout << "WASM: THROTTLE2_DECR" << std::endl;
      }
      break;
    }

    case THROTTLE2_INCR_SMALL: {
      throttleAxis[1]->onEventThrottleIncreaseSmall();
      if (loggingThrottlesEnabled) {
        std::cout << "WASM: THROTTLE2_INCR_SMALL" << std::endl;
      }
      break;
    }

    case THROTTLE2_DECR_SMALL: {
      throttleAxis[1]->onEventThrottleDecreaseSmall();
      if (loggingThrottlesEnabled) {
        std::cout << "WASM: THROTTLE2_DECR_SMALL" << std::endl;
      }
      break;
    }

    case THROTTLE_REVERSE_THRUST_TOGGLE: {
      throttleAxis[0]->onEventReverseToggle();
      throttleAxis[1]->onEventReverseToggle();
      if (loggingThrottlesEnabled) {
        std::cout << "WASM: THROTTLE_REVERSE_THRUST_TOGGLE" << std::endl;
      }
      break;
    }
    case THROTTLE_REVERSE_THRUST_HOLD: {
      throttleAxis[0]->onEventReverseHold(static_cast<bool>(data0));
      throttleAxis[1]->onEventReverseHold(static_cast<bool>(data0));
      if (loggingThrottlesEnabled) {
        std::cout << "WASM: THROTTLE_REVERSE_THRUST_HOLD: " << static_cast<long>(data0) << std::endl;
      }
      break;
    }

    case SPOILERS_ON: {
      spoilersHandler->onEventSpoilersOn();
      if (loggingFlightControlsEnabled) {
        std::cout << "WASM: SPOILERS_ON: ";
        std::cout << "(no data)";
        std::cout << " -> ";
        std::cout << spoilersHandler->getHandlePosition();
        std::cout << " / ";
        std::cout << spoilersHandler->getIsArmed();
        std::cout << std::endl;
      }
      break;
    }

    case SPOILERS_OFF: {
      spoilersHandler->onEventSpoilersOff();
      if (loggingFlightControlsEnabled) {
        std::cout << "WASM: SPOILERS_OFF: ";
        std::cout << "(no data)";
        std::cout << " -> ";
        std::cout << spoilersHandler->getHandlePosition();
        std::cout << " / ";
        std::cout << spoilersHandler->getIsArmed();
        std::cout << std::endl;
      }
      break;
    }

    case SPOILERS_TOGGLE: {
      spoilersHandler->onEventSpoilersToggle();
      if (loggingFlightControlsEnabled) {
        std::cout << "WASM: SPOILERS_TOGGLE: ";
        std::cout << "(no data)";
        std::cout << " -> ";
        std::cout << spoilersHandler->getHandlePosition();
        std::cout << " / ";
        std::cout << spoilersHandler->getIsArmed();
        std::cout << std::endl;
      }
      break;
    }

    case SPOILERS_SET: {
      spoilersHandler->onEventSpoilersSet(static_cast<long>(data0));
      if (loggingFlightControlsEnabled) {
        std::cout << "WASM: SPOILERS_SET: ";
        std::cout << static_cast<long>(data0);
        std::cout << " -> ";
        std::cout << spoilersHandler->getHandlePosition();
        std::cout << " / ";
        std::cout << spoilersHandler->getIsArmed();
        std::cout << std::endl;
      }
      break;
    }

    case AXIS_SPOILER_SET: {
      spoilersHandler->onEventSpoilersAxisSet(static_cast<long>(data0));
      if (loggingFlightControlsEnabled) {
        std::cout << "WASM: AXIS_SPOILER_SET: ";
        std::cout << static_cast<long>(data0);
        std::cout << " -> ";
        std::cout << spoilersHandler->getHandlePosition();
        std::cout << " / ";
        std::cout << spoilersHandler->getIsArmed();
        std::cout << std::endl;
      }
      break;
    }

    case SPOILERS_ARM_ON: {
      spoilersHandler->onEventSpoilersArmOn();
      if (loggingFlightControlsEnabled) {
        std::cout << "WASM: SPOILERS_ARM_ON: ";
        std::cout << "(no data)";
        std::cout << " -> ";
        std::cout << spoilersHandler->getHandlePosition();
        std::cout << " / ";
        std::cout << spoilersHandler->getIsArmed();
        std::cout << std::endl;
      }
      break;
    }

    case SPOILERS_ARM_OFF: {
      spoilersHandler->onEventSpoilersArmOff();
      if (loggingFlightControlsEnabled) {
        std::cout << "WASM: SPOILERS_ARM_OFF: ";
        std::cout << "(no data)";
        std::cout << " -> ";
        std::cout << spoilersHandler->getHandlePosition();
        std::cout << " / ";
        std::cout << spoilersHandler->getIsArmed();
        std::cout << std::endl;
      }
      break;
    }

    case SPOILERS_ARM_TOGGLE: {
      spoilersHandler->onEventSpoilersArmToggle();
      if (loggingFlightControlsEnabled) {
        std::cout << "WASM: SPOILERS_ARM_TOGGLE: ";
        std::cout << "(no data)";
        std::cout << " -> ";
        std::cout << spoilersHandler->getHandlePosition();
        std::cout << " / ";
        std::cout << spoilersHandler->getIsArmed();
        std::cout << std::endl;
      }
      break;
    }

    case SPOILERS_ARM_SET: {
      spoilersHandler->onEventSpoilersArmSet(static_cast<long>(data0) == 1);
      if (loggingFlightControlsEnabled) {
        std::cout << "WASM: SPOILERS_ARM_SET: ";
        std::cout << static_cast<long>(data0);
        std::cout << " -> ";
        std::cout << spoilersHandler->getHandlePosition();
        std::cout << " / ";
        std::cout << spoilersHandler->getIsArmed();
        std::cout << std::endl;
      }
      break;
    }

    case SIM_RATE_INCR: {
      // calculate frame rate that will be seen by FBW / AP
      double theoreticalFrameRate = (1 / sampleTime) / (simData.simulation_rate * 2);
      // determine if an increase of simulation rate can be allowed
      if ((simData.simulation_rate < maxSimulationRate && theoreticalFrameRate >= 5) || simData.simulation_rate < 1 ||
          !limitSimulationRateByPerformance) {
        sendEvent(SIM_RATE_INCR, 0, SIMCONNECT_GROUP_PRIORITY_DEFAULT);
        std::cout << "WASM: Simulation rate " << simData.simulation_rate;
        std::cout << " -> " << simData.simulation_rate * 2;
        std::cout << " (theoretical fps " << theoreticalFrameRate << ")" << std::endl;
      } else {
        std::cout << "WASM: Simulation rate " << simData.simulation_rate;
        std::cout << " -> " << simData.simulation_rate;
        std::cout << " (limited by max sim rate or theoretical fps " << theoreticalFrameRate << ")" << std::endl;
      }
      break;
    }

    case SIM_RATE_DECR: {
      if (simData.simulation_rate > minSimulationRate) {
        sendEvent(SIM_RATE_DECR, 0, SIMCONNECT_GROUP_PRIORITY_DEFAULT);
        std::cout << "WASM: Simulation rate " << simData.simulation_rate;
        std::cout << " -> " << simData.simulation_rate / 2;
        std::cout << std::endl;
      } else {
        std::cout << "WASM: Simulation rate " << simData.simulation_rate;
        std::cout << " -> " << simData.simulation_rate;
        std::cout << " (limited by min sim rate)" << std::endl;
      }
      break;
    }

    case SIM_RATE_SET: {
      long targetSimulationRate = min(maxSimulationRate, max(1, static_cast<long>(data0)));
      sendEvent(SIM_RATE_SET, targetSimulationRate, SIMCONNECT_GROUP_PRIORITY_DEFAULT);
      std::cout << "WASM: Simulation Rate set to " << targetSimulationRate << std::endl;
      break;
    }

    default:
      break;
  }
}

void SimConnectInterface::simConnectProcessSimObjectData(const SIMCONNECT_RECV_SIMOBJECT_DATA* data) {
  // process depending on request id
  switch (data->dwRequestID) {
    case 0:
      // store aircraft data
      simData = *((SimData*)&data->dwData);
      return;

    default:
      // print unknown request id
      std::cout << "WASM: Unknown request id in SimConnect connection: ";
      std::cout << data->dwRequestID << std::endl;
      return;
  }
}

void SimConnectInterface::simConnectProcessClientData(const SIMCONNECT_RECV_CLIENT_DATA* data) {
  // process depending on request id
  switch (data->dwRequestID) {
    case ClientData::AUTOPILOT_STATE_MACHINE:
      // store aircraft data
      clientDataAutopilotStateMachine = *((ClientDataAutopilotStateMachine*)&data->dwData);
      return;

    case ClientData::AUTOPILOT_LAWS:
      // store aircraft data
      clientDataAutopilotLaws = *((ClientDataAutopilotLaws*)&data->dwData);
      return;

    case ClientData::AUTOTHRUST:
      // store aircraft data
      clientDataAutothrust = *((ClientDataAutothrust*)&data->dwData);
      return;

    case ClientData::ELAC_DISCRETE_OUTPUTS:
      // store aircraft data
      clientDataElacDiscreteOutputs = *((base_elac_discrete_outputs*)&data->dwData);
      return;

    case ClientData::ELAC_ANALOG_OUTPUTS:
      // store aircraft data
      clientDataElacAnalogOutputs = *((base_elac_analog_outputs*)&data->dwData);
      return;

    case ClientData::ELAC_1_BUS_OUTPUT:
      // store aircraft data
      clientDataElacBusOutputs = *((base_elac_out_bus*)&data->dwData);
      return;

    case ClientData::ELAC_2_BUS_OUTPUT:
      // store aircraft data
      clientDataElacBusOutputs = *((base_elac_out_bus*)&data->dwData);
      return;

    case ClientData::SEC_DISCRETE_OUTPUTS:
      // store aircraft data
      clientDataSecDiscreteOutputs = *((base_sec_discrete_outputs*)&data->dwData);
      return;

    case ClientData::SEC_ANALOG_OUTPUTS:
      // store aircraft data
      clientDataSecAnalogOutputs = *((base_sec_analog_outputs*)&data->dwData);
      return;

    case ClientData::SEC_1_BUS_OUTPUT:
      // store aircraft data
      clientDataSecBusOutputs = *((base_sec_out_bus*)&data->dwData);
      return;

    case ClientData::SEC_2_BUS_OUTPUT:
      // store aircraft data
      clientDataSecBusOutputs = *((base_sec_out_bus*)&data->dwData);
      return;

    case ClientData::FAC_DISCRETE_OUTPUTS:
      // store aircraft data
      clientDataFacDiscreteOutputs = *((base_fac_discrete_outputs*)&data->dwData);
      return;

    case ClientData::FAC_ANALOG_OUTPUTS:
      // store aircraft data
      clientDataFacAnalogOutputs = *((base_fac_analog_outputs*)&data->dwData);
      return;

    case ClientData::FAC_1_BUS_OUTPUT:
      // store aircraft data
      clientDataFacBusOutputs = *((base_fac_bus*)&data->dwData);
      return;

    case ClientData::FAC_2_BUS_OUTPUT:
      // store aircraft data
      clientDataFacBusOutputs = *((base_fac_bus*)&data->dwData);
      return;

    default:
      // print unknown request id
      std::cout << "WASM: Unknown request id in SimConnect connection: ";
      std::cout << data->dwRequestID << std::endl;
      return;
  }
}

bool SimConnectInterface::sendClientData(SIMCONNECT_DATA_DEFINITION_ID id, DWORD size, void* data) {
  // check if we are connected
  if (!isConnected) {
    return false;
  }

  // check if client data is enabled
  if (!clientDataEnabled) {
    std::cout << "WASM: Client data is disabled but tried to write it!";
    return true;
  }

  // set output data
  HRESULT result = SimConnect_SetClientData(hSimConnect, id, id, SIMCONNECT_CLIENT_DATA_SET_FLAG_DEFAULT, 0, size, data);

  // check result of data request
  if (result != S_OK) {
    // request failed
    return false;
  }

  // success
  return true;
}

bool SimConnectInterface::sendData(SIMCONNECT_DATA_DEFINITION_ID id, DWORD size, void* data) {
  // check if we are connected
  if (!isConnected) {
    return false;
  }

  // set output data
  HRESULT result = SimConnect_SetDataOnSimObject(hSimConnect, id, SIMCONNECT_OBJECT_ID_USER, 0, 0, size, data);

  // check result of data request
  if (result != S_OK) {
    // request failed
    return false;
  }

  // success
  return true;
}

bool SimConnectInterface::addDataDefinition(const HANDLE connectionHandle,
                                            const SIMCONNECT_DATA_DEFINITION_ID id,
                                            const SIMCONNECT_DATATYPE dataType,
                                            const std::string& dataName,
                                            const std::string& dataUnit) {
  HRESULT result =
      SimConnect_AddToDataDefinition(connectionHandle, id, dataName.c_str(),
                                     SimConnectInterface::isSimConnectDataTypeStruct(dataType) ? nullptr : dataUnit.c_str(), dataType);

  return (result == S_OK);
}

bool SimConnectInterface::addInputDataDefinition(const HANDLE connectionHandle,
                                                 const SIMCONNECT_DATA_DEFINITION_ID groupId,
                                                 const SIMCONNECT_CLIENT_EVENT_ID eventId,
                                                 const std::string& eventName,
                                                 const bool maskEvent) {
  HRESULT result = SimConnect_MapClientEventToSimEvent(connectionHandle, eventId, eventName.c_str());

  if (result != S_OK) {
    // failed -> abort
    return false;
  }

  result = SimConnect_AddClientEventToNotificationGroup(connectionHandle, groupId, eventId, maskEvent ? TRUE : FALSE);
  if (result != S_OK) {
    // failed -> abort
    return false;
  }

  result = SimConnect_SetNotificationGroupPriority(connectionHandle, groupId, SIMCONNECT_GROUP_PRIORITY_HIGHEST_MASKABLE);

  if (result != S_OK) {
    // failed -> abort
    return false;
  }

  // success
  return true;
}

bool SimConnectInterface::isSimConnectDataTypeStruct(SIMCONNECT_DATATYPE type) {
  switch (type) {
    case SIMCONNECT_DATATYPE_INITPOSITION:
    case SIMCONNECT_DATATYPE_MARKERSTATE:
    case SIMCONNECT_DATATYPE_WAYPOINT:
    case SIMCONNECT_DATATYPE_LATLONALT:
    case SIMCONNECT_DATATYPE_XYZ:
      return true;

    default:
      return false;
  }
  return false;
}

std::string SimConnectInterface::getSimConnectExceptionString(SIMCONNECT_EXCEPTION exception) {
  switch (exception) {
    case SIMCONNECT_EXCEPTION_NONE:
      return "NONE";

    case SIMCONNECT_EXCEPTION_ERROR:
      return "ERROR";

    case SIMCONNECT_EXCEPTION_SIZE_MISMATCH:
      return "SIZE_MISMATCH";

    case SIMCONNECT_EXCEPTION_UNRECOGNIZED_ID:
      return "UNRECOGNIZED_ID";

    case SIMCONNECT_EXCEPTION_UNOPENED:
      return "UNOPENED";

    case SIMCONNECT_EXCEPTION_VERSION_MISMATCH:
      return "VERSION_MISMATCH";

    case SIMCONNECT_EXCEPTION_TOO_MANY_GROUPS:
      return "TOO_MANY_GROUPS";

    case SIMCONNECT_EXCEPTION_NAME_UNRECOGNIZED:
      return "NAME_UNRECOGNIZED";

    case SIMCONNECT_EXCEPTION_TOO_MANY_EVENT_NAMES:
      return "TOO_MANY_EVENT_NAMES";

    case SIMCONNECT_EXCEPTION_EVENT_ID_DUPLICATE:
      return "EVENT_ID_DUPLICATE";

    case SIMCONNECT_EXCEPTION_TOO_MANY_MAPS:
      return "TOO_MANY_MAPS";

    case SIMCONNECT_EXCEPTION_TOO_MANY_OBJECTS:
      return "TOO_MANY_OBJECTS";

    case SIMCONNECT_EXCEPTION_TOO_MANY_REQUESTS:
      return "TOO_MANY_REQUESTS";

    case SIMCONNECT_EXCEPTION_WEATHER_INVALID_PORT:
      return "WEATHER_INVALID_PORT";

    case SIMCONNECT_EXCEPTION_WEATHER_INVALID_METAR:
      return "WEATHER_INVALID_METAR";

    case SIMCONNECT_EXCEPTION_WEATHER_UNABLE_TO_GET_OBSERVATION:
      return "WEATHER_UNABLE_TO_GET_OBSERVATION";

    case SIMCONNECT_EXCEPTION_WEATHER_UNABLE_TO_CREATE_STATION:
      return "WEATHER_UNABLE_TO_CREATE_STATION";

    case SIMCONNECT_EXCEPTION_WEATHER_UNABLE_TO_REMOVE_STATION:
      return "WEATHER_UNABLE_TO_REMOVE_STATION";

    case SIMCONNECT_EXCEPTION_INVALID_DATA_TYPE:
      return "INVALID_DATA_TYPE";

    case SIMCONNECT_EXCEPTION_INVALID_DATA_SIZE:
      return "INVALID_DATA_SIZE";

    case SIMCONNECT_EXCEPTION_DATA_ERROR:
      return "DATA_ERROR";

    case SIMCONNECT_EXCEPTION_INVALID_ARRAY:
      return "INVALID_ARRAY";

    case SIMCONNECT_EXCEPTION_CREATE_OBJECT_FAILED:
      return "CREATE_OBJECT_FAILED";

    case SIMCONNECT_EXCEPTION_LOAD_FLIGHTPLAN_FAILED:
      return "LOAD_FLIGHTPLAN_FAILED";

    case SIMCONNECT_EXCEPTION_OPERATION_INVALID_FOR_OBJECT_TYPE:
      return "OPERATION_INVALID_FOR_OBJECT_TYPE";

    case SIMCONNECT_EXCEPTION_ILLEGAL_OPERATION:
      return "ILLEGAL_OPERATION";

    case SIMCONNECT_EXCEPTION_ALREADY_SUBSCRIBED:
      return "ALREADY_SUBSCRIBED";

    case SIMCONNECT_EXCEPTION_INVALID_ENUM:
      return "INVALID_ENUM";

    case SIMCONNECT_EXCEPTION_DEFINITION_ERROR:
      return "DEFINITION_ERROR";

    case SIMCONNECT_EXCEPTION_DUPLICATE_ID:
      return "DUPLICATE_ID";

    case SIMCONNECT_EXCEPTION_DATUM_ID:
      return "DATUM_ID";

    case SIMCONNECT_EXCEPTION_OUT_OF_BOUNDS:
      return "OUT_OF_BOUNDS";

    case SIMCONNECT_EXCEPTION_ALREADY_CREATED:
      return "ALREADY_CREATED";

    case SIMCONNECT_EXCEPTION_OBJECT_OUTSIDE_REALITY_BUBBLE:
      return "OBJECT_OUTSIDE_REALITY_BUBBLE";

    case SIMCONNECT_EXCEPTION_OBJECT_CONTAINER:
      return "OBJECT_CONTAINER";

    case SIMCONNECT_EXCEPTION_OBJECT_AI:
      return "OBJECT_AI";

    case SIMCONNECT_EXCEPTION_OBJECT_ATC:
      return "OBJECT_ATC";

    case SIMCONNECT_EXCEPTION_OBJECT_SCHEDULE:
      return "OBJECT_SCHEDULE";

    default:
      return "UNKNOWN";
  }
}
