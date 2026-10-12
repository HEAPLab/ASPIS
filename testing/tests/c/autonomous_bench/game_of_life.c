/*
 * Conway's Game of Life.
 *
 * A square grid of cells, each alive or dead, evolves in discrete
 * generations. At every step each cell looks at its eight neighbours:
 *   - a live cell with 2 or 3 live neighbours survives;
 *   - a dead cell with exactly 3 live neighbours becomes alive;
 *   - every other cell is dead in the next generation.
 * The grid wraps around at the borders (a torus), so every cell has
 * exactly eight neighbours.
 *
 * The program runs in two phases:
 *   1. self-check: a single glider is placed on an empty grid. A glider
 *      moves one cell diagonally every 4 generations, so after 4*K
 *      generations it must be the same 5-cell shape shifted by (K, K).
 *      This checks the rules without relying on a precomputed value.
 *   2. simulation: the grid is filled pseudo-randomly from a fixed seed and
 *      evolved for GENERATIONS steps. The population of every generation
 *      and the final grid are folded into a checksum.
 *
 * A single wrong cell spreads to its neighbours in the following
 * generations, so a fault that corrupts the grid early almost always
 * changes the final checksum.
 *
 * The grids are fixed-size global arrays, accessed by index and copied cell
 * by cell rather than with memcpy; every local is initialised at its
 * declaration and no memory is allocated dynamically. The result of the
 * glider self-check is printed as an integer rather than as one of two
 * strings chosen with the conditional operator.
 *
 * Expected output:
 *   glider=1 generations=200 population=82 checksum=0x9f1802e7
 *   SUCCESS
 *
 * License: MIT, like the rest of the repository.
 */

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>


/*
  Fault handlers
*/

void DataCorruption_Handler( void )
{
  fprintf( stderr, "FAULT_DETECTED: DataCorruption\n" );
  exit( 2 );
}


void SigMismatch_Handler( void )
{
  fprintf( stderr, "FAULT_DETECTED: SigMismatch\n" );
  exit( 3 );
}


/*
  Configuration
*/

#define SIZE          32      /* the grid is SIZE x SIZE cells   */
#define GENERATIONS   200     /* steps of the random simulation  */
#define GLIDER_SHIFT  8       /* the glider travels (8, 8)       */

/* value printed by the unmodified program; any change to the rules or to
   the initial grid changes it */
#define EXPECTED_CHECKSUM 0x9f1802e7u


/*
  Global state
*/

__attribute__((annotate("to_harden")))
uint8_t grid[ SIZE ][ SIZE ];

__attribute__((annotate("to_harden")))
uint8_t next_grid[ SIZE ][ SIZE ];

__attribute__((annotate("to_harden")))
uint32_t rng_state = 0x2545F491u;


/*
  Deterministic pseudo-random generator (xorshift32)
*/

__attribute__((annotate("to_harden")))
uint32_t xorshift32( void )
{
  uint32_t x = rng_state;
  x ^= x << 13;
  x ^= x >> 17;
  x ^= x << 5;
  rng_state = x;
  return x;
}


/*
  Grid helpers
*/

__attribute__((annotate("to_harden")))
void clear_grid( void )
{
  int r = 0;
  int c = 0;

  for ( r = 0; r < SIZE; r++ )
    for ( c = 0; c < SIZE; c++ )
      grid[ r ][ c ] = 0;
}


/* about 3 cells in 8 start alive */
__attribute__((annotate("to_harden")))
void seed_grid( void )
{
  int r = 0;
  int c = 0;

  for ( r = 0; r < SIZE; r++ )
    for ( c = 0; c < SIZE; c++ )
      grid[ r ][ c ] = ( uint8_t ) ( ( xorshift32() & 7 ) < 3 ? 1 : 0 );
}


/* coordinates wrap around the borders */
__attribute__((annotate("to_harden")))
int wrap( int i )
{
  return ( i + SIZE ) % SIZE;
}


__attribute__((annotate("to_harden")))
int count_neighbours( int r, int c )
{
  int count = 0;
  int dr = 0;
  int dc = 0;

  for ( dr = -1; dr <= 1; dr++ ) {
    for ( dc = -1; dc <= 1; dc++ ) {
      if ( dr == 0 && dc == 0 )
        continue;
      count += grid[ wrap( r + dr ) ][ wrap( c + dc ) ];
    }
  }
  return count;
}


