#include <stdlib.h>
#include <stdio.h>
#include "graph.h"

#define INF 1000000000
Graph* createGraph(int n) {
    if (n <= 0) {
        printf("Invalid input\n");
        return NULL;
    }

    Graph* g = malloc(sizeof(Graph));
    if (g == NULL)
        return NULL;

    g->n = n;
    g->graph = malloc(n * sizeof(int*));
    if (g->graph == NULL) {
        free(g);
        return NULL;
    }

    for (int i = 0; i < n; i++)
        g->graph[i] = NULL;

    for (int i = 0; i < n; i++) {
        g->graph[i] = malloc(n * sizeof(int));
        if (g->graph[i] == NULL) {
            freeGraph(g);
            return NULL;
        }
        for (int j = 0; j < n; j++)
            g->graph[i][j] = (i == j) ? 0 : INF;
    }

    return g;
}

void addEdge(Graph* g, int src, int dst, int weight) {
    if (g == NULL) {
        printf("Invalid input\n");
        return;
    }
    if (src < 0 || src >= g->n || dst < 0 || dst >= g->n) {
        printf("Invalid input\n");
        return;
    }
    if (weight < 0) {
        printf("Invalid input\n");
        return;
    }

    g->graph[src][dst] = weight;
}

void freeGraph(Graph* g) {
    if (g == NULL) return;
    if (g->graph != NULL) {
        for (int i = 0; i < g->n; i++)
            free(g->graph[i]);
        free(g->graph);
    }
    free(g);
}