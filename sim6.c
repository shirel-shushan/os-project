#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <stdbool.h>
#include <string.h>
#include <signal.h>
#include <semaphore.h>
#include <errno.h>
#include "graph.h"
#include "dijkstra.h"
#include "raylib.h"

#define SCREEN_WIDTH 800
#define SCREEN_HEIGHT 600
#define NODE_RADIUS 20
#define ANIM_TIME 0.6f
#define MAX_TRAVELERS 20
#define INF 1000000000

typedef enum { MSG_WAITING = 0, MSG_PROGRESS = 1, MSG_FINISHED = 2 } MsgStatus;

/* IPC message structure sent from child -> parent through the pipe */
typedef struct {
    pid_t pid;
    int traveler_id;
    int arrived_node;   /* node the traveler is at / just left */
    int next_node;       /* node the traveler is heading to (-1 if none) */
    MsgStatus status;     /* WAITING = blocked outside next_node, PROGRESS = entered, FINISHED = done */
} IPCMessage;

typedef struct {
    Color color;
    pid_t pid;

    int curNode, nextNode;
    Vector2 animFrom, animTo;
    float animTimer;
    bool animating;
    bool isWaiting;       /* blocked outside nextNode, waiting for the semaphore */
    int waitingForNode;
    bool done;
    float x, y;
} TravelerGUI;

static Color travelerColors[] = {
    RED, GREEN, PURPLE, ORANGE, PINK,
    YELLOW, LIME, VIOLET, BROWN, GOLD,
    MAGENTA, DARKGREEN, DARKBLUE, DARKPURPLE, MAROON
};

