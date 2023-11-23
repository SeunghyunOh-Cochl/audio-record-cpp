# How to record Audio on Linux ?

## Pulseaudio record
### How to set device?

1. check device name and which is recording device
```shell
user@user:~/$ arecord -l
**** List of CAPTURE Hardware Devices ****
card 0: Device [USB Audio Device], device 0: USB Audio [USB Audio]
  Subdevices: 1/1
  Subdevice #0: subdevice #0
card 2: OnBoard [USB Audio OnBoard], device 0: USB Audio [USB Audio]
  Subdevices: 1/1
  Subdevice #0: subdevice #0
card 2: OnBoard [USB Audio OnBoard], device 1: USB Audio [USB Audio #1]
  Subdevices: 1/1
  Subdevice #0: subdevice #0
```

```shell
# plughw:[sound-card-number,device-number]
arecord -t wav -c 1 -D plughw:2,0 -f S16_LE -d 5 -r 16000 test.wav
```

2. In my case, USB Audio Device is recording device. Then find out check device name in pulseaudio

``user
user:pacmd list-sources
...
    index: 7
	name: <alsa_input.usb-GeneralPlus_USB_Audio_Device-00.analog-mono>
	driver: <module-alsa-card.c>
	flags: HARDWARE HW_MUTE_CTRL HW_VOLUME_CTRL DECIBEL_VOLUME LATENCY DYNAMIC_LATENCY
	state: SUSPENDED
	suspend cause: IDLE
	priority: 9049
	volume: mono: 52058 /  79% / -6.00 dB
	        balance 0.00
	base volume: 18471 /  28% / -33.00 dB
	volume steps: 65537
	muted: no
	current latency: 0.00 ms
	max rewind: 0 KiB
	sample spec: s16le 1ch 44100Hz
	channel map: mono
	             Mono
	used by: 0
	linked by: 0
	configured latency: 0.00 ms; range is 0.50 .. 2000.00 ms
	card: 0 <alsa_card.usb-GeneralPlus_USB_Audio_Device-00>
	module: 13
	properties:
		alsa.resolution_bits = "16"
		device.api = "alsa"
		device.class = "sound"
		alsa.class = "generic"
		alsa.subclass = "generic-mix"
		alsa.name = "USB Audio"
		alsa.id = "USB Audio"
		alsa.subdevice = "0"
		alsa.subdevice_name = "subdevice #0"
		alsa.device = "0"
		alsa.card = "0"
		alsa.card_name = "USB Audio Device"
		alsa.long_card_name = "GeneralPlus USB Audio Device at usb-ff540000.usb-1.2, full speed"
		alsa.driver_name = "snd_usb_audio"
		device.bus_path = "platform-ff540000.usb-usb-0:1.2:1.0"
		sysfs.path = "/devices/platform/ff540000.usb/usb1/1-1/1-1.2/1-1.2:1.0/sound/card0"
		udev.id = "usb-GeneralPlus_USB_Audio_Device-00"
		device.bus = "usb"
		device.vendor.id = "1b3f"
		device.vendor.name = "Generalplus Technology Inc."
		device.product.id = "2008"
		device.product.name = "USB Audio Device"
		device.serial = "GeneralPlus_USB_Audio_Device"
		device.string = "hw:0"
		device.buffering.buffer_size = "176400"
		device.buffering.fragment_size = "88200"
		device.access_mode = "mmap+timer"
		device.profile.name = "analog-mono"
		device.profile.description = "Analog Mono"
		device.description = "USB Audio Device Analog Mono"
		alsa.mixer_name = "USB Mixer"
		alsa.components = "USB1b3f:2008"
		module-udev-detect.discovered = "1"
		device.icon_name = "audio-card-usb"
	ports:
		analog-input-mic: Microphone (priority 8700, latency offset 0 usec, available: unknown)
			properties:
				device.icon_name = "audio-input-microphone"
	active port: <analog-input-mic>

```

3. Then add device name to "const char *dev (/*< Sink (resp. source) name, or NULL for default */) parameters in pa_simple_new.

```cpp
std::string device = "alsa_input.usb-GeneralPlus_USB_Audio_Device-00.analog-mono";
if (!(s = pa_simple_new(NULL, argv[0], PA_STREAM_RECORD, device.c_str(), "record", &ss,
                        NULL, &buf_attr, &error))) {
  fprintf(stderr, __FILE__ ": pa_simple_new() failed: %s\n",
          pa_strerror(error));
  finish(s);
  return -1;
}
```

4. Good!

### Package
sudo apt install -y libpulse-dev

### Build
g++ pulseaudio-record-example.cc -o pulseaudio-record-example -lm -std=c++11 -ldl -lstdc++ -lpulse -lpulse-simple

## Pulseaudio stream
### Package
sudo apt install -y libpulse-dev

### Build
g++ -fopenmp pulseaudio-stream-example.cc -o pulseaudio-stream-example -lm -std=c++11 -ldl -lstdc++ -lpulse -lpulse-simple

## ALSA record
### Package
sudo apt-get install -y libasound-dev

### Build
g++ alsa-record-example.cc -I/usr/include/  -o alsa-record-example -lm -ldl -lasound 

## Portaudio record
### Package
apt install -y portaudio19-dev 

### Build
g++ portaudio-record-exmple.cc -I/usr/include/  -o portaudio-record-exmple -lm -ldl -lportaudio

## TODO
- pipewire
- soundfile format: soundfile example
