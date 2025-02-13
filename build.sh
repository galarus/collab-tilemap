gcc-14 -fanalyzer -g -Wall -o client client.c -lraylib  -lczmq -lGL -lm -lpthread -ldl -lrt -lX11

#clang -g -Xclang -analyze  -o client client.c -lraylib  -lczmq -lGL -lm -lpthread -ldl -lrt -lX11
