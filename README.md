# os-project
Operating Systems project - graph simulation with Dijkstra
# OS Project - Graph Simulation & Pathfinding Animation

**Submitted by:** Shirel Shushan & Hodaya Zichri

## Project Overview
This project is a visual graph simulation and pathfinding animation built in C. It calculates the shortest path between a source and a destination using Dijkstra's algorithm, and visualizes the graph and the movement of an entity along the path using the `raylib` graphics library.

---

## Milestone 1: Dijkstra's Algorithm
* **Description:** Implementation of Dijkstra's algorithm. The program reads a directed graph from a text file, builds an adjacency matrix, and calculates the shortest path between a source and destination.
* **Compilation:** `make milestone1`
* **Execution:** `./dijkstra <input_file.txt>`

## Milestone 2: Static Graph GUI
* **Description:** Visual representation of the graph using `raylib`. The nodes are dynamically arranged in a circular layout. Directed edges are drawn with arrows and display their respective weights. Edges with a weight of INF (no connection) are ignored.
* **Compilation:** `make milestone2`
* **Execution:** `./sim <input_file.txt>`

## Milestone 3: Path Animation
* **Description:** Added a moving entity (orange circle) that travels across the graph along the shortest path calculated by Dijkstra. 
  * Features a clickable **Play/Stop button** at the bottom of the screen.
  * **Edge behavior:** The entity jumps along the edge. An edge with weight W is divided into W jumps, with each jump taking exactly 300 milliseconds.
  * **Node behavior:** The entity waits for exactly 1 full second at each intermediate node before continuing to the next edge.
  * Once the destination is reached, a "DESTINATION REACHED!" message is displayed.
* **Compilation:** `make milestone3`
* **Execution:** `./sim <input_file.txt>`
  
Milestone 6 Synchronization Strategy:
To ensure that no two traveler processes occupy the same node simultaneously, we implemented Named Semaphores (sem_open) acting as mutexes for each individual node. Before a child process enters its next node, it calls sem_wait on that specific node's semaphore. Upon leaving the node (after a 1-second simulation sleep), it calls sem_post to release the resource for other waiting travelers. Potential deadlocks from previous executions are prevented by clearing all semaphores using sem_unlink at initialization and termination

---

## How to Run the Project
1. Open the terminal in the project directory.
2. Clean old files: `make clean`
3. Compile the simulation: `make milestone3`
4. Run the program with your input file: `./sim input.txt`
## Milestone 4: Multiple Travelers

* **Description:** Added support for multiple traveler processes. Each traveler is represented by a separate child process created using `fork()`. All travelers independently calculate and follow their shortest path through the graph.
* **Compilation:** `make milestone4`
* **Execution:** `./sim input.txt`

---

## Milestone 5: Concurrent Traveler Simulation

* **Description:** Multiple travelers move concurrently through the graph. Each traveler follows its own shortest path while the GUI displays all active travelers simultaneously. Different colors are used to distinguish travelers.
* **Compilation:** `make milestone5`
* **Execution:** `./sim input.txt`

---

## Milestone 6: Node Synchronization

* **Description:** Added synchronization between traveler processes using POSIX named semaphores.

  * Each graph node is protected by a dedicated semaphore.
  * Only one traveler may occupy a node at any given time.
  * Travelers attempting to enter an occupied node must wait until it becomes available.
  * Waiting travelers are displayed in the GUI and reported in the terminal output.
  * Semaphores are cleaned using `sem_unlink()` to prevent stale synchronization objects between runs.

* **Compilation:** `make milestone6`

* **Execution:** `./sim input.txt`

---

## Milestone 7: Scheduling Algorithms

* **Description:** Added scheduling support for traveler admission into occupied nodes.

  * FCFS (First Come First Served) scheduler.
  * SJF (Shortest Job First) scheduler.
  * The scheduler is selected through command-line arguments.
  * The active scheduler is displayed in the GUI.
  * Different scheduling policies may produce different traveler completion orders.

* **Compilation:** `make milestone7`

* **Execution:**

```bash
./sim -schd fcfs input.txt
./sim -schd sjf input.txt
```

### Scheduling Policies

**FCFS (First Come First Served)**

* Travelers are admitted according to arrival order.

**SJF (Shortest Job First)**

* Travelers with the shortest remaining path receive higher priority.

---

## How to Run the Project

```bash
make clean

make milestone1
./dijkstra input.txt

make milestone2
./sim input.txt

make milestone3
./sim input.txt

make milestone4
./sim input.txt

make milestone5
./sim input.txt

make milestone6
./sim input.txt

make milestone7
./sim -schd fcfs input.txt
./sim -schd sjf input.txt
```
