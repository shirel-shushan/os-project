CC = gcc
CFLAGS = -Wall -g
RAYLIB_FLAGS = -lraylib -lm -ldl -lpthread -lGL -lX11

milestone1:
	$(CC) $(CFLAGS) main.c graph.c dijkstra.c -o dijkstra

milestone2:
	$(CC) $(CFLAGS) gui.c graph.c dijkstra.c -o sim $(RAYLIB_FLAGS)

milestone3:
	$(CC) $(CFLAGS) gui.c graph.c dijkstra.c -o sim $(RAYLIB_FLAGS)

milestone4:
	$(CC) $(CFLAGS) gui.c graph.c dijkstra.c -o sim $(RAYLIB_FLAGS)

clean:
	rm -f dijkstra sim main gui