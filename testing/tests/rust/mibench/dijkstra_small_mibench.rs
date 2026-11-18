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

// Keep the names of the original C code
#![allow(non_snake_case, non_camel_case_types, non_upper_case_globals)]

use aspis_rust_annotations::aspis;

#[unsafe(no_mangle)]
pub extern "C" fn DataCorruption_Handler() {
    print!("DATA_CORRUPTION_DETECTED");
    std::process::exit(0);
}

#[unsafe(no_mangle)]
pub extern "C" fn SigMismatch_Handler() {
    print!("SIG_MISMATCH_DETECTED");
    std::process::exit(0);
}

const NUM_NODES: i32 = 10;
const NONE: i32 = 9999;
const Q_SIZE: i32 = 100;

#[derive(Clone, Copy)]
struct NODE {
    iDist: i32,
    iPrev: i32,
}
/*
 NOTE: The original MiBench implementation used malloc() for the queue.
 ASPIS/EDDI cannot automatically duplicate heap-allocated memory.
 To enable full protection, we replaced the dynamic queue with this static
 pool (qPool), which is correctly shadow-copied thanks to the
 'to_duplicate' annotation.
*/
#[derive(Clone, Copy)]
struct QITEM {
    iNode: i32,
    iDist: i32,
    iPrev: i32,
}

// Queue pool
#[aspis(to_duplicate)]
static mut qPool: [QITEM; Q_SIZE as usize] = [QITEM {
    iNode: 0,
    iDist: 0,
    iPrev: 0,
}; Q_SIZE as usize];
static mut qHead_idx: i32 = 0;
static mut qTail_idx: i32 = 0;

#[aspis(to_duplicate)]
#[rustfmt::skip]
static mut AdjMatrix: [[i32; NUM_NODES as usize]; NUM_NODES as usize] = [
    [NONE,    1,    4, NONE, NONE, NONE, NONE, NONE, NONE, NONE],
    [NONE, NONE, NONE,    2,    5, NONE, NONE, NONE, NONE, NONE],
    [NONE, NONE, NONE, NONE,    1, NONE, NONE, NONE, NONE, NONE],
    [NONE, NONE, NONE, NONE, NONE,    3, NONE, NONE, NONE, NONE],
    [NONE, NONE, NONE, NONE, NONE,    1,    2, NONE, NONE, NONE],
    [NONE, NONE, NONE, NONE, NONE, NONE, NONE,    2, NONE, NONE],
    [NONE, NONE, NONE, NONE, NONE, NONE, NONE,    1,    3, NONE],
    [NONE, NONE, NONE, NONE, NONE, NONE, NONE, NONE, NONE,    2],
    [NONE, NONE, NONE, NONE, NONE, NONE, NONE, NONE, NONE,    1],
    [NONE, NONE, NONE, NONE, NONE, NONE, NONE, NONE, NONE, NONE],
];

#[aspis(to_duplicate)]
static mut rgnNodes: [NODE; NUM_NODES as usize] = [NODE { iDist: 0, iPrev: 0 }; NUM_NODES as usize];

// Queue count
static mut g_qCount: i32 = 0;

// Working variables
static mut ch: i32 = 0;
static mut iNode: i32 = 0;
static mut iPrev: i32 = 0;
static mut i: i32 = 0;
static mut iCost: i32 = 0;
static mut iDist: i32 = 0;

fn enqueue(node: i32, dist: i32, prev: i32) {
    unsafe {
        if g_qCount >= Q_SIZE {
            eprint!("Queue overflow.\n");
            std::process::exit(1);
        }
        qPool[qTail_idx as usize].iNode = node;
        qPool[qTail_idx as usize].iDist = dist;
        qPool[qTail_idx as usize].iPrev = prev;
        qTail_idx = (qTail_idx + 1) % Q_SIZE;
        g_qCount += 1;
    }
}

fn dequeue() {
    unsafe {
        if g_qCount > 0 {
            iNode = qPool[qHead_idx as usize].iNode;
            iDist = qPool[qHead_idx as usize].iDist;
            iPrev = qPool[qHead_idx as usize].iPrev;
            qHead_idx = (qHead_idx + 1) % Q_SIZE;
            g_qCount -= 1;
        }
    }
}

#[allow(unused_variables)]
fn dijkstra(chStart: i32, chEnd: i32) {
    unsafe {
        ch = 0;
        while ch < NUM_NODES {
            rgnNodes[ch as usize].iDist = NONE;
            rgnNodes[ch as usize].iPrev = NONE;
            ch += 1;
        }

        rgnNodes[chStart as usize].iDist = 0;
        rgnNodes[chStart as usize].iPrev = NONE;
        enqueue(chStart, 0, NONE);

        while g_qCount > 0 {
            dequeue();
            i = 0;
            while i < NUM_NODES {
                iCost = AdjMatrix[iNode as usize][i as usize];
                if iCost != NONE {
                    if (NONE == rgnNodes[i as usize].iDist)
                        || (rgnNodes[i as usize].iDist > (iCost + iDist))
                    {
                        rgnNodes[i as usize].iDist = iDist + iCost;
                        rgnNodes[i as usize].iPrev = iNode;
                        enqueue(i, iDist + iCost, iNode);
                    }
                }
                i += 1;
            }
        }
    }
}

fn main() {
    dijkstra(0, 9);

    // Shortest path from 0 to 9 on the hardcoded graph is cost 10.
    if unsafe { rgnNodes[9].iDist } == 10 {
        print!("SUCCESS");
    } else {
        print!("FAIL");
    }
}
