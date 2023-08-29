# How to record Audio on Linux ?

## Pulseaudio record
### Package
apt install pulseaudio

### Build
g++ pulseaudio-record-example.cc -o pulseaudio-record-example -lm -std=c++11 -ldl -lstdc++ -lpulse -lpulse-simple

## Pulseaudio stream
### Package
apt install pulseaudio

### Build
g++ -fopenmp pulseaudio-stream-example.cc -o pulseaudio-stream-example -lm -std=c++11 -ldl -lstdc++ -lpulse -lpulse-simple

## ALSA record
### Package
sudo apt-get install libasound-dev

### Build
g++ alsa-record-example.cc -I/usr/include/  -o alsa-record-example -lm -ldl -lasound 

## Portaudio record
### Package
apt install portaudio19-dev 

### Build
g++ portaudio-record-exmple.cc -I/usr/include/  -o portaudio-record-exmple -lm -ldl -lportaudio

## TODO
- pipewire
- soundfile format: soundfile example