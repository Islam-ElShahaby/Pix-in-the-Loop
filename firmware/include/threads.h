#ifndef CONTROLLER_THREADS_H
#define CONTROLLER_THREADS_H

#include <zephyr/kernel.h>
#include "types.h"

/* Extern Message Queues */
extern struct k_msgq io_msgq;
extern struct k_msgq spi1_msgq;
extern struct k_msgq spi2_msgq;

/* Thread Entry Prototypes */
void io_cmd_dispatcher(void *p1, void *p2, void *p3);
void spi_worker_entry(void *p1, void *p2, void *p3);

#endif /* CONTROLLER_THREADS_H */