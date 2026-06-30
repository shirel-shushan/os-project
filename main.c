#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <stdbool.h>
#include <string.h>
#include "graph.h"
#include "dijkstra.h"
#include <signal.h>
#include <semaphore.h>

#define MAX_TRAVELERS 20

/* IPC message structure for communication between child and parent */
typedef struct {
    pid_t pid;          /* Child process PID */
    int traveler_id;    /* ID of the traveler for GUI tracking */
    int arrived_node;   /* The current node the traveler has reached */
    int next_node;      /* The next node in the path (-1 if destination reached) */
    bool finished;      /* True if the traveler has fully completed the journey */
} IPCMessage;

/* Child process function representing an autonomous traveler */
void runTravelerChild(int travelerId, int start, int end, char* filename, int writeFd) {
    pid_t myPid = getpid();

    /* Step 1: Each child reads the graph independently from the input file */
    FILE* file = fopen(filename, "r");
    if (file == NULL) {
        IPCMessage msg = { myPid, travelerId, start, -1, true };
        write(writeFd, &msg, sizeof(msg));
        close(writeFd);
        exit(1);
    }

    int N, M;
    if (fscanf(file, "%d %d", &N, &M) != 2) {
        fclose(file);
        exit(1);
    }
    
    Graph* g = createGraph(N);
    if (g == NULL) {
        fclose(file);
        exit(1);
    }

    for (int i = 0; i < M; i++) {
        int src, dst, weight;
        if (fscanf(file, "%d %d %d", &src, &dst, &weight) == 3) {
            addEdge(g, src, dst, weight);
        }
    }
    fclose(file);

    /* Step 2: Child calculates its own shortest path using Dijkstra */
    int pathLen = 0;
    int totalWeight = 0;
    int* path = dijkstra(g, start, end, &pathLen, &totalWeight);

    /* Handle error case: No path found or source equals destination */
    if (path == NULL || pathLen <= 0) {
        IPCMessage msg = { myPid, travelerId, start, -1, true };
        write(writeFd, &msg, sizeof(msg));
        if (path) free(path);
        freeGraph(g);
        close(writeFd);
        exit(0);
    }

    /* Step 3: Simulate the journey along the path with Node Synchronization */
    for (int i = 0; i < pathLen - 1; i++) {
        int next_node = path[i + 1];

        /* Create a unique semaphore name for the next node (e.g., "/sem_node_2") */
        char sem_name[64];
        sprintf(sem_name, "/sem_node_%d", next_node);

        /* Open/Create a named semaphore initialized to 1 (acting as a Mutex) */
        sem_t* node_sem = sem_open(sem_name, O_CREAT, 0644, 1);
        if (node_sem == SEM_FAILED) {
            perror("sem_open failed in child");
            break;
        }

        // ======================================================================
        // 📢 הדפסת החלטת מתזמן: מי ממתין ולמה (דרישת משימה B)
        // ======================================================================
        printf("[SCHEDULER] Traveler (PID=%d) is WAITING to enter Node %d. Reason: Node is currently tracked or occupied.\n", 
               myPid, next_node);
        fflush(stdout); 

        /* --- CRITICAL SECTION START: Wait outside if the node is occupied --- */
        sem_wait(node_sem);

        // ======================================================================
        // 📢 הדפסת החלטת מתזמן: מי נבחר ולמה (דרישת משימה B)
        // ======================================================================
        printf("[SCHEDULER] Traveler (PID=%d) was SELECTED to occupy Node %d. Reason: Semaphore token acquired.\n", 
               myPid, next_node);
        fflush(stdout);

        IPCMessage msg;
        msg.pid = myPid;
        msg.traveler_id = travelerId;
        msg.arrived_node = path[i];
        msg.next_node = next_node;
        msg.finished = false;

        /* Write progress report into the pipe */
        write(writeFd, &msg, sizeof(msg));

        /* Sleep for 1 full second inside the node as required */
        sleep(1);

        /* --- CRITICAL SECTION END: Release the node for the next waiting process --- */
        sem_post(node_sem);
        sem_close(node_sem);
    }

    /* Brief sleep before reaching the final destination */
    sleep(1);

    /* Send final termination message to the parent */
    IPCMessage finalMsg = { myPid, travelerId, end, -1, true };
    write(writeFd, &finalMsg, sizeof(finalMsg));

    /* Clean up memory and close descriptor */
    free(path);
    freeGraph(g);
    close(writeFd);
    exit(0);
}

