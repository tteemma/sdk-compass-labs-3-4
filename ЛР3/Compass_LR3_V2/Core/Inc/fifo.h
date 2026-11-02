#ifndef FIFO_H
#define FIFO_H
#include <stdbool.h>
#include <stdint.h>
#ifndef FIFO_CAPACITY
#define FIFO_CAPACITY 512u
#endif
_Static_assert(FIFO_CAPACITY >= 2 && FIFO_CAPACITY <= 32768 && (FIFO_CAPACITY & (FIFO_CAPACITY-1)) == 0, "FIFO size must be a power of two, 2..32768");
typedef struct { uint8_t data[FIFO_CAPACITY]; volatile uint16_t head, tail; } Fifo;
static inline bool fifo_put(Fifo *q, uint8_t c) {
    uint16_t h=q->head;
    if ((uint16_t)(h-q->tail) == FIFO_CAPACITY) return false;
    q->data[h & (FIFO_CAPACITY-1)] = c;
    __asm volatile("" ::: "memory"); q->head=(uint16_t)(h+1); return true;
}
static inline bool fifo_get(Fifo *q, uint8_t *c) {
    uint16_t t=q->tail;
    if (t == q->head) return false;
    *c=q->data[t & (FIFO_CAPACITY-1)];
    __asm volatile("" ::: "memory"); q->tail=(uint16_t)(t+1); return true;
}
static inline bool fifo_empty(const Fifo *q) { return q->head==q->tail; }
static inline unsigned fifo_space(const Fifo *q) { return FIFO_CAPACITY-(uint16_t)(q->head-q->tail); }
#endif
