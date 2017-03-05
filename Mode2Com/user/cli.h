extern char cmd_state;
#define	CMD_OK	99	// command state for OK commands.
#define	CMD_ERR	98	// command state for not OK commands.

/*
 * Send a character out.
 * Returns true if the character was queued to send, false if flow-control
 * prevented sending the character.
 */
char
cli_putc(char c);

/*
 * Syntax Error.
 * All error codes are a single letter.
 *
 * E: unknown command
 * C: command echo (debug mode)
 * 1: cannot parse first argument
 * 2: cannot parse second argument
 */
void
cmdError(char c);

/*
 * Put 2 characters
 */
void
puthex(byte b);

/*
 * Returns true if there is input pending.
 * If flag is true, discards pending input.
 */
unsigned char
cli_input_pending(unsigned char flag);

/*
 * CLI functions.
 */

void
cli_eprom_read(byte addr);

void
cli_eprom_write(byte addr, byte value);

void
cli_eprom_next(void);

void
cli_eprom_msg_report(void);

void
cli_eprom_errors(byte flag);

void
cli_eprom_modes(byte mode, char *name);

void
cli_tod_report(void);

void
cli_tod_seconds(void);

void
cli_tod_hours(byte hours, byte minutes);

void
cli_tod_month(byte month, byte day);

void
cli_tod_year(byte year);

void
cli_tod_cproc(void);

void
cli_adc(byte channel);

void
cli_adc_battery(void);

unsigned char
make_dirfile(const char **path, char *dirname);

void
cli_port(byte channel);

void
cli_calibrate_set(byte bias);

void
cli_calibrate_echo(void);

void
cli_calibrate_tod(void);

void
cli_calibrate_GPS(void);

void
cli_sensors(void);
