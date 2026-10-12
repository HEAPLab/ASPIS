/*
 * Telemetry link: a spacecraft-style downlink.
 *
 * The program simulates one telemetry session end to end:
 *
 *   1. sensors   a deterministic pseudo-random generator produces readings
 *                (temperature, three gyroscope axes, bus voltage, status
 *                flags) for every frame;
 *   2. packing   each reading set is serialised big-endian into a 12-byte
 *                payload, followed by a CRC-16/CCITT-FALSE;
 *   3. encoding  every byte is split into two nibbles and each nibble is
 *                protected with a Hamming(7,4) code;
 *   4. channel   the link flips one bit in some codewords, chosen by a second
 *                pseudo-random generator, as radiation on the link would;
 *   5. decoding  the receiver corrects single-bit errors, rebuilds the bytes,
 *                checks the CRC and unpacks the fields;
 *   6. check     every decoded frame is compared with the original readings.
 *
 * At most one bit per codeword is flipped, so every error is correctable and
 * a fault-free run rebuilds every frame exactly. Almost every value computed
 * along the way reaches the final output, so a corrupted bit anywhere in the
 * pipeline changes the printed checksum.
 *
 * Everything is integer arithmetic on fixed-size buffers: global arrays are
 * accessed by index rather than walked with a pointer, every local is
 * initialised at its declaration, and no memory is allocated dynamically.
 *
 * Expected output:
 *   frames=128 corrected=738 crc_errors=0 checksum=0x7b9f2e10
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

#define N_FRAMES      128
#define PAYLOAD_LEN   12                     /* bytes of sensor data      */
#define FRAME_LEN     ( PAYLOAD_LEN + 2 )    /* payload + CRC-16          */
#define CODEWORDS     ( FRAME_LEN * 2 )      /* one Hamming(7,4) per nibble */
#define FLIP_PERIOD   5                      /* about one codeword in 5 is hit */

/* value printed by the unmodified program; any change to the pipeline or
   to its inputs changes it */
#define EXPECTED_CHECKSUM 0x7b9f2e10u


/*
  One set of sensor readings
*/

typedef struct {
  int16_t  temperature;   /* hundredths of a degree Celsius */
  int16_t  gyro[ 3 ];     /* milli-degrees per second       */
  uint16_t voltage;       /* millivolts                     */
  uint8_t  status;        /* bit flags                      */
  uint8_t  sequence;      /* frame counter, wraps at 256    */
} reading_t;


/*
  Global state: the readings as sampled on board and as rebuilt on ground,
  plus the session statistics.
*/

__attribute__((annotate("to_harden")))
reading_t sent[ N_FRAMES ];

__attribute__((annotate("to_harden")))
reading_t received[ N_FRAMES ];

__attribute__((annotate("to_harden")))
uint32_t sensor_state = 0x12345678u;

__attribute__((annotate("to_harden")))
uint32_t channel_state = 0x9E3779B9u;

__attribute__((annotate("to_harden")))
unsigned bits_flipped = 0;

__attribute__((annotate("to_harden")))
unsigned bits_corrected = 0;

__attribute__((annotate("to_harden")))
unsigned crc_errors = 0;


/*
  Deterministic pseudo-random generator (xorshift32)
*/

__attribute__((annotate("to_harden")))
void xorshift32( uint32_t *state, uint32_t *out )
{
  uint32_t x = *state;
  x ^= x << 13;
  x ^= x >> 17;
  x ^= x << 5;
  *state = x;
  *out = x;
}


/*
  1. Sensors
*/

__attribute__((annotate("to_harden")))
void sample_sensors( int index, reading_t *r )
{
  uint32_t noise = 0;
  int axis = 0;

  xorshift32( &sensor_state, &noise );

  /* slow drifts plus small noise, all in integer arithmetic */
  r->temperature = ( int16_t ) ( 2150 + index * 3 - ( int ) ( noise & 0x3F ) );
  for ( axis = 0; axis < 3; axis++ ) {
    xorshift32( &sensor_state, &noise );
    r->gyro[ axis ] = ( int16_t ) ( ( int ) ( noise & 0x7FF ) - 1024 );
  }
  xorshift32( &sensor_state, &noise );
  r->voltage = ( uint16_t ) ( 28000 + ( noise & 0x1FF ) );
  r->status = ( uint8_t ) ( ( index % 16 == 0 ? 0x80 : 0 )   /* heartbeat   */
                            | ( r->voltage > 28400 ? 0x01 : 0 )  /* high bus   */
                            | ( r->temperature < 2100 ? 0x02 : 0 ) );
  r->sequence = ( uint8_t ) index;
}


