/*
 dijkstra_small_mibench.c — ASPIS adaptation of MiBench dijkstra/small.

 Original: MiBench benchmark suite, University of Michigan.
 Algorithm: Dijkstra's shortest path (BFS with FIFO relaxation queue).

 Graph (directed, 10 nodes, NONE = no direct edge):
   0→1:1  0→2:4
   1→3:2  1→4:5
   2→4:1
   3→5:3
   4→5:1  4→6:2
   5→7:2
   6→7:1  6→8:3
   7→9:2
   8→9:1

 Shortest path 0→9 has cost 10
*/

#include <stdio.h>
#include <stdlib.h>

void DataCorruption_Handler(void) { printf("DATA_CORRUPTION_DETECTED"); exit(0); }
void SigMismatch_Handler(void)    { printf("SIG_MISMATCH_DETECTED");    exit(0); }

#define NUM_NODES  10
#define NONE       9999
#define Q_SIZE     100

typedef struct { int iDist; int iPrev; } NODE;
/*
 NOTE: The original MiBench implementation used malloc() for the queue.
 ASPIS/EDDI cannot automatically duplicate heap-allocated memory.
 To enable full protection, we replaced the dynamic queue with this static
 pool (qPool), which is correctly shadow-copied thanks to the
 'to_duplicate' annotation.
*/
typedef struct { int iNode; int iDist; int iPrev; } QITEM;

// Queue pool
__attribute__((annotate("to_duplicate")))
static QITEM qPool[Q_SIZE];
static int qHead_idx = 0;
static int qTail_idx = 0;

__attribute__((annotate("to_duplicate")))
static int AdjMatrix[NUM_NODES][NUM_NODES] = {
    {NONE,    1,    4, NONE, NONE, NONE, NONE, NONE, NONE, NONE},
    {NONE, NONE, NONE,    2,    5, NONE, NONE, NONE, NONE, NONE},
    {NONE, NONE, NONE, NONE,    1, NONE, NONE, NONE, NONE, NONE},
    {NONE, NONE, NONE, NONE, NONE,    3, NONE, NONE, NONE, NONE},
    {NONE, NONE, NONE, NONE, NONE,    1,    2, NONE, NONE, NONE},
    {NONE, NONE, NONE, NONE, NONE, NONE, NONE,    2, NONE, NONE},
    {NONE, NONE, NONE, NONE, NONE, NONE, NONE,    1,    3, NONE},
    {NONE, NONE, NONE, NONE, NONE, NONE, NONE, NONE, NONE,    2},
    {NONE, NONE, NONE, NONE, NONE, NONE, NONE, NONE, NONE,    1},
    {NONE, NONE, NONE, NONE, NONE, NONE, NONE, NONE, NONE, NONE},
};

__attribute__((annotate("to_duplicate")))
static NODE rgnNodes[NUM_NODES];

// Queue count
static int g_qCount = 0;

// Working variables
static int ch;
static int iNode, iPrev;
static int i, iCost, iDist;

static void enqueue(int node, int dist, int prev) {
    if (g_qCount >= Q_SIZE) { fprintf(stderr, "Queue overflow.\n"); exit(1); }
    qPool[qTail_idx].iNode = node;
    qPool[qTail_idx].iDist = dist;
    qPool[qTail_idx].iPrev = prev;
    qTail_idx = (qTail_idx + 1) % Q_SIZE;
    g_qCount++;
}

static void dequeue(void) {
    if (g_qCount > 0) {
        iNode = qPool[qHead_idx].iNode;
        iDist = qPool[qHead_idx].iDist;
        iPrev = qPool[qHead_idx].iPrev;
        qHead_idx = (qHead_idx + 1) % Q_SIZE;
        g_qCount--;
    }
}

static void dijkstra(int chStart, int chEnd) {
    for (ch = 0; ch < NUM_NODES; ch++) {
        rgnNodes[ch].iDist = NONE;
        rgnNodes[ch].iPrev = NONE;
    }

    rgnNodes[chStart].iDist = 0;
    rgnNodes[chStart].iPrev = NONE;
    enqueue(chStart, 0, NONE);

    while (g_qCount > 0) {
        dequeue();
        for (i = 0; i < NUM_NODES; i++) {
            if ((iCost = AdjMatrix[iNode][i]) != NONE) {
                if ((NONE == rgnNodes[i].iDist) ||
                    (rgnNodes[i].iDist > (iCost + iDist))) {
                    rgnNodes[i].iDist = iDist + iCost;
                    rgnNodes[i].iPrev = iNode;
                    enqueue(i, iDist + iCost, iNode);
                }
            }
        }
    }
}

int main(void) {
    dijkstra(0, 9);

    // Shortest path from 0 to 9 on the hardcoded graph is cost 10.
    if (rgnNodes[9].iDist == 10) { 
        printf("SUCCESS");
    } else {
        printf("FAIL");
    }
    return 0;
}
