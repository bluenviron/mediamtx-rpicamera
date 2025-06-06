#ifndef __PIPE_H__
#define __PIPE_H__

#include <stdbool.h>
#include <stdint.h>

void pipe_write_error(int fd, const char *format, ...);
void pipe_write_ready(int fd);
void pipe_write_data(int fd, const uint8_t *mapped, uint32_t size, uint64_t ts);
void pipe_write_secondary_data(int fd, const uint8_t *mapped, uint32_t size, uint64_t ts);
uint32_t pipe_read(int fd, uint8_t **pbuf);

#endif
