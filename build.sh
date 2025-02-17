gcc-14 -fsanitize=address -g -Wall -o client client.c -lraylib  -lczmq -lGL -lm -lpthread -ldl -lrt -lX11 -I/usr/include/uuid -luuid

#clang -fsanitize=address -g -o test test.c -lraylib  -lczmq -lGL -lm -lpthread -ldl -lrt -lX11
