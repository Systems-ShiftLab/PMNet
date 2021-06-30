all: serverAdaptor

serverAdaptor: serverAdaptor.cpp ../redisAdaptorCommon/common.h ../socketHandler/socketHandler.c ../socketHandler/socketHandler.h
	g++ serverAdaptor.cpp ../socketHandler/socketHandler.c -fpermissive -lpthread -O3 -o serverAdaptor

clean:
	-rm serverAdaptor
