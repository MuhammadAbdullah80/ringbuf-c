#include "ringbuf.h"

#include <string.h>

/*
 * head plus count, rather than head plus tail.
 *
 * With two indices, head == tail means both "empty" and "full", and every
 * implementation that takes that route ends up either sacrificing one slot or
 * carrying a separate flag. Storing the count instead makes the state
 * unambiguous and the capacity fully usable.
 */

static size_t rb_tail(const ringbuf *rb)
{
    /* count <= capacity, so head + capacity - count cannot underflow. */
    return (rb->head + rb->capacity - rb->count) % rb->capacity;
}

int ringbuf_init(ringbuf *rb, void *storage, size_t capacity)
{
    if (rb == NULL || storage == NULL || capacity == 0) {
        return -1;
    }
    rb->data = (unsigned char *)storage;
    rb->capacity = capacity;
    rb->head = 0;
    rb->count = 0;
    return 0;
}

void ringbuf_clear(ringbuf *rb)
{
    if (rb == NULL) {
        return;
    }
    rb->head = 0;
    rb->count = 0;
}

size_t ringbuf_capacity(const ringbuf *rb)
{
    return rb == NULL ? 0 : rb->capacity;
}

size_t ringbuf_size(const ringbuf *rb)
{
    return rb == NULL ? 0 : rb->count;
}

size_t ringbuf_free_space(const ringbuf *rb)
{
    return rb == NULL ? 0 : rb->capacity - rb->count;
}

int ringbuf_is_empty(const ringbuf *rb)
{
    return rb == NULL || rb->count == 0;
}

int ringbuf_is_full(const ringbuf *rb)
{
    return rb != NULL && rb->count == rb->capacity;
}

size_t ringbuf_write(ringbuf *rb, const void *src, size_t n)
{
    const unsigned char *in;
    size_t space;
    size_t first;

    if (rb == NULL || src == NULL || rb->capacity == 0) {
        return 0;
    }

    space = rb->capacity - rb->count;
    if (n > space) {
        n = space;
    }
    if (n == 0) {
        return 0;
    }

    in = (const unsigned char *)src;

    /* Split the copy at the wrap point; the second memcpy is a no-op when the
     * run does not wrap, and memcpy with a length of 0 is well defined. */
    first = rb->capacity - rb->head;
    if (first > n) {
        first = n;
    }
    memcpy(rb->data + rb->head, in, first);
    memcpy(rb->data, in + first, n - first);

    rb->head = (rb->head + n) % rb->capacity;
    rb->count += n;
    return n;
}

size_t ringbuf_overwrite(ringbuf *rb, const void *src, size_t n)
{
    const unsigned char *in;
    size_t dropped = 0;
    size_t space;

    if (rb == NULL || src == NULL || rb->capacity == 0) {
        return 0;
    }
    if (n == 0) {
        return 0;
    }

    in = (const unsigned char *)src;

    /* Only the final `capacity` bytes of src can possibly survive, so skip the
     * rest instead of copying it in and immediately evicting it. */
    if (n > rb->capacity) {
        dropped += n - rb->capacity;
        in += n - rb->capacity;
        n = rb->capacity;
    }

    space = rb->capacity - rb->count;
    if (n > space) {
        dropped += ringbuf_discard(rb, n - space);
    }

    ringbuf_write(rb, in, n);
    return dropped;
}

size_t ringbuf_peek(const ringbuf *rb, void *dst, size_t n)
{
    unsigned char *out;
    size_t tail;
    size_t first;

    if (rb == NULL || dst == NULL || rb->capacity == 0) {
        return 0;
    }

    if (n > rb->count) {
        n = rb->count;
    }
    if (n == 0) {
        return 0;
    }

    out = (unsigned char *)dst;
    tail = rb_tail(rb);

    first = rb->capacity - tail;
    if (first > n) {
        first = n;
    }
    memcpy(out, rb->data + tail, first);
    memcpy(out + first, rb->data, n - first);

    return n;
}

size_t ringbuf_read(ringbuf *rb, void *dst, size_t n)
{
    size_t got;

    if (rb == NULL) {
        return 0;
    }
    got = ringbuf_peek(rb, dst, n);
    rb->count -= got;
    return got;
}

size_t ringbuf_discard(ringbuf *rb, size_t n)
{
    if (rb == NULL) {
        return 0;
    }
    if (n > rb->count) {
        n = rb->count;
    }
    rb->count -= n;
    return n;
}