int main(int argc, char* argv[]) {

    /* Check for correct command line arguments */
    if (argc != 2) {
        printf("Usage: %s <input_file>\n", argv[0]);
        return 1;
    }

    /* Open the input file to parse basic configuration */
    FILE* file = fopen(argv[1], "r");
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

    /* Skip the edge definitions */
    for (int i = 0; i < M; i++) {
        int src, dst, weight;
        if (fscanf(file, "%d %d %d", &src, &dst, &weight) != 3) {
            printf("Invalid edge definition\n");
            fclose(file);
            return 1;
        }
    }

    /* Read the number of travelers directly from the next line */
    int numTravelers = 0;
    if (fscanf(file, "%d", &numTravelers) != 1 || numTravelers <= 0) {
        printf("Invalid number of travelers\n");
        fclose(file);
        return 1;
    }

    if (numTravelers > MAX_TRAVELERS) {
        printf("Error: Number of travelers exceeds limit\n");
        fclose(file);
        return 1;
    }

    int sources[MAX_TRAVELERS];
    int destinations[MAX_TRAVELERS];

    for (int i = 0; i < numTravelers; i++) {
        if (fscanf(file, "%d %d", &sources[i], &destinations[i]) != 2) {
            printf("Invalid traveler data\n");
            fclose(file);
            return 1;
        }
    }
    fclose(file);

    /* Unlink old semaphores from potential previous crashes to start fresh */
    for (int i = 0; i < N; i++) {
        char sem_name[64];
        sprintf(sem_name, "/sem_node_%d", i);
        sem_unlink(sem_name);
    }

    /* Create IPC pipes for each traveler */
    int pipes[MAX_TRAVELERS][2];
    pid_t childPids[MAX_TRAVELERS];

    for (int i = 0; i < numTravelers; i++) {
        if (pipe(pipes[i]) == -1) {
            perror("Pipe creation failed");
            return 1;
        }
        int flags = fcntl(pipes[i][0], F_GETFL, 0);
        fcntl(pipes[i][0], F_SETFL, flags | O_NONBLOCK);
    }

    /* Spawn child processes using fork() */
    for (int i = 0; i < numTravelers; i++) {
        pid_t pid = fork();
        
        if (pid < 0) {
            perror("Fork failed");
            for (int j = 0; j < i; j++) {
                kill(childPids[j], SIGKILL);
                waitpid(childPids[j], NULL, 0);
            }
            return 1;
        }

        if (pid == 0) {
            for (int j = 0; j < numTravelers; j++) {
                close(pipes[j][0]);
                if (j != i) {
                    close(pipes[j][1]);
                }
            }
            runTravelerChild(i, sources[i], destinations[i], argv[1], pipes[i][1]);
        } else {
            childPids[i] = pid;
            close(pipes[i][1]);
        }
    }

    /* Simulation management and logging loop */
    bool finished[MAX_TRAVELERS] = {false};
    int completedCount = 0;

    while (completedCount < numTravelers) {
        for (int i = 0; i < numTravelers; i++) {
            if (finished[i]) continue;

            IPCMessage msg;
            ssize_t bytesRead = read(pipes[i][0], &msg, sizeof(msg));

            if (bytesRead > 0) {
                if (msg.finished) {
                    printf("[PID=%d] arrived at node %d | DESTINATION\n", msg.pid, msg.arrived_node);
                    printf("[PID=%d] finished\n", msg.pid);
                    finished[i] = true;
                    completedCount++;
                } else {
                    printf("[PID=%d] arrived at node %d | next node: %d\n", msg.pid, msg.arrived_node, msg.next_node);
                }
            }
        }
        usleep(10000); 
    }

    /* Final cleanup: Unlink semaphores upon successful termination */
    for (int i = 0; i < N; i++) {
        char sem_name[64];
        sprintf(sem_name, "/sem_node_%d", i);
        sem_unlink(sem_name);
    }

    for (int i = 0; i < numTravelers; i++) {
        waitpid(childPids[i], NULL, 0);
        close(pipes[i][0]);
    }

    return 0;
}