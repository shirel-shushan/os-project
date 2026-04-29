// #include <stdio.h>
// #include <stdlib.h>
// #include <limits.h>
// #include <stdbool.h>

// #define MAX_NODES 100

// // Helper function to print the path using the parent array
// void printPath(int parent[], int j) {
//     if (parent[j] == -1) {
//         printf("%d", j);
//         return;
//     }
//     printPath(parent, parent[j]);
//     printf(" -> %d", j);
// }

// // Main Dijkstra function
// void runDijkstra(int graph[MAX_NODES][MAX_NODES], int numNodes, int startNode, int endNode) {
//     int dist[MAX_NODES];
//     int parent[MAX_NODES];
//     bool visited[MAX_NODES];

//     for (int i = 0; i < numNodes; i++) {
//         dist[i] = INT_MAX;
//         parent[i] = -1;
//         visited[i] = false;
//     }

//     dist[startNode] = 0;

//     for (int count = 0; count < numNodes - 1; count++) {
//         int min = INT_MAX, u = -1;

//         for (int v = 0; v < numNodes; v++) {
//             if (!visited[v] && dist[v] <= min) {
//                 min = dist[v];
//                 u = v;
//             }
//         }

//         if (u == -1) break;
//         visited[u] = true;

//         for (int v = 0; v < numNodes; v++) {
//             if (!visited[v] && graph[u][v] != 0 && dist[u] != INT_MAX 
//                 && dist[u] + graph[u][v] < dist[v]) {
//                 dist[v] = dist[u] + graph[u][v];
//                 parent[v] = u;
//             }
//         }
//     }

//     // Output formatting as required
//     if (startNode == endNode) {
//         printf("%d\n", startNode);
//     } else if (dist[endNode] == INT_MAX) {
//         printf("No path found\n");
//     } else {
//         printPath(parent, endNode);
//         printf("\n");
//     }
// }
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include "dijkstra.h"

#define INF 1000000000

int* dijkstra(Graph* g, int start, int end, int* pathLen, int* totalWeight) {
    int n = g->n;

    // arrays to track distance, previous node, and visited status
    int dist[n];
    int parent[n];
    bool visited[n];

    // set all distances to infinity, nothing visited yet
    for (int i = 0; i < n; i++) {
        dist[i]    = INF;
        parent[i]  = -1;
        visited[i] = false;
    }

    // distance from start to itself is 0
    dist[start] = 0;

    // main loop - runs n-1 times
    for (int count = 0; count < n - 1; count++) {

        // find the unvisited node with the smallest distance
        int min = INF, u = -1;
        for (int v = 0; v < n; v++)
            if (!visited[v] && dist[v] <= min) {
                min = dist[v];
                u = v;
            }

        // no reachable node found - graph is disconnected
        if (u == -1) break;

        // mark node as visited
        visited[u] = true;

        // update distances to neighbors of u
        for (int v = 0; v < n; v++) {
            if (!visited[v] &&
                g->graph[u][v] < INF &&                   // edge exists
                dist[u] + g->graph[u][v] < dist[v]) {     // found shorter path
                dist[v]   = dist[u] + g->graph[u][v];
                parent[v] = u;  // remember where we came from
            }
        }
    }

    // if distance to end is still infinity - no path exists
    if (dist[end] == INF)
        return NULL;

    // build the path backwards from end to start
    int temp[n];
    int len = 0;
    int current = end;
    while (current != -1) {
        temp[len++] = current;
        current = parent[current];
    }

    // reverse the path so it goes from start to end
    int* path = malloc(len * sizeof(int));
    if (path == NULL) return NULL;

    for (int i = 0; i < len; i++)
        path[i] = temp[len - i - 1];

    // set output values
    *pathLen     = len;
    *totalWeight = dist[end];

    return path;
}