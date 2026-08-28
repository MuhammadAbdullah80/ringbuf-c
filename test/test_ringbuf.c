/* Test suite for ringbuf. Builds and runs standalone: no framework. */
#include "../ringbuf.h"

#include <stdio.h>
#include <string.h>

static int checks = 0;
static int failures = 0;

static void check(const char *name, int ok)
{
    checks++;
    if (!ok) {
        failures++;
        printf("FAIL %s\n", name);
    }
}

static void check_bytes(const char *name, const void *got, const char *want, size_t n)
{
    checks++;
    if (memcmp(got, want, n) != 0) {
        size_t i;
        failures++;
        printf("FAIL %s\n      want: ", name);
        for (i = 0; i < n; i++) {
            putchar(want[i]);
        }
        printf("\n      got:  ");
        for (i = 0; i < n; i++) {
            putchar(((const char *)got)[i]);
        }
        putchar('\n');
    }
}

static void test_init(void)
{
    unsigned char storage[8];
    ringbuf rb;

    check("init rejects a NULL ring", ringbuf_init(NULL, storage, sizeof storage) == -1);
    check("init rejects NULL storage", ringbuf_init(&rb, NULL, sizeof storage) == -1);
    check("init rejects zero capacity", ringbuf_init(&rb, storage, 0) == -1);

    check("init succeeds", ringbuf_init(&rb, storage, sizeof storage) == 0);
    check("a fresh ring is empty", ringbuf_is_empty(&rb));
    check("a fresh ring is not full", !ringbuf_is_full(&rb));
    check("a fresh ring holds nothing", ringbuf_size(&rb) == 0);
    check("capacity is what was asked for", ringbuf_capacity(&rb) == 8);
    check("all of it is free", ringbuf_free_space(&rb) == 8);
}

static void test_round_trip(void)
{
    unsigned char storage[8];
    unsigned char out[16];
    ringbuf rb;

    ringbuf_init(&rb, storage, sizeof storage);

    check("write reports what it stored", ringbuf_write(&rb, "ABCD", 4) == 4);
    check("size reflects the write", ringbuf_size(&rb) == 4);
    check("free space shrank", ringbuf_free_space(&rb) == 4);
    check("a ring holding bytes is not empty", !ringbuf_is_empty(&rb));

    check("read reports what it copied", ringbuf_read(&rb, out, 4) == 4);
    check_bytes("bytes come back in order", out, "ABCD", 4);
    check("the ring is empty again", ringbuf_is_empty(&rb));

    check("reading an empty ring copies nothing", ringbuf_read(&rb, out, 4) == 0);
}

static void test_capacity_is_fully_usable(void)
{
    unsigned char storage[8];
    unsigned char out[16];
    ringbuf rb;

    ringbuf_init(&rb, storage, sizeof storage);

    /* Every byte of storage is usable - no slot is sacrificed to distinguish
     * the full state from the empty one. */
    check("the whole capacity can be written", ringbuf_write(&rb, "ABCDEFGH", 8) == 8);
    check("the ring reports full", ringbuf_is_full(&rb));
    check("nothing is free", ringbuf_free_space(&rb) == 0);
    check("a write into a full ring stores nothing", ringbuf_write(&rb, "X", 1) == 0);

    check("all of it reads back", ringbuf_read(&rb, out, 8) == 8);
    check_bytes("and it is unchanged", out, "ABCDEFGH", 8);
}

static void test_partial_write(void)
{
    unsigned char storage[4];
    unsigned char out[8];
    ringbuf rb;

    ringbuf_init(&rb, storage, sizeof storage);

    /* write truncates to what fits, keeping the *first* bytes. The caller has
     * to check the return value; overwrite() is the other policy. */
    check("write reports the short count", ringbuf_write(&rb, "ABCDEF", 6) == 4);
    check("read returns what was stored", ringbuf_read(&rb, out, 8) == 4);
    check_bytes("write keeps the leading bytes", out, "ABCD", 4);
}

