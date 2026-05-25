#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#include "graph.h"
#include "dijkstra.h"
#include "raylib.h"

#define SCREEN_WIDTH 800
#define SCREEN_HEIGHT 600
#define NODE_RADIUS 20
#define JUMP_TIME 0.3f
#define WAIT_TIME 1.0f
#define MAX_TRAVELERS 10
#define INF 1000000000

typedef struct {
    int* path;
    int pathLen;
    int totalWeight;
    int srcNode;
    int dstNode;
    Color color;
    pid_t pid;

    float x, y;
    int pathIndex;
    int jumpsDone;
    int totalJumps;
    float timer;
    bool waiting;
    bool done;
    bool signalSent;
} Traveler;

static Color travelerColors[] = {
    RED, GREEN, PURPLE, ORANGE, PINK,
    YELLOW, LIME, VIOLET, BROWN, GOLD
};

static void skipCommentLines(FILE* file) {
    char line[256];
    long pos;

    while ((pos = ftell(file)), fgets(line, sizeof(line), file)) {
        int i = 0;

        while (line[i] == ' ' || line[i] == '\t') {
            i++;
        }

        if (line[i] != '#' && line[i] != '\n' && line[i] != '\r' && line[i] != '\0') {
            fseek(file, pos, SEEK_SET);
            return;
        }
    }
}

void DrawDirectedEdge(Vector2 start, Vector2 end, int weight) {
    float dx = end.x - start.x;
    float dy = end.y - start.y;
    float length = sqrtf(dx * dx + dy * dy);

    if (length == 0) return;

    float angle = atan2f(dy, dx);

    Vector2 adjustedEnd = {
        end.x - NODE_RADIUS * cosf(angle),
        end.y - NODE_RADIUS * sinf(angle)
    };

    DrawLineEx(start, adjustedEnd, 2.5f, DARKGRAY);

    float arrowSize = 12.0f;

    Vector2 p1 = {
        adjustedEnd.x - arrowSize * cosf(angle - PI / 7),
        adjustedEnd.y - arrowSize * sinf(angle - PI / 7)
    };

    Vector2 p2 = {
        adjustedEnd.x - arrowSize * cosf(angle + PI / 7),
        adjustedEnd.y - arrowSize * sinf(angle + PI / 7)
    };

    DrawLineEx(adjustedEnd, p1, 2.5f, DARKGRAY);
    DrawLineEx(adjustedEnd, p2, 2.5f, DARKGRAY);

    int textX = (int)(start.x + dx * 0.45f);
    int textY = (int)(start.y + dy * 0.45f);

    const char* weightText = TextFormat("%d", weight);
    int textWidth = MeasureText(weightText, 18);

    DrawRectangle(textX - 3, textY - 3, textWidth + 6, 24, WHITE);
    DrawText(weightText, textX, textY, 18, MAROON);
}

