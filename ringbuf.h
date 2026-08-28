/*
 * ringbuf - a fixed-capacity circular byte buffer.
 *
 * The caller owns the storage. Nothing here allocates, which is what makes it
 * usable from an interrupt handler or a freestanding target:
 *
 *     unsigned char storage[256];
 *     ringbuf rb;
 *     ringbuf_init(&rb, storage, sizeof storage);
 *
 * Capacity does not have to be a power of two. Implementations that require one
 * do so to replace a modulo with a mask; this one keeps the modulo and accepts
 * any capacity, because a caller sizing a buffer to a protocol's maximum frame
 * should not have to round up to 512 to store 300 bytes.
 *
 * Not thread-safe. A single-producer/single-consumer arrangement is safe only
 * with the appropriate atomics and memory barriers, which are deliberately out
 * of scope here.
 */
#ifndef RINGBUF_H
#define RINGBUF_H

#include <stddef.h>

typedef struct {
    unsigned char *data;
    size_t capacity;
    size_t head;  /* index of the next byte to be written */
    size_t count; /* bytes currently stored */
} ringbuf;

/*
 * Points rb at capacity bytes of caller-owned storage.
 *
 * Returns 0 on success, -1 if rb or storage is NULL or capacity is 0. A
 * zero-capacity ring is rejected rather than silently accepted as a buffer that
 * discards everything written to it.
 */
int ringbuf_init(ringbuf *rb, void *storage, size_t capacity);

/* Drops all stored bytes. The storage is not cleared. */
void ringbuf_clear(ringbuf *rb);

size_t ringbuf_capacity(const ringbuf *rb);
size_t ringbuf_size(const ringbuf *rb);
size_t ringbuf_free_space(const ringbuf *rb);
int ringbuf_is_empty(const ringbuf *rb);
int ringbuf_is_full(const ringbuf *rb);

/*
 * Copies up to n bytes in, stopping when full.
 *
 * Returns the number of bytes actually stored, which is less than n when the
 * buffer filled. The caller must check: a partial write that reports success
 * would silently truncate a frame.
 */
size_t ringbuf_write(ringbuf *rb, const void *src, size_t n);

/*
 * Copies n bytes in, discarding the oldest bytes to make room.
 *
 * For a buffer holding the most recent output - a log tail, a scope trace -
 * where losing old data is the point. Returns the number of bytes discarded,
 * including any part of src itself that could never fit: writing 300 bytes into
 * a 256-byte ring keeps the last 256 and reports 44 dropped.
 */
size_t ringbuf_overwrite(ringbuf *rb, const void *src, size_t n);

/* Copies up to n bytes out and removes them. Returns bytes copied. */
size_t ringbuf_read(ringbuf *rb, void *dst, size_t n);

/* Copies up to n bytes out without removing them. Returns bytes copied. */
size_t ringbuf_peek(const ringbuf *rb, void *dst, size_t n);

/* Removes up to n bytes without copying them. Returns bytes removed. */
size_t ringbuf_discard(ringbuf *rb, size_t n);

#endif /* RINGBUF_H */
