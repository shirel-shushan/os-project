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
