This an implementation of the LZ77 compression algorithm.

**Comparison of the compression ratio for [enwik8](https://www.mattmahoney.net/dc/text.html):**

| Program | Level | Ratio |
|-|-|-|
| deflate | 5 | 0.35 |
| lz4 | 1 | 0.57 |
| **lz77** | **n/a** | **0.55** |
| lzma2 | 5 | 0.26 |
| zstd | 3 | 0.35 |
