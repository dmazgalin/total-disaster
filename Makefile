SOURCES=main.cpp H264_Decoder.cpp YUV420P_Player.cpp AudioDecoder.cpp
OBJECTS=$(SOURCES:.c=.o)

DEBUG=-g

CC=clang++

CFLAGS += $(shell pkg-config --cflags glfw3 portaudio-2.0) -I./include -stdlib=libc++ -std=c++0x $(DEBUG) -I/usr/local/include
LDFLAGS += $(shell pkg-config --libs glfw3 portaudio-2.0) -stdlib=libc++ -std=c++0x -lpng -lavcodec -lavutil -lavformat -lavresample -ljack

ifeq ($(shell uname -s), Darwin)
		LDFLAGS += -framework OpenGL -framework Cocoa
		CFLAGS +=
else
		LDFLAGS += 
endif


all: td

td: $(OBJECTS) Makefile
	$(CC) $(OBJECTS) $(CFLAGS) $(LDFLAGS) -o $@

clean:
	@rm ./td || echo "Nothing to do"