/*
  2. Packing: big-endian serialisation and CRC-16/CCITT-FALSE
*/

__attribute__((annotate("to_harden")))
void put16( uint8_t buf[], int pos, uint16_t value )
{
  buf[ pos ] = ( uint8_t ) ( value >> 8 );
  buf[ pos + 1 ] = ( uint8_t ) ( value & 0xFF );
}


__attribute__((annotate("to_harden")))
uint16_t get16( const uint8_t buf[], int pos )
{
  return ( uint16_t ) ( ( buf[ pos ] << 8 ) | buf[ pos + 1 ] );
}


__attribute__((annotate("to_harden")))
uint16_t crc16( const uint8_t buf[], int len )
{
  uint16_t crc = 0xFFFF;
  int i = 0;
  int bit = 0;

  for ( i = 0; i < len; i++ ) {
    crc ^= ( uint16_t ) ( buf[ i ] << 8 );
    for ( bit = 0; bit < 8; bit++ ) {
      if ( crc & 0x8000 )
        crc = ( uint16_t ) ( ( crc << 1 ) ^ 0x1021 );
      else
        crc = ( uint16_t ) ( crc << 1 );
    }
  }
  return crc;
}


__attribute__((annotate("to_harden")))
void pack_frame( const reading_t *r, uint8_t frame[] )
{
  put16( frame, 0, ( uint16_t ) r->temperature );
  put16( frame, 2, ( uint16_t ) r->gyro[ 0 ] );
  put16( frame, 4, ( uint16_t ) r->gyro[ 1 ] );
  put16( frame, 6, ( uint16_t ) r->gyro[ 2 ] );
  put16( frame, 8, r->voltage );
  frame[ 10 ] = r->status;
  frame[ 11 ] = r->sequence;
  put16( frame, PAYLOAD_LEN, crc16( frame, PAYLOAD_LEN ) );
}


__attribute__((annotate("to_harden")))
void unpack_frame( const uint8_t frame[], reading_t *r )
{
  r->temperature = ( int16_t ) get16( frame, 0 );
  r->gyro[ 0 ] = ( int16_t ) get16( frame, 2 );
  r->gyro[ 1 ] = ( int16_t ) get16( frame, 4 );
  r->gyro[ 2 ] = ( int16_t ) get16( frame, 6 );
  r->voltage = get16( frame, 8 );
  r->status = frame[ 10 ];
  r->sequence = frame[ 11 ];
}


/*
  3. Hamming(7,4): 4 data bits d1..d4 and 3 parity bits p1..p3, laid out
     as bits 1..7 = p1 p2 d1 p3 d2 d3 d4 (bit 1 is the least significant).
*/

__attribute__((annotate("to_harden")))
uint8_t hamming_encode( uint8_t nibble )
{
  uint8_t d1 = ( nibble >> 0 ) & 1;
  uint8_t d2 = ( nibble >> 1 ) & 1;
  uint8_t d3 = ( nibble >> 2 ) & 1;
  uint8_t d4 = ( nibble >> 3 ) & 1;
  uint8_t p1 = d1 ^ d2 ^ d4;
  uint8_t p2 = d1 ^ d3 ^ d4;
  uint8_t p3 = d2 ^ d3 ^ d4;

  return ( uint8_t ) ( p1 | ( p2 << 1 ) | ( d1 << 2 ) | ( p3 << 3 )
                       | ( d2 << 4 ) | ( d3 << 5 ) | ( d4 << 6 ) );
}


/* returns the corrected nibble; counts the corrections it makes */
__attribute__((annotate("to_harden")))
uint8_t hamming_decode( uint8_t code )
{
  uint8_t b[ 8 ] = { 0 };
  uint8_t syndrome = 0;
  int i = 0;

  for ( i = 1; i <= 7; i++ )
    b[ i ] = ( code >> ( i - 1 ) ) & 1;

  syndrome = ( uint8_t ) ( ( b[ 1 ] ^ b[ 3 ] ^ b[ 5 ] ^ b[ 7 ] )
                           | ( ( b[ 2 ] ^ b[ 3 ] ^ b[ 6 ] ^ b[ 7 ] ) << 1 )
                           | ( ( b[ 4 ] ^ b[ 5 ] ^ b[ 6 ] ^ b[ 7 ] ) << 2 ) );
  if ( syndrome != 0 ) {
    b[ syndrome ] ^= 1;
    bits_corrected++;
  }
  return ( uint8_t ) ( b[ 3 ] | ( b[ 5 ] << 1 ) | ( b[ 6 ] << 2 ) | ( b[ 7 ] << 3 ) );
}