/* computes the next generation, then copies it back into grid */
__attribute__((annotate("to_harden")))
void step( void )
{
  int r = 0;
  int c = 0;
  int n = 0;

  for ( r = 0; r < SIZE; r++ ) {
    for ( c = 0; c < SIZE; c++ ) {
      n = count_neighbours( r, c );
      if ( grid[ r ][ c ] )
        next_grid[ r ][ c ] = ( uint8_t ) ( n == 2 || n == 3 );
      else
        next_grid[ r ][ c ] = ( uint8_t ) ( n == 3 );
    }
  }

  for ( r = 0; r < SIZE; r++ )
    for ( c = 0; c < SIZE; c++ )
      grid[ r ][ c ] = next_grid[ r ][ c ];
}


__attribute__((annotate("to_harden")))
int population( void )
{
  int r = 0;
  int c = 0;
  int alive = 0;

  for ( r = 0; r < SIZE; r++ )
    for ( c = 0; c < SIZE; c++ )
      alive += grid[ r ][ c ];
  return alive;
}


/*
  Phase 1: glider self-check

  The glider below moves towards increasing row and column:
      . X .
      . . X
      X X X
*/

__attribute__((annotate("to_harden")))
void place_glider( int r, int c )
{
  grid[ wrap( r ) ][ wrap( c + 1 ) ] = 1;
  grid[ wrap( r + 1 ) ][ wrap( c + 2 ) ] = 1;
  grid[ wrap( r + 2 ) ][ wrap( c ) ] = 1;
  grid[ wrap( r + 2 ) ][ wrap( c + 1 ) ] = 1;
  grid[ wrap( r + 2 ) ][ wrap( c + 2 ) ] = 1;
}


/* returns 0 if the glider ended up exactly where it should */
__attribute__((annotate("to_harden")))
int glider_check( void )
{
  int start = 3;
  int end = start + GLIDER_SHIFT;
  int g = 0;

  clear_grid();
  place_glider( start, start );
  for ( g = 0; g < 4 * GLIDER_SHIFT; g++ )
    step();

  if ( population() != 5 )
    return 1;
  if ( !grid[ wrap( end ) ][ wrap( end + 1 ) ]
       || !grid[ wrap( end + 1 ) ][ wrap( end + 2 ) ]
       || !grid[ wrap( end + 2 ) ][ wrap( end ) ]
       || !grid[ wrap( end + 2 ) ][ wrap( end + 1 ) ]
       || !grid[ wrap( end + 2 ) ][ wrap( end + 2 ) ] )
    return 1;
  return 0;
}


/*
  Phase 2: random simulation
*/

/* FNV-1a, 32 bit, one byte at a time */
__attribute__((annotate("to_harden")))
uint32_t fold_byte( uint32_t hash, uint8_t byte )
{
  hash ^= byte;
  hash *= 16777619u;
  return hash;
}


__attribute__((annotate("to_harden")))
uint32_t fold_grid( uint32_t hash )
{
  int r = 0;
  int c = 0;

  for ( r = 0; r < SIZE; r++ )
    for ( c = 0; c < SIZE; c++ )
      hash = fold_byte( hash, grid[ r ][ c ] );
  return hash;
}


int main( void )
{
  uint32_t checksum = 2166136261u;
  int glider_ok = 0;
  int alive = 0;
  int g = 0;

  glider_ok = ( glider_check() == 0 );

  seed_grid();
  for ( g = 0; g < GENERATIONS; g++ ) {
    step();
    alive = population();
    checksum = fold_byte( checksum, ( uint8_t ) ( alive & 0xFF ) );
    checksum = fold_byte( checksum, ( uint8_t ) ( alive >> 8 ) );
  }
  checksum = fold_grid( checksum );

  printf( "glider=%d generations=%d population=%d checksum=0x%08x\n",
          glider_ok, GENERATIONS, alive, ( unsigned ) checksum );

  if ( glider_ok && checksum == EXPECTED_CHECKSUM )
    printf( "SUCCESS" );
  else
    printf( "FAIL" );

  return 0;
}

// expected output
// glider=1 generations=200 population=82 checksum=0x9f1802e7
// SUCCESS