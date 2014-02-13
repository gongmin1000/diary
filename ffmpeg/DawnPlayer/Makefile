INCLUDE_PATH=-I/home/dongrui/program/ffmpeg/include -I/home/dongrui/program/sdl2/include

LINKER_FLAGS=-L/home/dongrui/program/ffmpeg/lib -L/home/dongrui/program/sdl2/lib -lavformat -lavcodec  -lavdevice  -lavfilter  -lavutil  -lswresample  -lswscale -lz -lpthread  -lSDL2 /usr/lib/x86_64-linux-gnu/libpulse-simple.so /usr/lib/x86_64-linux-gnu/libpulse.so

CFLAGS=-D__STDC_CONSTANT_MACROS 

DawnPlayer:DawnPlayer.cpp
	g++ -g -o DawnPlayer DawnPlayer.cpp $(CFLAGS) $(INCLUDE_PATH)  $(LINKER_FLAGS)


clean:
	rm -rf DawnPlayer.o DawnPlayer 
