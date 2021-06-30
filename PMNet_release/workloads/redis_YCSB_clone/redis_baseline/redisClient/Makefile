all: redisClient

redisClient: redisClient.cpp ../redisAdaptorCommon/common.h ../socketHandler/socketHandler.c ../socketHandler/socketHandler.h
	g++ redisClient.cpp ../socketHandler/socketHandler.c -O3 -o redisClient

clean:
	-rm redisClient
