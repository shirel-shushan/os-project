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
#include "graph.h"
#include "dijkstra.h"
#include "raylib.h"

#define SCREEN_WIDTH 800
#define SCREEN_HEIGHT 600
#define NODE_RADIUS 20
#define ANIM_TIME 0.6f
#define MAX_TRAVELERS 20
#define MAX_NODES 64
#define INF 1000000000

typedef enum { MSG_ENTER_REQUEST, MSG_LEAVE, MSG_FINISHED } ChildMsgType;

/* Message sent from child -> parent */
typedef struct {
    pid_t pid;
    int traveler_id;
    int arrived_node;    /* node the child wants to enter (or just left) */
    int next_node;       /* node after arrived_node in path (-1 if destination) */
    int path_remaining;  /* nodes left including arrived_node – used by SJF */
    ChildMsgType type;
} ChildMessage;

/* One entry in a per-node waiting queue */
typedef struct {
    int traveler_id;
    pid_t pid;
    int arrived_node;
    int next_node;
    int path_remaining;
} WaitEntry;

typedef enum { SCHED_FCFS, SCHED_SJF } SchedAlgo;

/* Per-node state managed by the parent */
static WaitEntry nodeQueue[MAX_NODES][MAX_TRAVELERS];
static int nodeQueueLen[MAX_NODES];
static bool nodeOccupied[MAX_NODES];

/* GUI traveler state */
typedef struct {
    Color color;
    pid_t pid;
    Vector2 animFrom, animTo;
    float animTimer;
    bool animating;
    bool isWaiting;
    int waitingForNode;
    bool done;
    float x, y;
} TravelerGUI;

static Color travelerColors[15];
static void initColors() {
    travelerColors[0]=RED; travelerColors[1]=GREEN; travelerColors[2]=PURPLE;
    travelerColors[3]=ORANGE; travelerColors[4]=PINK; travelerColors[5]=YELLOW;
    travelerColors[6]=LIME; travelerColors[7]=VIOLET; travelerColors[8]=BROWN;
    travelerColors[9]=GOLD; travelerColors[10]=MAGENTA; travelerColors[11]=DARKGREEN;
    travelerColors[12]=DARKBLUE; travelerColors[13]=DARKPURPLE; travelerColors[14]=MAROON;
}

static void skipCommentLines(FILE* file) {
    char line[256]; long pos;
    while ((pos = ftell(file)), fgets(line, sizeof(line), file)) {
        int i = 0;
        while (line[i]==' '||line[i]=='\t') i++;
        if (line[i]!='#'&&line[i]!='\n'&&line[i]!='\r'&&line[i]!='\0') { fseek(file,pos,SEEK_SET); return; }
    }
}

void DrawDirectedEdge(Vector2 start, Vector2 end, int weight) {
    float dx=end.x-start.x, dy=end.y-start.y;
    float length=sqrtf(dx*dx+dy*dy); if(length==0) return;
    float angle=atan2f(dy,dx);
    Vector2 adjEnd={end.x-NODE_RADIUS*cosf(angle), end.y-NODE_RADIUS*sinf(angle)};
    DrawLineEx(start,adjEnd,2.5f,DARKGRAY);
    float as=12.0f;
    Vector2 p1={adjEnd.x-as*cosf(angle-PI/7),adjEnd.y-as*sinf(angle-PI/7)};
    Vector2 p2={adjEnd.x-as*cosf(angle+PI/7),adjEnd.y-as*sinf(angle+PI/7)};
    DrawLineEx(adjEnd,p1,2.5f,DARKGRAY); DrawLineEx(adjEnd,p2,2.5f,DARKGRAY);
    int tx=(int)(start.x+dx*0.45f), ty=(int)(start.y+dy*0.45f);
    const char* wt=TextFormat("%d",weight); int tw=MeasureText(wt,18);
    DrawRectangle(tx-3,ty-3,tw+6,24,WHITE); DrawText(wt,tx,ty,18,MAROON);
}

