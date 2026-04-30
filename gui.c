#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "graph.h"
#include "dijkstra.h" 
#include "raylib.h"

#define SCREEN_WIDTH 800
#define SCREEN_HEIGHT 600
#define NODE_RADIUS 20
// #define INF 1000000

// Animation states
typedef enum {
    STATE_IDLE,         
    STATE_MOVING,       
    STATE_WAITING_NODE, 
    STATE_FINISHED      
} AnimationState;

// Helper function to draw a directed edge with an arrow
void DrawDirectedEdge(Vector2 start, Vector2 end, int weight) {
    float dx = end.x - start.x;
    float dy = end.y - start.y;
    float length = sqrt(dx*dx + dy*dy);

    if (length == 0) return;

    float angle = atan2(dy, dx);
    Vector2 adjustedEnd;
    adjustedEnd.x = end.x - NODE_RADIUS * cos(angle);
    adjustedEnd.y = end.y - NODE_RADIUS * sin(angle);

    DrawLineEx(start, adjustedEnd, 2.5f, DARKGRAY);

    float arrowSize = 12.0f;
    Vector2 p1 = { adjustedEnd.x - arrowSize * cos(angle - PI/7),
                   adjustedEnd.y - arrowSize * sin(angle - PI/7) };
    Vector2 p2 = { adjustedEnd.x - arrowSize * cos(angle + PI/7),
                   adjustedEnd.y - arrowSize * sin(angle + PI/7) };
    DrawLineEx(adjustedEnd, p1, 2.5f, DARKGRAY);
    DrawLineEx(adjustedEnd, p2, 2.5f, DARKGRAY);

    int textX = start.x + dx * 0.45f;
    int textY = start.y + dy * 0.45f;
    const char* weightText = TextFormat("%d", weight);
    int textWidth = MeasureText(weightText, 20);
    
    DrawRectangle(textX - 3, textY - 3, textWidth + 6, 26, WHITE);
    DrawText(weightText, textX, textY, 20, MAROON);
}