int main(int argc, char* argv[]) {
    if (argc != 2) {
        printf("Usage: %s <input_file>\n", argv[0]);
        return 1;
    }

    FILE* file = fopen(argv[1], "r");

    if (file == NULL) {
        printf("Error opening file\n");
        return 1;
    }

    skipCommentLines(file);

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

    skipCommentLines(file);

    int numTravelers;

    if (fscanf(file, "%d", &numTravelers) != 1 ||
        numTravelers <= 0 ||
        numTravelers > MAX_TRAVELERS) {
        printf("Invalid input\n");
        freeGraph(g);
        fclose(file);
        return 1;
    }

    Traveler travelers[MAX_TRAVELERS];

    for (int i = 0; i < numTravelers; i++) {
        int src, dst;

        if (fscanf(file, "%d %d", &src, &dst) != 2 ||
            src < 0 || dst < 0 || src >= N || dst >= N) {
            printf("Invalid input\n");
            freeGraph(g);
            fclose(file);
            return 1;
        }

        travelers[i].srcNode = src;
        travelers[i].dstNode = dst;
        travelers[i].color = travelerColors[i % 10];
        travelers[i].pid = -1;
        travelers[i].done = false;
        travelers[i].signalSent = false;
        travelers[i].waiting = false;
        travelers[i].timer = 0;
        travelers[i].pathIndex = 0;
        travelers[i].jumpsDone = 0;
        travelers[i].totalJumps = 0;
        travelers[i].x = 0;
        travelers[i].y = 0;

        if (src == dst) {
            travelers[i].path = malloc(sizeof(int));
            if (travelers[i].path == NULL) {
                printf("Memory allocation failed\n");
                freeGraph(g);
                fclose(file);
                return 1;
            }

            travelers[i].path[0] = src;
            travelers[i].pathLen = 1;
            travelers[i].totalWeight = 0;
            travelers[i].done = true;
        } else {
            int pathLen = 0;
            int totalWeight = 0;

            travelers[i].path = dijkstra(g, src, dst, &pathLen, &totalWeight);

            if (travelers[i].path == NULL) {
                printf("No path found for traveler %d\n", i);
                travelers[i].pathLen = 0;
                travelers[i].totalWeight = 0;
                travelers[i].done = true;
            } else {
                travelers[i].pathLen = pathLen;
                travelers[i].totalWeight = totalWeight;
            }
        }
    }

    fclose(file);

    for (int i = 0; i < numTravelers; i++) {
        pid_t pid = fork();

        if (pid < 0) {
            printf("Fork failed\n");

            for (int j = 0; j < i; j++) {
                if (travelers[j].pid > 0) {
                    kill(travelers[j].pid, SIGTERM);
                }
            }

            for (int j = 0; j < numTravelers; j++) {
                free(travelers[j].path);
            }

            freeGraph(g);
            return 1;
        }

        if (pid == 0) {
            printf("[%d] started\n", getpid());

            while (1) {
                sleep(1);
            }

            exit(0);
        }

        travelers[i].pid = pid;
    }

    InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "Milestone 4 - Multiple Travelers");
    SetTargetFPS(60);

    Vector2 positions[N];

    float centerX = SCREEN_WIDTH / 2.0f;
    float centerY = SCREEN_HEIGHT / 2.0f;
    float radius = 220.0f;

    for (int i = 0; i < N; i++) {
        float angle = i * (2.0f * PI / N);
        positions[i].x = centerX + radius * cosf(angle);
        positions[i].y = centerY + radius * sinf(angle);
    }

    for (int i = 0; i < numTravelers; i++) {
        if (travelers[i].pathLen > 0) {
            travelers[i].x = positions[travelers[i].srcNode].x;
            travelers[i].y = positions[travelers[i].srcNode].y;

            if (travelers[i].pathLen > 1) {
                int cur = travelers[i].path[0];
                int next = travelers[i].path[1];
                travelers[i].totalJumps = g->graph[cur][next];
            }
        }
    }

    bool playing = false;
    bool allDone = false;

    Rectangle playButton = {20, 20, 100, 40};

    while (!WindowShouldClose()) {
        float dt = GetFrameTime();

        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            if (CheckCollisionPointRec(GetMousePosition(), playButton)) {
                playing = !playing;
            }
        }

        if (playing && !allDone) {
            for (int i = 0; i < numTravelers; i++) {
                Traveler* t = &travelers[i];

                if (t->done || t->pathLen <= 1) {
                    continue;
                }

                int cur = t->path[t->pathIndex];
                int next = t->path[t->pathIndex + 1];

                if (t->totalJumps <= 0 || t->totalJumps >= INF) {
                    t->done = true;
                    continue;
                }

                if (t->waiting) {
                    t->timer += dt;

                    if (t->timer >= WAIT_TIME) {
                        t->timer = 0;
                        t->waiting = false;
                        t->jumpsDone = 0;

                        cur = t->path[t->pathIndex];
                        next = t->path[t->pathIndex + 1];
                        t->totalJumps = g->graph[cur][next];
                    }

                    continue;
                }

                t->timer += dt;

                while (t->timer >= JUMP_TIME && !t->done) {
                    t->timer -= JUMP_TIME;
                    t->jumpsDone++;

                    if (t->jumpsDone >= t->totalJumps) {
                        t->pathIndex++;

                        t->x = positions[next].x;
                        t->y = positions[next].y;

                        if (t->pathIndex >= t->pathLen - 1) {
                            t->done = true;

                            if (!t->signalSent && t->pid > 0) {
                                kill(t->pid, SIGTERM);
                                t->signalSent = true;
                            }
                        } else {
                            t->waiting = true;
                            t->timer = 0;
                        }
                    }
                }

                if (!t->done && !t->waiting) {
                    cur = t->path[t->pathIndex];
                    next = t->path[t->pathIndex + 1];

                    float progress = (t->jumpsDone + t->timer / JUMP_TIME) / t->totalJumps;

                    if (progress > 1.0f) {
                        progress = 1.0f;
                    }

                    t->x = positions[cur].x + (positions[next].x - positions[cur].x) * progress;
                    t->y = positions[cur].y + (positions[next].y - positions[cur].y) * progress;
                }
            }

            allDone = true;

            for (int i = 0; i < numTravelers; i++) {
                if (!travelers[i].done) {
                    allDone = false;
                    break;
                }
            }
        }

        BeginDrawing();
        ClearBackground(RAYWHITE);

        for (int i = 0; i < N; i++) {
            for (int j = 0; j < N; j++) {
                int weight = g->graph[i][j];

                if (weight > 0 && weight < INF) {
                    DrawDirectedEdge(positions[i], positions[j], weight);
                }
            }
        }

        for (int i = 0; i < N; i++) {
            DrawCircleV(positions[i], NODE_RADIUS, SKYBLUE);
            DrawCircleLines((int)positions[i].x, (int)positions[i].y, NODE_RADIUS, BLUE);

            const char* text = TextFormat("%d", i);
            int fontSize = 20;
            int textWidth = MeasureText(text, fontSize);

            DrawText(
                text,
                (int)(positions[i].x - textWidth / 2),
                (int)(positions[i].y - fontSize / 2 + 2),
                fontSize,
                DARKBLUE
            );
        }

        for (int i = 0; i < numTravelers; i++) {
            if (travelers[i].pathLen > 0) {
                Vector2 travelerPosition = {travelers[i].x, travelers[i].y};

                DrawCircleV(travelerPosition, 12, travelers[i].color);
                DrawText(
                    TextFormat("%d", i),
                    (int)(travelers[i].x - 4),
                    (int)(travelers[i].y - 8),
                    14,
                    WHITE
                );
            }
        }

        DrawRectangleRec(playButton, LIGHTGRAY);
        DrawRectangleLinesEx(playButton, 2, DARKGRAY);

        const char* buttonText = playing ? "STOP" : "PLAY";
        int buttonTextWidth = MeasureText(buttonText, 20);

        DrawText(
            buttonText,
            (int)(playButton.x + (playButton.width - buttonTextWidth) / 2),
            (int)(playButton.y + 10),
            20,
            DARKGRAY
        );

        if (allDone) {
            const char* message = "All travelers reached destination!";
            int messageWidth = MeasureText(message, 22);

            DrawRectangle(
                SCREEN_WIDTH / 2 - messageWidth / 2 - 10,
                SCREEN_HEIGHT - 55,
                messageWidth + 20,
                36,
                LIGHTGRAY
            );

            DrawText(
                message,
                SCREEN_WIDTH / 2 - messageWidth / 2,
                SCREEN_HEIGHT - 48,
                22,
                DARKGREEN
            );
        }

        EndDrawing();
    }

    for (int i = 0; i < numTravelers; i++) {
        if (travelers[i].pid > 0) {
            kill(travelers[i].pid, SIGTERM);
            waitpid(travelers[i].pid, NULL, 0);
        }

        free(travelers[i].path);
    }

    CloseWindow();
    freeGraph(g);

    return 0;
}