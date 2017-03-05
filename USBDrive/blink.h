/*
 * Routines to blink the DAQ V1.0 lights.
 */

/*
 * Initializes TRIS bits.
 * ADCON registers must be set to not interfere.
 * Blinks both LEDs once, slowly.
 */
void
blink_init(void);

/*
 * Success display routine.
 * Turns Green on and Red Off.  Never returns.
 */
void
blink_ok(void);

/*
 * Signal a fatal error.
 *
 * Turns on Red, off Green.
 * Then blinks Green 'code' times.
 * Turns both off
 * Repeat forever.
 */
void
blink_error(unsigned char code);

/* 
 * Debug blink.
 * Same as error blink but returns after one pass.
 */
void
blink_debug(unsigned char code);
