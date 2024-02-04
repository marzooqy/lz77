This an implementation of the LZ77 compression algorithm.

Techniques used: LZ Compression | Hash Chains | Rolling Hash | Parallelism
Optimizations: Varied Encodings for the length and offset | Search the left of the buffer to improve the match

**Comparison of the compression ratio for [enwik8](https://www.mattmahoney.net/dc/text.html):**

| Program | Level | Ratio |
|-|:-:|:-:|
| lzma2 | 5 | 0.26 |
| deflate | 5 | 0.35 |
| zstd | 3 | 0.35 |
| **lz77** | **n/a** | **0.55** |
| lz4 | 1 | 0.57 |

The compression ratio is not as good as the other algorithms due to the fact that this implementation only uses LZ compression without any entropy encoding. Current version is also optimized for speed rather than compression ratio.