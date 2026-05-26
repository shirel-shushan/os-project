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

    /* Step 3: Simulate the journey along the path and report to the parent */
    for (int i = 0; i < pathLen; i++) {
        sleep(1); /* Simulate movement time between nodes */

        IPCMessage msg;
        msg.pid = myPid;
        msg.traveler_id = travelerId;
        msg.arrived_node = path[i];
        
        if (i == pathLen - 1) {
            msg.next_node = -1; /* Destination reached */
        } else {
            msg.next_node = path[i + 1]; /* Next immediate node */
        }
        msg.finished = false;

        /* Write progress report into the pipe */
        write(writeFd, &msg, sizeof(msg));
    }

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

    /* Skip the edge definitions (the parent process does not build the graph in M5) */
    for (int i = 0; i < M; i++) {
        int src, dst, weight;
        if (fscanf(file, "%d %d %d", &src, &dst, &weight) != 3) {
            printf("Invalid edge definition\n");
            fclose(file);
            return 1;
        }
    }

    /* Locate and read the travelers section */
    char line[256];
    bool foundTravelers = false;
    while (fgets(line, sizeof(line), file)) {
        if (strstr(line, "#travelers") || strstr(line, "travelers")) {
            foundTravelers = true;
            break;
        }
    }

    if (!foundTravelers) {
        printf("Error: #travelers section not found\n");
        fclose(file);
        return 1;
    }

    int numTravelers = 0;
    if (fscanf(file, "%d", &numTravelers) != 1 || numTravelers <= 0) {
        printf("Invalid number of travelers\n");
        fclose(file);
        return 1;
    }

    /* EDGE CASE PROTECTION: Ensure travelers do not exceed array limits */
    if (numTravelers > MAX_TRAVELERS) {
        printf("Error: Number of travelers (%d) exceeds maximum allowed (%d)\n", numTravelers, MAX_TRAVELERS);
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
        /* EDGE CASE PROTECTION: Validate that traveler nodes exist within the graph bounds */
        if (sources[i] < 0 || sources[i] >= N || destinations[i] < 0 || destinations[i] >= N) {
            printf("Error: Traveler %d nodes are out of graph bounds (0 to %d)\n", i, N - 1);
            fclose(file);
            return 1;
        }
    }
    fclose(file);

    /* Create IPC pipes for each traveler */
    int pipes[MAX_TRAVELERS][2];
    pid_t childPids[MAX_TRAVELERS];

    for (int i = 0; i < numTravelers; i++) {
        if (pipe(pipes[i]) == -1) {
            perror("Pipe creation failed");
            return 1;
        }
        /* Set the read end of the pipe to Non-blocking mode so the GUI loop won't freeze */
        int flags = fcntl(pipes[i][0], F_GETFL, 0);
        fcntl(pipes[i][0], F_SETFL, flags | O_NONBLOCK);
    }

    /* Spawn child processes using fork() */
    for (int i = 0; i < numTravelers; i++) {
        pid_t pid = fork();
        
        /* EDGE CASE PROTECTION: Check if fork failed due to system resource exhaustion */
        if (pid < 0) {
            perror("Fork failed");
            /* Terminate already created children to avoid leakage */
            for (int j = 0; j < i; j++) {
                kill(childPids[j], SIGKILL);
                waitpid(childPids[j], NULL, 0);
            }
            return 1;
        }

        if (pid == 0) {
            /* --- Child Process Execution --- */
            /* Close unused read ends and other children's write descriptors */
            for (int j = 0; j < numTravelers; j++) {
                close(pipes[j][0]);
                if (j != i) {
                    close(pipes[j][1]);
                }
            }
            /* Run the autonomous traveler logic */
            runTravelerChild(i, sources[i], destinations[i], argv[1], pipes[i][1]);
        } else {
            /* --- Parent Process Execution --- */
            childPids[i] = pid;
            close(pipes[i][1]); /* Close the write end; parent only reads from the child */
        }
    }

    /* Simulation management and logging loop */
    bool finished[MAX_TRAVELERS] = {false};
    int completedCount = 0;

    while (completedCount < numTravelers) {
        
        for (int i = 0; i < numTravelers; i++) {
            if (finished[i]) continue;

            IPCMessage msg;
            /* Perform a non-blocking read from the child's pipe */
            ssize_t bytesRead = read(pipes[i][0], &msg, sizeof(msg));

            if (bytesRead > 0) {
                if (msg.finished) {
                    /* Print termination status exactly per requirements */
                    printf("[PID=%d] finished\n", msg.pid);
                    finished[i] = true;
                    completedCount++;
                } else {
                    /* Print travel log status exactly per requirements */
                    if (msg.next_node == -1) {
                        printf("[PID=%d] arrived at node %d | DESTINATION\n", msg.pid, msg.arrived_node);
                    } else {
                        printf("[PID=%d] arrived at node %d | next node: %d\n", msg.pid, msg.arrived_node, msg.next_node);
                    }
                }
            }
        }
        
        /* Small delay to prevent the loop from consuming 100% of CPU core */
        usleep(10000); 
    }

    /* Final cleanup of child processes to prevent zombie leakage */
    for (int i = 0; i < numTravelers; i++) {
        waitpid(childPids[i], NULL, 0);
        close(pipes[i][0]);
    }

    return 0;
}