#ifndef _OR32_SETUP_H
#define _OR32_SETUP_H

#define COMMAND_LINE_SIZE	256

#ifndef __ASSEMBLY__
#ifdef __KERNEL__

extern char cmd_line[COMMAND_LINE_SIZE];

#endif
#endif


#endif