static void skipCommentLines(FILE* file) {
    char line[256];
    long pos;
    while ((pos = ftell(file)), fgets(line, sizeof(line), file)) {
        int i = 0;
        while (line[i] == ' ' || line[i] == '\t') i++;
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
    Vector2 adjustedEnd = { end.x - NODE_RADIUS * cosf(angle), end.y - NODE_RADIUS * sinf(angle) };
    DrawLineEx(start, adjustedEnd, 2.5f, DARKGRAY);

    float arrowSize = 12.0f;
    Vector2 p1 = { adjustedEnd.x - arrowSize * cosf(angle - PI / 7), adjustedEnd.y - arrowSize * sinf(angle - PI / 7) };
    Vector2 p2 = { adjustedEnd.x - arrowSize * cosf(angle + PI / 7), adjustedEnd.y - arrowSize * sinf(angle + PI / 7) };
    DrawLineEx(adjustedEnd, p1, 2.5f, DARKGRAY);
    DrawLineEx(adjustedEnd, p2, 2.5f, DARKGRAY);

    int textX = (int)(start.x + dx * 0.45f);
    int textY = (int)(start.y + dy * 0.45f);
    const char* weightText = TextFormat("%d", weight);
    int textWidth = MeasureText(weightText, 18);
    DrawRectangle(textX - 3, textY - 3, textWidth + 6, 24, WHITE);
    DrawText(weightText, textX, textY, 18, MAROON);
}

/* ---- Child process: computes its own path, locks each node before entering it ---- */
void runTravelerChild(int travelerId, int start, int end, const char* filename, int writeFd) {
    pid_t myPid = getpid();

    FILE* file = fopen(filename, "r");
    if (file == NULL) {
        IPCMessage msg = { myPid, travelerId, start, -1, MSG_FINISHED };
        write(writeFd, &msg, sizeof(msg));
        close(writeFd);
        exit(1);
    }

    skipCommentLines(file);
    int N, M;
    if (fscanf(file, "%d %d", &N, &M) != 2) { fclose(file); exit(1); }

    Graph* g = createGraph(N);
    for (int i = 0; i < M; i++) {
        int src, dst, weight;
        if (fscanf(file, "%d %d %d", &src, &dst, &weight) == 3) {
            addEdge(g, src, dst, weight);
        }
    }
    fclose(file);

    int pathLen = 0, totalWeight = 0;
    int* path = dijkstra(g, start, end, &pathLen, &totalWeight);

    if (path == NULL || pathLen <= 0) {
        IPCMessage msg = { myPid, travelerId, start, -1, MSG_FINISHED };
        write(writeFd, &msg, sizeof(msg));
        if (path) free(path);
        freeGraph(g);
        close(writeFd);
        exit(0);
    }

    for (int i = 0; i < pathLen - 1; i++) {
        int next_node = path[i + 1];

        char sem_name[64];
        sprintf(sem_name, "/sem_node_%d", next_node);
        sem_t* node_sem = sem_open(sem_name, O_CREAT, 0644, 1);
        if (node_sem == SEM_FAILED) { perror("sem_open failed in child"); break; }

        /* --- CRITICAL SECTION: only one traveler may occupy next_node at a time --- */
        /* First try to enter immediately. If the node is already locked, send WAIT to the parent. */
        if (sem_trywait(node_sem) == -1) {
            IPCMessage waitMsg = { myPid, travelerId, path[i], next_node, MSG_WAITING };
            write(writeFd, &waitMsg, sizeof(waitMsg));

            /* Now really wait until the node becomes free. */
            sem_wait(node_sem);
        }

        IPCMessage progressMsg = { myPid, travelerId, path[i], next_node, MSG_PROGRESS };
        write(writeFd, &progressMsg, sizeof(progressMsg));

        sleep(2); /* keep the node occupied long enough so WAIT is visible */

        sem_post(node_sem);
        sem_close(node_sem);
        /* --- CRITICAL SECTION END --- */
    }

    sleep(1);

    IPCMessage finalMsg = { myPid, travelerId, end, -1, MSG_FINISHED };
    write(writeFd, &finalMsg, sizeof(finalMsg));

    free(path);
    freeGraph(g);
    close(writeFd);
    exit(0);
}

int main(int argc, char* argv[]) {
    if (argc != 2) {
        printf("Usage: %s <input_file>\n", argv[0]);
        return 1;
    }

    FILE* file = fopen(argv[1], "r");
    if (file == NULL) { printf("Error opening file\n"); return 1; }

    skipCommentLines(file);
    int N, M;
    if (fscanf(file, "%d %d", &N, &M) != 2 || N <= 0 || M < 0) {
        printf("Invalid input\n"); fclose(file); return 1;
    }

    Graph* g = createGraph(N);
    for (int i = 0; i < M; i++) {
        int src, dst, weight;
        if (fscanf(file, "%d %d %d", &src, &dst, &weight) != 3) {
            printf("Invalid input\n"); freeGraph(g); fclose(file); return 1;
        }
        addEdge(g, src, dst, weight);
    }

    skipCommentLines(file);
    int numTravelers;
    if (fscanf(file, "%d", &numTravelers) != 1 || numTravelers <= 0 || numTravelers > MAX_TRAVELERS) {
        printf("Invalid input\n"); freeGraph(g); fclose(file); return 1;
    }

    int sources[MAX_TRAVELERS], destinations[MAX_TRAVELERS];
    for (int i = 0; i < numTravelers; i++) {
        if (fscanf(file, "%d %d", &sources[i], &destinations[i]) != 2) {
            printf("Invalid input\n"); freeGraph(g); fclose(file); return 1;
        }
    }
    fclose(file);

    /* Clear any semaphores left over from a previous crashed run */
    for (int i = 0; i < N; i++) {
        char sem_name[64];
        sprintf(sem_name, "/sem_node_%d", i);
        sem_unlink(sem_name);
    }

    int pipes[MAX_TRAVELERS][2];
    pid_t childPids[MAX_TRAVELERS];

    for (int i = 0; i < numTravelers; i++) {
        if (pipe(pipes[i]) == -1) { perror("pipe"); return 1; }
        int flags = fcntl(pipes[i][0], F_GETFL, 0);
        fcntl(pipes[i][0], F_SETFL, flags | O_NONBLOCK);
    }

    for (int i = 0; i < numTravelers; i++) {
        pid_t pid = fork();
        if (pid < 0) {
            perror("fork");
            for (int j = 0; j < i; j++) { kill(childPids[j], SIGKILL); waitpid(childPids[j], NULL, 0); }
            return 1;
        }
        if (pid == 0) {
            for (int j = 0; j < numTravelers; j++) {
                close(pipes[j][0]);
                if (j != i) close(pipes[j][1]);
            }
            runTravelerChild(i, sources[i], destinations[i], argv[1], pipes[i][1]);
        } else {
            childPids[i] = pid;
            close(pipes[i][1]);
        }
    }

    InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "Milestone 6 - Node Synchronization");
    SetTargetFPS(60);

    Vector2 positions[N];
    float centerX = SCREEN_WIDTH / 2.0f, centerY = SCREEN_HEIGHT / 2.0f, radius = 220.0f;
    for (int i = 0; i < N; i++) {
        float angle = i * (2.0f * PI / N);
        positions[i].x = centerX + radius * cosf(angle);
        positions[i].y = centerY + radius * sinf(angle);
    }

    TravelerGUI travelers[MAX_TRAVELERS];
    for (int i = 0; i < numTravelers; i++) {
        travelers[i].color = travelerColors[i % 15];
        travelers[i].pid = childPids[i];
        travelers[i].curNode = sources[i];
        travelers[i].nextNode = sources[i];
        travelers[i].animating = false;
        travelers[i].isWaiting = false;
        travelers[i].waitingForNode = -1;
        travelers[i].done = false;
        travelers[i].x = positions[sources[i]].x;
        travelers[i].y = positions[sources[i]].y;
    }

    bool finishedFlags[MAX_TRAVELERS] = {false};
    int completedCount = 0;

    while (!WindowShouldClose()) {
        float dt = GetFrameTime();

        for (int i = 0; i < numTravelers; i++) {
            if (finishedFlags[i]) continue;

            IPCMessage msg;
            ssize_t n;
            while ((n = read(pipes[i][0], &msg, sizeof(msg))) == sizeof(msg)) {
                if (msg.status == MSG_WAITING) {
                    printf("[PID=%d] WAIT before node %d\n", msg.pid, msg.next_node);
                    fflush(stdout);
                    travelers[i].isWaiting = true;
                    travelers[i].waitingForNode = msg.next_node;
                } else if (msg.status == MSG_PROGRESS) {
                    printf("[PID=%d] arrived at node %d | next node: %d\n", msg.pid, msg.arrived_node, msg.next_node);
                    fflush(stdout);
                    travelers[i].isWaiting = false;
                    travelers[i].curNode = msg.arrived_node;
                    travelers[i].nextNode = msg.next_node;
                    travelers[i].animFrom = positions[msg.arrived_node];
                    travelers[i].animTo = positions[msg.next_node];
                    travelers[i].animTimer = 0;
                    travelers[i].animating = true;
                } else { /* MSG_FINISHED */
                    printf("[PID=%d] arrived at node %d | DESTINATION\n", msg.pid, msg.arrived_node);
                    printf("[PID=%d] finished\n", msg.pid);
                    fflush(stdout);
                    finishedFlags[i] = true;
                    completedCount++;
                    travelers[i].done = true;
                    travelers[i].isWaiting = false;
                    travelers[i].x = positions[msg.arrived_node].x;
                    travelers[i].y = positions[msg.arrived_node].y;
                }
            }
        }

        for (int i = 0; i < numTravelers; i++) {
            if (travelers[i].animating) {
                travelers[i].animTimer += dt;
                float t = travelers[i].animTimer / ANIM_TIME;
                if (t >= 1.0f) { t = 1.0f; travelers[i].animating = false; }
                travelers[i].x = travelers[i].animFrom.x + (travelers[i].animTo.x - travelers[i].animFrom.x) * t;
                travelers[i].y = travelers[i].animFrom.y + (travelers[i].animTo.y - travelers[i].animFrom.y) * t;
            }
        }

        BeginDrawing();
        ClearBackground(RAYWHITE);

        for (int i = 0; i < N; i++)
            for (int j = 0; j < N; j++) {
                int weight = g->graph[i][j];
                if (weight > 0 && weight < INF) DrawDirectedEdge(positions[i], positions[j], weight);
            }

        for (int i = 0; i < N; i++) {
            DrawCircleV(positions[i], NODE_RADIUS, SKYBLUE);
            DrawCircleLines((int)positions[i].x, (int)positions[i].y, NODE_RADIUS, BLUE);
            const char* text = TextFormat("%d", i);
            int fontSize = 20;
            int textWidth = MeasureText(text, fontSize);
            DrawText(text, (int)(positions[i].x - textWidth / 2), (int)(positions[i].y - fontSize / 2 + 2), fontSize, DARKBLUE);
        }

        /* Draw travelers that are actively moving / occupying a node */
        for (int i = 0; i < numTravelers; i++) {
            if (travelers[i].isWaiting) continue; /* drawn separately below, offset outside the node */
            Vector2 p = { travelers[i].x, travelers[i].y };
            DrawCircleV(p, 12, travelers[i].color);
            DrawText(TextFormat("%d", i), (int)(p.x - 4), (int)(p.y - 8), 14, WHITE);
        }

        /* Draw travelers waiting outside a locked node: gray ring, stacked around the node,
           offset outward so it's visually obvious they have not entered yet */
        int waitCountPerNode[64] = {0};
        for (int i = 0; i < numTravelers; i++) {
            if (!travelers[i].isWaiting) continue;
            int node = travelers[i].waitingForNode;
            int slot = waitCountPerNode[node]++;
            float baseAngle = -PI / 2.0f + slot * 0.6f;
            float dist = NODE_RADIUS + 28.0f;
            Vector2 p = {
                positions[node].x + dist * cosf(baseAngle),
                positions[node].y + dist * sinf(baseAngle)
            };
            DrawCircleV(p, 12, Fade(GRAY, 0.85f));
            DrawCircleLines((int)p.x, (int)p.y, 14, travelers[i].color);
            DrawText(TextFormat("%d", i), (int)(p.x - 4), (int)(p.y - 8), 14, BLACK);
            DrawText("wait", (int)(p.x - 14), (int)(p.y + 14), 10, DARKGRAY);
        }

        if (completedCount >= numTravelers) {
            const char* message = "All travelers reached destination!";
            int messageWidth = MeasureText(message, 22);
            DrawRectangle(SCREEN_WIDTH / 2 - messageWidth / 2 - 10, SCREEN_HEIGHT - 55, messageWidth + 20, 36, LIGHTGRAY);
            DrawText(message, SCREEN_WIDTH / 2 - messageWidth / 2, SCREEN_HEIGHT - 48, 22, DARKGREEN);
        }

        EndDrawing();
    }

    for (int i = 0; i < numTravelers; i++) {
        kill(childPids[i], SIGTERM);
        waitpid(childPids[i], NULL, 0);
        close(pipes[i][0]);
    }

    for (int i = 0; i < N; i++) {
        char sem_name[64];
        sprintf(sem_name, "/sem_node_%d", i);
        sem_unlink(sem_name);
    }

    CloseWindow();
    freeGraph(g);
    return 0;
}
