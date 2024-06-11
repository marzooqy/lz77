This an implementation of the LZ77 compression algorithm.

**Techniques used:** LZ Compression | Hash Tables | Parallelism | Variable Length Integers

**Optimizations:**

1- Encode very large matches by splitting them into multiple blocks

2- Search the left of the buffer to improve the match (Backtracking)

**Comparison of the compression ratio for [enwik8](https://www.mattmahoney.net/dc/text.html):**

| Program | Level | Ratio |
|:-:|:-:|:-:|
| lzma2 | 5 | 0.26 |
| deflate | 5 | 0.35 |
| zstd | 3 | 0.35 |
| **lz77** | **n/a** | **0.54** |
| lz4 | 1 | 0.57 |

The compression ratio is not as good as the other algorithms due to the fact that this implementation only uses LZ compression without any entropy encoding.

The algorithm is also optimized for speed rather than compression ratio.
