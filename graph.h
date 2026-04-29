#ifndef GRAPH_H
#define GRAPH_H
#define INF 1000000000

typedef struct {
    int **graph;
    int n;
} Graph;

Graph* createGraph(int n);
void addEdge(Graph* g, int src, int dst, int weight);
void freeGraph(Graph* g);

#endif

