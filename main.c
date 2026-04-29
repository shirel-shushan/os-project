#include <stdio.h>
#include <stdlib.h>
#include "graph.h"
#include "dijkstra.h"

int main(int argc, char* argv[]) {

    // make sure the user provided a file name
    if (argc != 2) {
        printf("Usage: %s <input_file>\n", argv[0]);
        return 1;
    }

    // open the input file
    FILE* file = fopen(argv[1], "r");
    if (file == NULL) {
        printf("Error opening file\n");
        return 1;
    }

    // read number of nodes and edges
    int N, M;
    if (fscanf(file, "%d %d", &N, &M) != 2 || N <= 0 || M < 0) {
        printf("Invalid input\n");
        fclose(file);
        return 1;
    }

    // create the graph
    Graph* g = createGraph(N);
    if (g == NULL) {
        fclose(file);
        return 1;
    }

    // read each edge and add it to the graph
    for (int i = 0; i < M; i++) {
        int src, dst, weight;
        if (fscanf(file, "%d %d %d", &src, &dst, &weight) != 3) {
            printf("Invalid input\n");
            freeGraph(g);
            fclose(file);
            return 1;
        }
        // check for invalid values
        if (src < 0 || dst < 0 || weight < 0 || src >= N || dst >= N) {
            printf("Invalid input\n");
            freeGraph(g);
            fclose(file);
            return 1;
        }
        addEdge(g, src, dst, weight);
    }

    // read the source and destination nodes for dijkstra
    int start, end;
    if (fscanf(file, "%d %d", &start, &end) != 2 ||
        start < 0 || end < 0 || start >= N || end >= N) {
        printf("Invalid input\n");
        freeGraph(g);
        fclose(file);
        return 1;
    }
    fclose(file);

    // if source equals destination - print and exit
    if (start == end) {
        printf("%d %d\n", start, end);
        freeGraph(g);
        return 0;
    }

    // run dijkstra and get the shortest path
    int pathLen, totalWeight;
    int* path = dijkstra(g, start, end, &pathLen, &totalWeight);

    // print the result
    if (path == NULL) {
        printf("No path found\n");
    } else {
        for (int i = 0; i < pathLen; i++) {
            if (i < pathLen - 1)
                printf("%d -> ", path[i]);
            else
                printf("%d", path[i]);
        }
        printf("\n%d\n", totalWeight);
        free(path);
    }

    freeGraph(g);
    return 0;
}