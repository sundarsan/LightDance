CC := clang
CFLAGS := \
	-g -O2 -Wall -Wextra \
	-Wno-unused-parameter -Wno-deprecated-declarations -Wno-unused-function
CPPFLAGS := \
	-I/Library/Frameworks/Phidget21.framework/Headers \
	-I/opt/local/include \

MICROPHONE_OBJS := microphone-monitor.o \
	AudioMonitor.o MusicMonitor.o LightController.o \
	SimLightController.o

all: light-switcher microphone-monitor

light-switcher: light-switcher.o
	clang \
	   \
	  -framework Phidget21  -o $@ $<

microphone-monitor: $(MICROPHONE_OBJS)
	clang++ \
	  -g -O2 -framework AudioUnit  -o $@ $(MICROPHONE_OBJS) \
	  -Wno-deprecated-declarations \
	  -framework Carbon -framework CoreAudio \
	  -framework Phidget21 \
	  -L/opt/local/lib -laubio \
	  -framework OpenGL -framework GLUT

%.o: %.cpp Makefile
	$(CC) -c -o $@ $< $(CFLAGS) $(CPPFLAGS)

%.o: %.c Makefile
	$(CC) -c -o $@ $< $(CFLAGS) $(CPPFLAGS)
