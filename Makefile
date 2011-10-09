all: light-switcher microphone-monitor

light-switcher: light-switcher.c
	clang \
	  -I/Library/Frameworks/Phidget21.framework/Headers \
	  -g -O2 -framework Phidget21  -o $@ $<

microphone-monitor: microphone-monitor.cpp
	clang++ \
	  -g -O2 -framework AudioUnit  -o $@ $< \
	  -framework Carbon -framework CoreAudio
