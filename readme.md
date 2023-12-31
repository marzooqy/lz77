This an implementation of the LZ77 compression algorithm.

Comparison of the compression ratio for [enwik8](https://www.mattmahoney.net/dc/text.html):

| Compressor | Level | Ratio |
|-|-|-|
| DEFLATE | 5 | 0.35 |
| LZ4 | 1 | 0.57 |
| LZ77 | N/A | 0.55 |
| LZMA2 | 5 | 0.26 |
| ZSTD | 3 | 0.35 |