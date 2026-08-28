# ringbuf

A fixed-capacity circular byte buffer in C99. No allocation, no dependencies,
two files.

```c
#include "ringbuf.h"

unsigned char storage[256];
ringbuf rb;
ringbuf_init(&rb, storage, sizeof storage);

ringbuf_write(&rb, frame, frame_len);   /* stores what fits */
ringbuf_read(&rb, out, sizeof out);     /* oldest first */
```

The caller owns the storage, which is what makes this usable from an interrupt
handler or a freestanding target.

## API

| Function | Behaviour |
| --- | --- |
| `ringbuf_init` | points the ring at caller-owned storage; 0 on success, -1 on bad arguments |
| `ringbuf_write` | copies up to `n` bytes, stopping when full; returns bytes stored |
| `ringbuf_overwrite` | copies `n` bytes, evicting the oldest; returns bytes dropped |
| `ringbuf_read` | copies up to `n` bytes out and removes them |
| `ringbuf_peek` | copies up to `n` bytes out without removing them |
| `ringbuf_discard` | removes up to `n` bytes without copying |
| `ringbuf_clear` | drops everything |
| `ringbuf_size` / `_capacity` / `_free_space` / `_is_empty` / `_is_full` | queries |

Every function tolerates a `NULL` ring and returns the do-nothing answer.

## Two policies for a full buffer

`write` keeps the **oldest** data: it stores what fits and returns a short
count, which the caller must check.

`overwrite` keeps the **newest**: it evicts as much old data as it needs and
returns how many bytes were dropped. That is what a log tail or a scope trace
wants.

The count `overwrite` returns includes any part of the input that could never
have fitted. Writing 300 bytes into a 256-byte ring keeps the last 256 and
reports 44 dropped — rather than copying all 300 in and evicting as it goes,
which would give the same result more slowly.

## Design notes

**`head` plus `count`, not `head` plus `tail`.** With two indices,
`head == tail` means both empty and full, and implementations that take that
route either sacrifice a slot or carry a separate flag. Storing the count makes
the state unambiguous and **the whole capacity usable** — a 256-byte ring holds
256 bytes. There is a test for exactly that.

**Capacity need not be a power of two.** Implementations that require one do so
to replace a modulo with a mask. This one keeps the modulo, because a caller
sizing a buffer to a protocol's maximum frame should not have to round 300 up to
512. Capacity 5 and 7 are both exercised in the tests.

**Not thread-safe.** A single-producer/single-consumer arrangement is safe only
with the appropriate atomics and memory barriers, deliberately out of scope.

## Tests

```
make test
```

83 checks and no framework. CI builds under gcc and clang at `-std=c99` and
`-std=c11` with `-Wall -Wextra -Werror -pedantic`, then runs the suite again
under AddressSanitizer + UBSan and once more under valgrind.

## License

MIT