int main(int argc, char* argv[]) {
    // If we forgot to pass the file name, print instructions
    if (argc < 2) {
        printf("Usage: %s <input_file>\n", argv[0]);
        return 1;
    }

    // Read data from the file given in the terminal
    FILE* file = fopen(argv[1], "r");
    if (file == NULL) {
        printf("Error: Could not open %s\n", argv[1]);
        return 1;
    }

    int N, M;
    if (fscanf(file, "%d %d", &N, &M) != 2 || N <= 0 || M < 0) {
        printf("Error: Invalid input format (N, M)\n");
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
            printf("Error: Invalid input format for edge %d\n", i+1);
            freeGraph(g); fclose(file); return 1;
        }
        addEdge(g, src, dst, weight);
    }
    
    int start_node, end_node;
    if (fscanf(file, "%d %d", &start_node, &end_node) != 2) {
        printf("Error: Invalid input format for path query\n");
        freeGraph(g); fclose(file); return 1;
    }
    fclose(file);

    // Run Dijkstra to get the shortest path
    int pathLen = 0, totalWeight = 0;
    int* path = dijkstra(g, start_node, end_node, &pathLen, &totalWeight);

    // GUI window setup
    InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "OS Project - Milestone 3");
    SetTargetFPS(60);

    Vector2* positions = malloc(N * sizeof(Vector2));
    float center_x = SCREEN_WIDTH / 2.0f;
    float center_y = SCREEN_HEIGHT / 2.0f;
    float layout_radius = 220.0f;

    for (int i = 0; i < N; i++) {
        float angle = i * (2.0f * PI / N);
        positions[i].x = center_x + layout_radius * cos(angle);
        positions[i].y = center_y + layout_radius * sin(angle);
    }

    // Variables for the animation
    AnimationState animState = STATE_IDLE;
    bool isPlaying = false;
    int currentPathIndex = 0;
    float stateStartTime = 0.0f;
    int currentEdgeWeight = 1;
    int jumpsCompleted = 0;
    
    Vector2 entityPos = {0};
    if (path != NULL && pathLen > 0) {
        entityPos = positions[path[0]]; // Put the circle on the starting node
    }

    Rectangle btnBounds = { SCREEN_WIDTH / 2.0f - 50, SCREEN_HEIGHT - 60, 100, 40 };

    while (!WindowShouldClose()) {
        
        // Check if the Play/Stop button was clicked
        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            if (CheckCollisionPointRec(GetMousePosition(), btnBounds)) {
                if (path != NULL && pathLen > 1) {
                    isPlaying = !isPlaying; 
                    
                    if (isPlaying && animState == STATE_IDLE) {
                        animState = STATE_MOVING;
                        currentEdgeWeight = g->graph[path[0]][path[1]];
                        stateStartTime = GetTime();
                        jumpsCompleted = 0;
                    }
                }
            }
        }

        // Animation time logic
        if (isPlaying && path != NULL) {
            float currentTime = GetTime();

            if (animState == STATE_MOVING) {
                // Jump every 300ms (0.3 seconds)
                if (currentTime - stateStartTime >= 0.3f) {
                    jumpsCompleted++;
                    stateStartTime = currentTime;

                    Vector2 startNode = positions[path[currentPathIndex]];
                    Vector2 endNode = positions[path[currentPathIndex+1]];

                    float progress = (float)jumpsCompleted / currentEdgeWeight;
                    if (progress > 1.0f) progress = 1.0f;

                    entityPos.x = startNode.x + (endNode.x - startNode.x) * progress;
                    entityPos.y = startNode.y + (endNode.y - startNode.y) * progress;

                    if (jumpsCompleted >= currentEdgeWeight) {
                        currentPathIndex++;
                        if (currentPathIndex >= pathLen - 1) {
                            animState = STATE_FINISHED;
                            isPlaying = false;
                        } else {
                            animState = STATE_WAITING_NODE;
                            stateStartTime = GetTime();
                        }
                    }
                }
            } 
            else if (animState == STATE_WAITING_NODE) {
                // Wait exactly 1 second at the node
                if (currentTime - stateStartTime >= 1.0f) {
                    animState = STATE_MOVING;
                    currentEdgeWeight = g->graph[path[currentPathIndex]][path[currentPathIndex+1]];
                    jumpsCompleted = 0;
                    stateStartTime = GetTime();
                }
            }
        }

        // Draw everything on the screen
        BeginDrawing();
        ClearBackground(RAYWHITE);

        for (int i = 0; i < N; i++) {
            for (int j = 0; j < N; j++) {
                int w = g->graph[i][j];
                if (w > 0 && w < INF) {
                    DrawDirectedEdge(positions[i], positions[j], w);
                }
            }
        }

        for (int i = 0; i < N; i++) {
            DrawCircleV(positions[i], NODE_RADIUS, SKYBLUE);
            DrawCircleLines(positions[i].x, positions[i].y, NODE_RADIUS, BLUE);
            const char* idText = TextFormat("%d", i);
            int textWidth = MeasureText(idText, 20);
            DrawText(idText, positions[i].x - textWidth/2, positions[i].y - 8, 20, DARKBLUE);
        }

        // Draw the moving entity (orange circle)
        if (path != NULL && pathLen > 0) {
            DrawCircleV(entityPos, NODE_RADIUS * 0.8f, ORANGE);
            DrawCircleLines(entityPos.x, entityPos.y, NODE_RADIUS * 0.8f, RED);
        }

        // Draw the Play/Stop button
        DrawRectangleRec(btnBounds, isPlaying ? RED : LIME);
        DrawRectangleLinesEx(btnBounds, 2, DARKGRAY);
        const char* btnText = isPlaying ? "STOP" : "PLAY";
        int btnTextWidth = MeasureText(btnText, 20);
        DrawText(btnText, btnBounds.x + (btnBounds.width - btnTextWidth) / 2, btnBounds.y + 10, 20, DARKGRAY);

        // Draw the finished message
        if (animState == STATE_FINISHED) {
            const char* msg = "DESTINATION REACHED!";
            int msgWidth = MeasureText(msg, 30);
            DrawText(msg, (SCREEN_WIDTH - msgWidth) / 2, 30, 30, DARKGREEN);
        }

        EndDrawing();
    }

    if(positions) free(positions);
    if(path) free(path);
    CloseWindow();
    freeGraph(g);

    return 0;
}