/* ---- Child process ---- */
void runTravelerChild(int id, int start, int end, const char* filename,
                      int writeFd, int permFd) {
    pid_t myPid = getpid();

    FILE* file = fopen(filename, "r");
    if (!file) { exit(1); }
    skipCommentLines(file);
    int N, M;
    if (fscanf(file,"%d %d",&N,&M)!=2) { fclose(file); exit(1); }
    Graph* g = createGraph(N);
    for (int i=0;i<M;i++) {
        int s,d,w; if(fscanf(file,"%d %d %d",&s,&d,&w)==3) addEdge(g,s,d,w);
    }
    fclose(file);

    int pathLen=0, totalWeight=0;
    int* path = dijkstra(g, start, end, &pathLen, &totalWeight);
    if (!path || pathLen<=0) {
        ChildMessage msg={myPid,id,start,-1,0,MSG_FINISHED};
        write(writeFd,&msg,sizeof(msg));
        if(path) free(path);
        freeGraph(g); close(writeFd); exit(0);
    }

    for (int i=0; i<pathLen-1; i++) {
        int target   = path[i+1];
        int after    = (i+2 < pathLen) ? path[i+2] : -1;
        int remaining = pathLen - i - 1; /* nodes left to visit including target */

        /* Tell parent we want to enter 'target' */
        ChildMessage req = {myPid, id, target, after, remaining, MSG_ENTER_REQUEST};
        write(writeFd, &req, sizeof(req));

        /* Block here until parent grants permission */
        char permit;
        read(permFd, &permit, 1);

        /* Inside the node: spend exactly 1 second */
        sleep(1);

        /* Tell parent we are leaving 'target' */
        ChildMessage leave = {myPid, id, target, after, remaining-1, MSG_LEAVE};
        write(writeFd, &leave, sizeof(leave));
    }

    ChildMessage fin = {myPid, id, end, -1, 0, MSG_FINISHED};
    write(writeFd, &fin, sizeof(fin));

    free(path); freeGraph(g); close(writeFd); exit(0);
}

/* ---- Scheduling: pick next entry from queue for a node ---- */
int pickNext(int node, SchedAlgo algo) {
    int len = nodeQueueLen[node];
    if (len == 0) return -1;
    if (algo == SCHED_FCFS) return 0; /* first in queue */
    /* SJF: pick traveler with fewest remaining nodes */
    int best = 0;
    for (int i = 1; i < len; i++) {
        if (nodeQueue[node][i].path_remaining < nodeQueue[node][best].path_remaining)
            best = i;
    }
    return best;
}

/* Grant permission to the chosen entry; remove it from queue */
void grantEntry(int node, int idx, int permPipes[][2],
                TravelerGUI* travelers, Vector2* positions,
                SchedAlgo algo) {
    WaitEntry chosen = nodeQueue[node][idx];
    /* shift queue */
    for (int k = idx; k < nodeQueueLen[node]-1; k++)
        nodeQueue[node][k] = nodeQueue[node][k+1];
    nodeQueueLen[node]--;

    nodeOccupied[node] = true;
    travelers[chosen.traveler_id].isWaiting = false;

    /* Log */
    if (chosen.next_node == -1)
        printf("[PID=%d] arrived at node %d | DESTINATION\n", chosen.pid, chosen.arrived_node);
    else
        printf("[PID=%d] arrived at node %d | next node: %d\n", chosen.pid, chosen.arrived_node, chosen.next_node);
    fflush(stdout);

    /* Animate into the node */
    travelers[chosen.traveler_id].animFrom = travelers[chosen.traveler_id].animTo;
    travelers[chosen.traveler_id].animTo = positions[node];
    travelers[chosen.traveler_id].animTimer = 0;
    travelers[chosen.traveler_id].animating = true;

    /* Send permission byte */
    char permit = 1;
    write(permPipes[chosen.traveler_id][1], &permit, 1);

    (void)algo; /* used via pickNext */
}