static void test_wraparound(void)
{
    unsigned char storage[8];
    unsigned char out[16];
    ringbuf rb;

    ringbuf_init(&rb, storage, sizeof storage);

    ringbuf_write(&rb, "ABCDEF", 6);
    check("consume part of it", ringbuf_read(&rb, out, 4) == 4);
    check_bytes("the oldest bytes came out", out, "ABCD", 4);

    /* head is at 6, tail at 4: this write wraps, and so does the read. */
    check("a wrapping write fills the ring", ringbuf_write(&rb, "GHIJKL", 6) == 6);
    check("the ring is full", ringbuf_is_full(&rb));
    check("a wrapping read returns everything", ringbuf_read(&rb, out, 8) == 8);
    check_bytes("in the right order across the wrap", out, "EFGHIJKL", 8);
}

static void test_non_power_of_two_capacity(void)
{
    unsigned char storage[5];
    unsigned char out[8];
    ringbuf rb;

    /* Capacity 5 exercises the modulo; a mask-based implementation could not
     * do this at all. */
    ringbuf_init(&rb, storage, sizeof storage);

    check("fills a 5-byte ring", ringbuf_write(&rb, "ABCDE", 5) == 5);
    check("drains three", ringbuf_read(&rb, out, 3) == 3);
    check_bytes("oldest first", out, "ABC", 3);
    check("refills across the wrap", ringbuf_write(&rb, "FGH", 3) == 3);
    check("reads all five", ringbuf_read(&rb, out, 5) == 5);
    check_bytes("order survives an odd capacity", out, "DEFGH", 5);
}

static void test_peek_and_discard(void)
{
    unsigned char storage[8];
    unsigned char out[16];
    ringbuf rb;

    ringbuf_init(&rb, storage, sizeof storage);
    ringbuf_write(&rb, "ABCDEF", 6);

    check("peek copies", ringbuf_peek(&rb, out, 3) == 3);
    check_bytes("peek sees the oldest bytes", out, "ABC", 3);
    check("peek does not consume", ringbuf_size(&rb) == 6);
    check("peek again returns the same bytes", ringbuf_peek(&rb, out, 3) == 3);
    check_bytes("identical on the second peek", out, "ABC", 3);

    check("peek is capped by what is stored", ringbuf_peek(&rb, out, 100) == 6);

    check("discard removes without copying", ringbuf_discard(&rb, 2) == 2);
    check("size dropped", ringbuf_size(&rb) == 4);
    check("discard is capped by what is stored", ringbuf_discard(&rb, 100) == 4);
    check("the ring is empty after over-discarding", ringbuf_is_empty(&rb));
    check("discarding an empty ring removes nothing", ringbuf_discard(&rb, 1) == 0);
}

static void test_clear(void)
{
    unsigned char storage[8];
    ringbuf rb;

    ringbuf_init(&rb, storage, sizeof storage);
    ringbuf_write(&rb, "ABCD", 4);

    ringbuf_clear(&rb);
    check("clear empties the ring", ringbuf_is_empty(&rb));
    check("capacity survives a clear", ringbuf_capacity(&rb) == 8);
    check("the ring is usable after a clear", ringbuf_write(&rb, "WXYZ", 4) == 4);
}

