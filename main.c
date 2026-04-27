#include <stdio.h>
#include <stdlib.h>

int main() {
    FILE *file = fopen("input.txt", "r");

    if (file == NULL) {
        printf("Error opening file\n");
        return 1;
    }

    int N, M;
    fscanf(file, "%d %d", &N, &M);

    printf("Number of vertices: %d\n", N);
    printf("Number of edges: %d\n", M);

    for (int i = 0; i < M; i++) {
        int src, dst, weight;
        fscanf(file, "%d %d %d", &src, &dst, &weight);
        printf("Edge %d: %d -> %d weight %d\n", i + 1, src, dst, weight);
    }

    int start, end;
    fscanf(file, "%d %d", &start, &end);

    printf("Dijkstra from %d to %d\n", start, end);

    fclose(file);
    return 0;
}