int main(int argc, char* argv[]) {
    if (argc != 4 || strcmp(argv[1],"-schd")!=0) {
        printf("Usage: %s -schd fcfs|sjf <input_file>\n", argv[0]);
        return 1;
    }
    SchedAlgo algo;
    if (strcmp(argv[2],"fcfs")==0) algo = SCHED_FCFS;
    else if (strcmp(argv[2],"sjf")==0) algo = SCHED_SJF;
    else { printf("Unknown scheduler: %s (use fcfs or sjf)\n", argv[2]); return 1; }
    const char* filename = argv[3];

    FILE* file = fopen(filename,"r");
    if (!file) { printf("Error opening file\n"); return 1; }
    skipCommentLines(file);
    int N, M;
    if (fscanf(file,"%d %d",&N,&M)!=2||N<=0||M<0) { printf("Invalid input\n"); fclose(file); return 1; }
    Graph* g = createGraph(N);
    for (int i=0;i<M;i++) {
        int s,d,w;
        if (fscanf(file,"%d %d %d",&s,&d,&w)!=3) { printf("Invalid input\n"); freeGraph(g); fclose(file); return 1; }
        addEdge(g,s,d,w);
    }
    skipCommentLines(file);
    int numTravelers;
    if (fscanf(file,"%d",&numTravelers)!=1||numTravelers<=0||numTravelers>MAX_TRAVELERS) {
        printf("Invalid input\n"); freeGraph(g); fclose(file); return 1;
    }
    int sources[MAX_TRAVELERS], destinations[MAX_TRAVELERS];
    for (int i=0;i<numTravelers;i++) {
        if (fscanf(file,"%d %d",&sources[i],&destinations[i])!=2) {
            printf("Invalid input\n"); freeGraph(g); fclose(file); return 1;
        }
    }
    fclose(file);

    /* pipes[i]: child i writes -> parent reads (non-blocking on parent side) */
    /* permPipes[i]: parent writes -> child i reads (blocking on child side) */
    int pipes[MAX_TRAVELERS][2];
    int permPipes[MAX_TRAVELERS][2];
    pid_t childPids[MAX_TRAVELERS];

    for (int i=0;i<numTravelers;i++) {
        if (pipe(pipes[i])==-1||pipe(permPipes[i])==-1) { perror("pipe"); return 1; }
        int flags = fcntl(pipes[i][0], F_GETFL, 0);
        fcntl(pipes[i][0], F_SETFL, flags | O_NONBLOCK);
    }

    for (int i=0;i<numTravelers;i++) {
        pid_t pid = fork();
        if (pid<0) { perror("fork"); return 1; }
        if (pid==0) {
            for (int j=0;j<numTravelers;j++) {
                close(pipes[j][0]);
                if (j!=i) close(pipes[j][1]);
                close(permPipes[j][1]);
                if (j!=i) close(permPipes[j][0]);
            }
            runTravelerChild(i, sources[i], destinations[i], filename,
                             pipes[i][1], permPipes[i][0]);
        } else {
            childPids[i] = pid;
            close(pipes[i][1]);
            close(permPipes[i][0]);
        }
    }

    /* Initialise node state */
    for (int i=0;i<N;i++) { nodeQueueLen[i]=0; nodeOccupied[i]=false; }

    initColors();
    const char* title = (algo==SCHED_FCFS)
        ? "Milestone 7 - Scheduler: FCFS"
        : "Milestone 7 - Scheduler: SJF";
    InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, title);
    SetTargetFPS(60);

    Vector2 positions[N];
    float cx=SCREEN_WIDTH/2.0f, cy=SCREEN_HEIGHT/2.0f, rad=220.0f;
    for (int i=0;i<N;i++) {
        float angle = i*(2.0f*PI/N);
        positions[i].x = cx + rad*cosf(angle);
        positions[i].y = cy + rad*sinf(angle);
    }

    TravelerGUI travelers[MAX_TRAVELERS];
    for (int i=0;i<numTravelers;i++) {
        travelers[i].color = travelerColors[i%15];
        travelers[i].pid = childPids[i];
        travelers[i].animating = false;
        travelers[i].isWaiting = false;
        travelers[i].waitingForNode = -1;
        travelers[i].done = false;
        travelers[i].x = positions[sources[i]].x;
        travelers[i].y = positions[sources[i]].y;
        travelers[i].animTo = positions[sources[i]];
    }

    bool finishedFlags[MAX_TRAVELERS];
    for (int i=0;i<numTravelers;i++) finishedFlags[i]=false;
    int completedCount = 0;

    while (!WindowShouldClose()) {
        float dt = GetFrameTime();

        /* Drain messages from children */
        for (int i=0;i<numTravelers;i++) {
            if (finishedFlags[i]) continue;
            ChildMessage msg;
            while (read(pipes[i][0], &msg, sizeof(msg)) == sizeof(msg)) {

                if (msg.type == MSG_ENTER_REQUEST) {
                    int node = msg.arrived_node;
                    travelers[i].isWaiting = true;
                    travelers[i].waitingForNode = node;

                    if (!nodeOccupied[node]) {
                        /* Node free: grant immediately */
                        WaitEntry e = {i, msg.pid, msg.arrived_node, msg.next_node, msg.path_remaining};
                        nodeQueue[node][0] = e;
                        nodeQueueLen[node] = 1;
                        grantEntry(node, 0, permPipes, travelers, positions, algo);
                    } else {
                        /* Enqueue */
                        WaitEntry e = {i, msg.pid, msg.arrived_node, msg.next_node, msg.path_remaining};
                        nodeQueue[node][nodeQueueLen[node]++] = e;
                    }

                } else if (msg.type == MSG_LEAVE) {
                    int node = msg.arrived_node;
                    if (nodeQueueLen[node] > 0) {
                        /* Wake next based on scheduling algorithm */
                        int idx = pickNext(node, algo);
                        grantEntry(node, idx, permPipes, travelers, positions, algo);
                    } else {
                        nodeOccupied[node] = false;
                    }

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

        /* Advance animations */
        for (int i=0;i<numTravelers;i++) {
            if (!travelers[i].animating) continue;
            travelers[i].animTimer += dt;
            float t = travelers[i].animTimer / ANIM_TIME;
            if (t>=1.0f) { t=1.0f; travelers[i].animating=false; }
            travelers[i].x = travelers[i].animFrom.x + (travelers[i].animTo.x - travelers[i].animFrom.x)*t;
            travelers[i].y = travelers[i].animFrom.y + (travelers[i].animTo.y - travelers[i].animFrom.y)*t;
        }

        BeginDrawing();
        ClearBackground(RAYWHITE);

        /* Edges */
        for (int i=0;i<N;i++)
            for (int j=0;j<N;j++) {
                int w=g->graph[i][j];
                if (w>0&&w<INF) DrawDirectedEdge(positions[i],positions[j],w);
            }

        /* Nodes */
        for (int i=0;i<N;i++) {
            DrawCircleV(positions[i], NODE_RADIUS, SKYBLUE);
            DrawCircleLines((int)positions[i].x,(int)positions[i].y,NODE_RADIUS,BLUE);
            const char* txt=TextFormat("%d",i); int fs=20;
            DrawText(txt,(int)(positions[i].x-MeasureText(txt,fs)/2),(int)(positions[i].y-fs/2+2),fs,DARKBLUE);
        }

        /* Active travelers */
        for (int i=0;i<numTravelers;i++) {
            if (travelers[i].isWaiting) continue;
            Vector2 p={travelers[i].x,travelers[i].y};
            DrawCircleV(p,12,travelers[i].color);
            DrawText(TextFormat("%d",i),(int)(p.x-4),(int)(p.y-8),14,WHITE);
        }

        /* Waiting travelers: shown outside the node they want to enter */
        int waitCount[MAX_NODES];
        for (int i=0;i<N;i++) waitCount[i]=0;
        for (int i=0;i<numTravelers;i++) {
            if (!travelers[i].isWaiting) continue;
            int node = travelers[i].waitingForNode;
            int slot = waitCount[node]++;
            float baseAngle = -PI/2.0f + slot*0.65f;
            float dist = NODE_RADIUS + 30.0f;
            Vector2 p = {
                positions[node].x + dist*cosf(baseAngle),
                positions[node].y + dist*sinf(baseAngle)
            };
            DrawCircleV(p,12,Fade(GRAY,0.8f));
            DrawCircleLines((int)p.x,(int)p.y,14,travelers[i].color);
            DrawText(TextFormat("%d",i),(int)(p.x-4),(int)(p.y-8),14,BLACK);
            DrawText("wait",(int)(p.x-12),(int)(p.y+14),10,DARKGRAY);
        }

        /* HUD: algorithm name */
        const char* algLabel = (algo==SCHED_FCFS) ? "Scheduler: FCFS" : "Scheduler: SJF";
        DrawRectangle(10,10,MeasureText(algLabel,18)+16,30,DARKBLUE);
        DrawText(algLabel,18,16,18,WHITE);

        if (completedCount >= numTravelers) {
            const char* msg = "All travelers reached destination!";
            int mw = MeasureText(msg,22);
            DrawRectangle(SCREEN_WIDTH/2-mw/2-10,SCREEN_HEIGHT-55,mw+20,36,LIGHTGRAY);
            DrawText(msg,SCREEN_WIDTH/2-mw/2,SCREEN_HEIGHT-48,22,DARKGREEN);
        }

        EndDrawing();
    }

    for (int i=0;i<numTravelers;i++) {
        kill(childPids[i],SIGTERM);
        waitpid(childPids[i],NULL,0);
        close(pipes[i][0]);
        close(permPipes[i][1]);
    }
    CloseWindow();
    freeGraph(g);
    return 0;
}