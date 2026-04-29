#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "graph.h"
#include "dijkstra.h"
#include "raylib.h"

#define SCREEN_WIDTH 800
#define SCREEN_HEIGHT 600
#define NODE_RADIUS 20

void DrawDirectedEdge(Vector2 start, Vector2 end, int weight) {
    float dx = end.x - start.x;
    float dy = end.y - start.y;
    float length = sqrt(dx*dx + dy*dy);
    if (length == 0) return;

    float angle = atan2(dy, dx);

    // stop arrow at edge of node
    Vector2 adjustedEnd;
    adjustedEnd.x = end.x - NODE_RADIUS * cos(angle);
    adjustedEnd.y = end.y - NODE_RADIUS * sin(angle);

    // draw main line
    DrawLineEx(start, adjustedEnd, 2.5f, DARKGRAY);

    // draw arrow head
    float arrowSize = 12.0f;
    Vector2 p1 = { adjustedEnd.x - arrowSize * cos(angle - PI/7),
                   adjustedEnd.y - arrowSize * sin(angle - PI/7) };
    Vector2 p2 = { adjustedEnd.x - arrowSize * cos(angle + PI/7),
                   adjustedEnd.y - arrowSize * sin(angle + PI/7) };
    DrawLineEx(adjustedEnd, p1, 2.5f, DARKGRAY);
    DrawLineEx(adjustedEnd, p2, 2.5f, DARKGRAY);

    // draw weight label in the middle of the edge
    int textX = start.x + dx * 0.45f;
    int textY = start.y + dy * 0.45f;
    const char* weightText = TextFormat("%d", weight);
    int textWidth = MeasureText(weightText, 20);
    DrawRectangle(textX - 3, textY - 3, textWidth + 6, 26, WHITE);
    DrawText(weightText, textX, textY, 20, MAROON);
}

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
            freeGraph(g); fclose(file); return 1;
        }
        if (src < 0 || dst < 0 || weight < 0 || src >= N || dst >= N) {
            printf("Invalid input\n");
            freeGraph(g); fclose(file); return 1;
        }
        addEdge(g, src, dst, weight);
    }

    // read source and destination for dijkstra
    int start_node, end_node;
    if (fscanf(file, "%d %d", &start_node, &end_node) != 2 ||
        start_node < 0 || end_node < 0 || start_node >= N || end_node >= N) {
        printf("Invalid input\n");
        freeGraph(g); fclose(file); return 1;
    }
    fclose(file);

    // run dijkstra and print result to terminal
    int pathLen, totalWeight;
    int* path = NULL;

    if (start_node == end_node) {
        printf("%d %d\n", start_node, end_node);
    } else {
        path = dijkstra(g, start_node, end_node, &pathLen, &totalWeight);
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
        }
    }

    // open the GUI window
    InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "OS Project - Graph Simulation");
    SetTargetFPS(60);

    // calculate node positions in a circle layout
    Vector2* positions = malloc(N * sizeof(Vector2));
    if (positions == NULL) {
        CloseWindow(); freeGraph(g); free(path); return 1;
    }

    float center_x = SCREEN_WIDTH / 2.0f;
    float center_y = SCREEN_HEIGHT / 2.0f;
    float layout_radius = 220.0f;

    for (int i = 0; i < N; i++) {
        float angle = i * (2.0f * PI / N);
        positions[i].x = center_x + layout_radius * cos(angle);
        positions[i].y = center_y + layout_radius * sin(angle);
    }

    // main drawing loop
    while (!WindowShouldClose()) {
        BeginDrawing();
        ClearBackground(RAYWHITE);

        // draw edges first (behind nodes)
        for (int i = 0; i < N; i++)
            for (int j = 0; j < N; j++) {
                int w = g->graph[i][j];
                if (w > 0 && w < INF)
                    DrawDirectedEdge(positions[i], positions[j], w);
            }

        // draw nodes
        for (int i = 0; i < N; i++) {
            DrawCircleV(positions[i], NODE_RADIUS, SKYBLUE);
            DrawCircleLines(positions[i].x, positions[i].y, NODE_RADIUS, BLUE);

            // center the number inside the circle
            const char* idText = TextFormat("%d", i);
            int fontSize = 20;
            int textWidth = MeasureText(idText, fontSize);
            DrawText(idText,
                     positions[i].x - textWidth / 2,
                     positions[i].y - fontSize / 2 + 2,
                     fontSize, DARKBLUE);
        }

        EndDrawing();
    }

    // clean up memory
    free(positions);
    free(path);
    CloseWindow();
    freeGraph(g);
    return 0;
}