__attribute__((annotate("to_harden")))
void encode_frame( const uint8_t frame[], uint8_t code[] )
{
  int i = 0;

  for ( i = 0; i < FRAME_LEN; i++ ) {
    code[ 2 * i ] = hamming_encode( ( uint8_t ) ( frame[ i ] >> 4 ) );
    code[ 2 * i + 1 ] = hamming_encode( ( uint8_t ) ( frame[ i ] & 0x0F ) );
  }
}


__attribute__((annotate("to_harden")))
void decode_frame( const uint8_t code[], uint8_t frame[] )
{
  int i = 0;

  for ( i = 0; i < FRAME_LEN; i++ )
    frame[ i ] = ( uint8_t ) ( ( hamming_decode( code[ 2 * i ] ) << 4 )
                               | hamming_decode( code[ 2 * i + 1 ] ) );
}


/*
  4. Channel: flips at most one bit per codeword, so Hamming can always
     correct it
*/

__attribute__((annotate("to_harden")))
void channel( uint8_t code[] )
{
  int i = 0;
  uint32_t r = 0;

  for ( i = 0; i < CODEWORDS; i++ ) {
    xorshift32( &channel_state, &r );
    if ( r % FLIP_PERIOD == 0 ) {
      code[ i ] ^= ( uint8_t ) ( 1u << ( ( r >> 8 ) % 7 ) );
      bits_flipped++;
    }
  }
}


/*
  5-6. Session: send every frame through the link and rebuild it on ground
*/

__attribute__((annotate("to_harden")))
int same_reading( const reading_t *a, const reading_t *b )
{
  return a->temperature == b->temperature
         && a->gyro[ 0 ] == b->gyro[ 0 ]
         && a->gyro[ 1 ] == b->gyro[ 1 ]
         && a->gyro[ 2 ] == b->gyro[ 2 ]
         && a->voltage == b->voltage
         && a->status == b->status
         && a->sequence == b->sequence;
}


/* folds a frame into the session checksum (FNV-1a, 32 bit) */
__attribute__((annotate("to_harden")))
uint32_t fold( uint32_t hash, const uint8_t frame[] )
{
  int i = 0;

  for ( i = 0; i < FRAME_LEN; i++ ) {
    hash ^= frame[ i ];
    hash *= 16777619u;
  }
  return hash;
}


int main( void )
{
  uint8_t frame[ FRAME_LEN ] = { 0 };
  uint8_t code[ CODEWORDS ] = { 0 };
  uint8_t rebuilt[ FRAME_LEN ] = { 0 };
  uint32_t checksum = 2166136261u;
  int mismatches = 0;
  int i = 0;

  for ( i = 0; i < N_FRAMES; i++ ) {
    sample_sensors( i, &sent[ i ] );

    pack_frame( &sent[ i ], frame );
    encode_frame( frame, code );
    channel( code );
    decode_frame( code, rebuilt );

    if ( crc16( rebuilt, PAYLOAD_LEN ) != get16( rebuilt, PAYLOAD_LEN ) )
      crc_errors++;
    unpack_frame( rebuilt, &received[ i ] );
    if ( !same_reading( &sent[ i ], &received[ i ] ) )
      mismatches++;

    checksum = fold( checksum, rebuilt );
  }

  printf( "frames=%d corrected=%u crc_errors=%u checksum=0x%08x\n",
          N_FRAMES, bits_corrected, crc_errors, ( unsigned ) checksum );

  if ( mismatches == 0 && crc_errors == 0
       && bits_corrected == bits_flipped && bits_flipped > 0
       && checksum == EXPECTED_CHECKSUM )
    printf( "SUCCESS" );
  else
    printf( "FAIL" );

  return 0;
}

// expected output
// frames=128 corrected=738 crc_errors=0 checksum=0x7b9f2e10
// SUCCESS