static void test_overwrite(void)
{
    unsigned char storage[8];
    unsigned char out[16];
    ringbuf rb;

    ringbuf_init(&rb, storage, sizeof storage);

    check("overwrite into an empty ring drops nothing",
          ringbuf_overwrite(&rb, "ABCDEFGH", 8) == 0);
    check("the ring is full", ringbuf_is_full(&rb));

    check("overwrite evicts exactly what it needs",
          ringbuf_overwrite(&rb, "IJK", 3) == 3);
    check("the ring is still full", ringbuf_is_full(&rb));
    check("read it all back", ringbuf_read(&rb, out, 8) == 8);
    check_bytes("the newest bytes survived", out, "DEFGHIJK", 8);

    /* The subtle case: more bytes offered than the ring could ever hold. Only
     * the last `capacity` can survive, and the count of dropped bytes has to
     * include the part of src that was never stored. */
    ringbuf_init(&rb, storage, sizeof storage);
    check("overwrite larger than capacity reports every dropped byte",
          ringbuf_overwrite(&rb, "0123456789AB", 12) == 4);
    check("and fills the ring", ringbuf_size(&rb) == 8);
    check("read it back", ringbuf_read(&rb, out, 8) == 8);
    check_bytes("keeping the tail of the input", out, "456789AB", 8);

    /* Same, but starting from a partially full ring, so both eviction paths
     * run in one call. */
    ringbuf_init(&rb, storage, sizeof storage);
    ringbuf_write(&rb, "XY", 2);
    check("both eviction paths report together",
          ringbuf_overwrite(&rb, "0123456789", 10) == 4);
    check("read it back", ringbuf_read(&rb, out, 8) == 8);
    check_bytes("only the newest eight remain", out, "23456789", 8);

    check("overwrite of zero bytes is a no-op", ringbuf_overwrite(&rb, "A", 0) == 0);
}

static void test_null_safety(void)
{
    unsigned char storage[8];
    unsigned char out[8];
    ringbuf rb;

    ringbuf_init(&rb, storage, sizeof storage);

    check("capacity of NULL is 0", ringbuf_capacity(NULL) == 0);
    check("size of NULL is 0", ringbuf_size(NULL) == 0);
    check("free space of NULL is 0", ringbuf_free_space(NULL) == 0);
    check("NULL counts as empty", ringbuf_is_empty(NULL));
    check("NULL is not full", !ringbuf_is_full(NULL));

    check("write to NULL stores nothing", ringbuf_write(NULL, "A", 1) == 0);
    check("write of NULL src stores nothing", ringbuf_write(&rb, NULL, 1) == 0);
    check("read from NULL copies nothing", ringbuf_read(NULL, out, 1) == 0);
    check("read into NULL copies nothing", ringbuf_read(&rb, NULL, 1) == 0);
    check("peek on NULL copies nothing", ringbuf_peek(NULL, out, 1) == 0);
    check("peek into NULL copies nothing", ringbuf_peek(&rb, NULL, 1) == 0);
    check("discard on NULL removes nothing", ringbuf_discard(NULL, 1) == 0);
    check("overwrite on NULL drops nothing", ringbuf_overwrite(NULL, "A", 1) == 0);
    check("overwrite of NULL src drops nothing", ringbuf_overwrite(&rb, NULL, 1) == 0);

    ringbuf_clear(NULL);
    check("clear on NULL does not crash", 1);
}

static void test_stress_alternating(void)
{
    unsigned char storage[7];
    unsigned char out[4];
    ringbuf rb;
    unsigned int i;
    int ok = 1;

    /* Push and pop repeatedly through a capacity that is not a divisor of the
     * chunk size, so head and tail land on every offset. */
    ringbuf_init(&rb, storage, sizeof storage);

    for (i = 0; i < 1000; i++) {
        unsigned char in[3];
        in[0] = (unsigned char)(i & 0xFF);
        in[1] = (unsigned char)((i >> 8) & 0xFF);
        in[2] = (unsigned char)((i * 7) & 0xFF);

        if (ringbuf_write(&rb, in, 3) != 3) {
            ok = 0;
            break;
        }
        if (ringbuf_read(&rb, out, 3) != 3) {
            ok = 0;
            break;
        }
        if (memcmp(in, out, 3) != 0) {
            ok = 0;
            break;
        }
    }

    check("1000 write/read cycles round-trip intact", ok);
    check("the ring ends empty", ringbuf_is_empty(&rb));
}

int main(void)
{
    test_init();
    test_round_trip();
    test_capacity_is_fully_usable();
    test_partial_write();
    test_wraparound();
    test_non_power_of_two_capacity();
    test_peek_and_discard();
    test_clear();
    test_overwrite();
    test_null_safety();
    test_stress_alternating();

    if (failures == 0) {
        printf("all %d checks passed\n", checks);
        return 0;
    }
    printf("%d of %d checks FAILED\n", failures, checks);
    return 1;
}
