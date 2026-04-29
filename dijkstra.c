#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <stdbool.h>

#define MAX_NODES 100

// Helper function to print the path using the parent array
void printPath(int parent[], int j) {
    if (parent[j] == -1) {
        printf("%d", j);
        return;
    }
    printPath(parent, parent[j]);
    printf(" -> %d", j);
}

// Main Dijkstra function
void runDijkstra(int graph[MAX_NODES][MAX_NODES], int numNodes, int startNode, int endNode) {
    int dist[MAX_NODES];
    int parent[MAX_NODES];
    bool visited[MAX_NODES];

    for (int i = 0; i < numNodes; i++) {
        dist[i] = INT_MAX;
        parent[i] = -1;
        visited[i] = false;
    }

    dist[startNode] = 0;

    for (int count = 0; count < numNodes - 1; count++) {
        int min = INT_MAX, u = -1;

        for (int v = 0; v < numNodes; v++) {
            if (!visited[v] && dist[v] <= min) {
                min = dist[v];
                u = v;
            }
        }

        if (u == -1) break;
        visited[u] = true;

        for (int v = 0; v < numNodes; v++) {
            if (!visited[v] && graph[u][v] != 0 && dist[u] != INT_MAX 
                && dist[u] + graph[u][v] < dist[v]) {
                dist[v] = dist[u] + graph[u][v];
                parent[v] = u;
            }
        }
    }

    // Output formatting as required
    if (startNode == endNode) {
        printf("%d\n", startNode);
    } else if (dist[endNode] == INT_MAX) {
        printf("No path found\n");
    } else {
        printPath(parent, endNode);
        printf("\n");
    }
}