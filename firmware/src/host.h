/*
 * host.h
 *
 *  Created on: Mar 23, 2013
 *      Author: nchen
 */

#ifndef HOST_H_
#define HOST_H_

/*
 * All host transfers use 16-bit words,
 * and start with the command and target ID in one word
 */

typedef uint16_t HOST_DATA;
#define HOST_DATA_MASK  UINT16_MAX

#define HOST_COMMAND_SHIFT          12
#define HOST_COMMAND_LENGTH_SHIFT   14
#define HOST_ID_MASK                ((1 << HOST_COMMAND_SHIFT) - 1)
#define HOST_ID_ALL                 0
#define HOST_COMMAND_MASK           (HOST_DATA_MASK & ~HOST_ID_MASK)
#define HOST_COMMAND_LENGTH_MASK    (HOST_DATA_MASK & ~HOST_ID_MASK)

/* Length of data associated with command
 * HOST_COMMAND_VARIABLE commands have the following structure,
 *  0000: Length of data in words excluding length word
 *  0002: First data word
 *  0004: Second data word
 *  ....
 */
enum HOST_COMMAND_LENGTH {
    HOST_COMMAND_0 = 0,
    HOST_COMMAND_1 = (1 << HOST_COMMAND_LENGTH_SHIFT),
    HOST_COMMAND_2 = (2 << HOST_COMMAND_LENGTH_SHIFT),
    HOST_COMMAND_VARIABLE = (3 << HOST_COMMAND_LENGTH_SHIFT)
};

enum HOST_COMMAND {
    HOST_NOP = (0 << HOST_COMMAND_SHIFT) | HOST_COMMAND_0,
    HOST_ID = (1 << HOST_COMMAND_SHIFT) | HOST_COMMAND_0,
    HOST_FLIP = (2 << HOST_COMMAND_SHIFT) | HOST_COMMAND_0,

    HOST_BLANK = (0 << HOST_COMMAND_SHIFT) | HOST_COMMAND_1,
    HOST_IREF = (1 << HOST_COMMAND_SHIFT) | HOST_COMMAND_1,

    HOST_FRAME = (0 << HOST_COMMAND_SHIFT) | HOST_COMMAND_VARIABLE,
};

enum HOST_COMMAND_BLANK {
    HOST_BLANK_ON,
    HOST_BLANK_OFF
};

#endif /* HOST_H_ */
