This is the code for the sensor network at The Foundry

It consists of several Arduinos with Xbee radios with sensors attached
(currently just temperature sensors). These send data to a coordinator
which is attached through USB to a Linux host. The host runs a python
script which gathers the data and periodically sends it to:

A data stream at data.sparkfun.com:

Use this url: https://data.sparkfun.com/streams/WGqzXY7GDVS7wv3vzlVV

Twitter @TheFoundryTemp

The Arduino code is in ArduinoFoundrySensors.

The python code for the linux coodinator is in SensorHostCoordinator.
