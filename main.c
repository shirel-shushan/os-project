#include <stdio.h>
#include <stdlib.h>
#include "graph.h"

int main() {
    FILE* file = fopen("input.txt", "r");
    if (file == NULL) {
        printf("Error opening file\n");
        return 1;
    }

    int N, M;
    if (fscanf(file, "%d %d", &N, &M) != 2 || N <= 0 || M < 0) {
        printf("Invalid input\n");
        fclose(file);
        return 1;
    }

    Graph* g = createGraph(N);
    if (g == NULL) {
        fclose(file);
        return 1;
    }

    for (int i = 0; i < M; i++) {
        int src, dst, weight;
        if (fscanf(file, "%d %d %d", &src, &dst, &weight) != 3) {
            printf("Invalid input\n");
            freeGraph(g);
            fclose(file);
            return 1;
        }
        if (src < 0 || dst < 0 || weight < 0 || src >= N || dst >= N) {
            printf("Invalid input\n");
            freeGraph(g);
            fclose(file);
            return 1;
        }
        addEdge(g, src, dst, weight);
    }

//     int start, end;
//     if (fscanf(file, "%d %d", &start, &end) != 2 ||
//         start < 0 || end < 0 || start >= N || end >= N) {
//         printf("Invalid input\n");
//         freeGraph(g);
//         fclose(file);
//         return 1;
//     }
//     fclose(file);

//     // מקרה מקור == יעד
//    if (start == end) {
//     printf("%d\n0\n", start,0);
//     freeGraph(g);
//     return 0;
// }

//     int pathLen, totalWeight;
//     int* path = dijkstra(g, start, end, &pathLen, &totalWeight);

//     if (path == NULL) {
//         printf("No path found\n");
//     } else {
//         for (int i = 0; i < pathLen; i++) {
//             if (i < pathLen - 1)
//                 printf("%d -> ", path[i]);
//             else
//                 printf("%d", path[i]);
//         }
//         printf("\n%d\n", totalWeight);
//         free(path);
//     }

//     freeGraph(g);
//     return 0;
// }
printf("Weight 0->1: %d\n", g->graph[0][1]);
printf("Weight 0->2: %d\n", g->graph[0][2]);
printf("Weight 2->1: %d\n", g->graph[2][1]);
printf("Weight 4->5: %d\n", g->graph[4][5]);
printf("Weight 1->5: %d\n", g->graph[1][5]);
fclose(file);
    freeGraph(g);
    
    printf("Graph loaded and freed successfully!\n"); 
    return 0;
}