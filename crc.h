#ifndef _MANDELGTK_CRC_H
#define _MANDELGTK_CRC_H

#include <stdint.h>

unsigned long update_crc (uint32_t crc, const unsigned char *buf, int len);

#endif /* _MANDELGTK_CRC_H */
