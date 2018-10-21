all:
	g++ -O2 -g -std=c++11 audioResample.cpp  test.cpp -D__STDC_CONSTANT_MACROS -I/usr/local/Cellar/ffmpeg/3.4.2/include  -I../../../ffmpeg/build/include/ -L/usr/local/Cellar/ffmpeg/3.4.2/lib -L../../../ffmpeg/build/lib  -lavfilter -lavformat -lavcodec -lswscale -lswresample -lavutil -lglog -lz -lpthread -lm

clean:
	rm -rf *.o a.out*

recomp: clean all


# GLOG_logtostderr=1 ./a.out
