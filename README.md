# lz4kdr

userspace port of lz4kdr,a speed tuned derivative of huawei's LZ4KD that i use as the zram compressor in my kernel

it compresses one independent block of up to 4 KiB per call,4 KiB pages are what it is built for. the on-wire format is LZ4KD without its 8 byte pattern pages,so data from the original LZ4KD encoder decodes here unless it is a pattern page,and the other way around

## when to use it instead of lz4

when your data already comes in small independent blocks and you compress more often than you decompress: memory pages,swap,cache entries,values in a key-value store,network packets

on 4 KiB blocks it compresses faster than lz4 at the same ratio. lz4 is built around a 64 KiB window and a 16 KiB hash table,on a single page most of that is wasted: `LZ4_compress_fast_extState()` clears the whole 16 KiB table on every call and the window can never reach past the page. lz4kdr uses a 2 KiB hash table that stays in L1,hashes two positions from one 8 byte load and adapts its search step to how well the last pages compressed

measured on an i5-11400F,one core,turbo off,kernel flags (`-O3`,no simd),about 51 thousand 4 KiB pages of a real python heap,machine code,text and already compressed cache files,ns per page:

```
                     compress   decompress   ratio
lz4 1.10 (accel 1)       5460         1610    2.67
lz4kdr                   4950         1910    2.68

per data type, compress only:
python heap              4892  ->  4847
machine code             8180  ->  5989
text                    12225  ->  9210
compressed files         1993  ->  1320
```

## when not to use it

files and streams: matches are only found inside one 4 KiB block,lz4 with its 64 KiB window compresses larger data noticeably better

decompress heavy workloads: lz4kdr decodes about 18% slower than lz4

inputs over 4096 bytes are rejected,split them yourself and store the length of every block

## usage

add `lz4kdr_encode.c` and `lz4kdr_decode.c` to your build,include `lz4kdr.h`

```c
#include "lz4kdr.h"

/* one per thread, the encoder clears it on every call */
static _Thread_local unsigned char state[2048];

unsigned char page[4096], out[4096 + 64], back[4096];

int n = lz4kdr_encode(state, page, out, sizeof(page), sizeof(out), 0);
if (n > 0) {
	/* compressed, keep n bytes of out */
	int m = lz4kdr_decode(out, back, n, sizeof(back));
	/* m == 4096 on success */
} else if (n == 0) {
	/* incompressible, store the page as is */
} else {
	/* bad arguments or input over 4096 bytes */
}
```

`state` must be at least `lz4kdr_encode_state_bytes_min()` bytes (2 KiB),use one per thread

`out` should be at least `in_max + 64` bytes,with less the encoder may give up and return 0

the decoder needs the original size as `out_max`,store it next to the compressed block

`out_limit` stops the encoder early once the result can't be smaller than that,0 means `min(in_max, out_max)`

inputs of 16 bytes or less always return 0,they never compress and the search loop reads 8 bytes ahead

the decoder checks every length and offset against its buffers,corrupt input returns a negative status instead of reading or writing out of bounds

## test

```
clang -std=c11 -O1 -g -fsanitize=address,undefined test.c lz4kdr_encode.c lz4kdr_decode.c -o test && ./test
```

round trips five kinds of data in 16 sizes from 1 to 4096 bytes and feeds the decoder corrupted blocks


