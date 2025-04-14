# ADR-001: Columnar Storage Format

## Status

Accepted

## Context

Market data analytics queries typically scan large volumes of time-series data, filtering on specific columns (e.g., instrument_id, timestamp, price) and computing aggregations (count, sum, min, max). The storage format must balance:

1. **Query Performance**: Fast scans and aggregations
2. **Compression Ratio**: Efficient storage for large datasets
3. **Write Performance**: High-throughput ingestion
4. **Schema Flexibility**: Support for evolving schemas

## Decision

We will use a **columnar storage format** (VTS1) with the following characteristics:

### Segment-Based Layout
- Each segment contains up to 65,536 rows (2^16)
- Segments are the unit of compression and query execution
- Row count is a power of 2 for SIMD alignment

### Per-Column Encoding
- Each column selects its optimal encoding independently
- Supported encodings: BitPack, RLE, Dictionary, VarInt, Delta
- Encoding metadata stored in segment header

### Zone Maps
- Each segment maintains min/max statistics per column
- Zone maps for 1024-row chunks within segments
- Enables predicate pushdown and segment pruning

### Bloom Filters
- Per-column bloom filters for equality predicates
- 1% false positive rate (tunable)
- Enables early rejection of segments

### Checksums
- CRC32C checksum on all blocks
- Hardware acceleration (SSE4.2, ARMv8)
- Detects corruption

## Rationale

### Why Columnar?

| Aspect | Row-based | Columnar |
|--------|-----------|----------|
| Scan Performance | O(total_bytes) | O(accessed_columns) |
| Compression | Low | High (same-type data) |
| Aggregations | Slow | Fast (column scans) |
| Point Lookups | Fast | Slow (reassembly needed) |

Market data analytics are scan-heavy with aggregation operations, making columnar storage the clear choice.

### Why 65,536 Rows per Segment?

1. **Compression Efficiency**: Larger segments compress better
2. **Memory Locality**: Fits in L2 cache (~256 KB per column)
3. **SIMD Alignment**: Power of 2 enables vectorized processing
4. **Pruning Granularity**: Not too coarse (avoid scanning too much) or fine (too much overhead)

### Why Per-Column Encoding?

Different columns have different data characteristics:
- `instrument_id`: Low cardinality → Dictionary encoding
- `timestamp`: Sequential → Delta encoding
- `quantity`: Small range → Bit packing
- `price_ticks`: Variable range → VarInt

Per-column encoding maximizes compression ratio.

## Consequences

### Positive
- High compression ratio (5-20x typical)
- Fast analytical queries
- Efficient predicate pushdown
- SIMD-friendly column scans

### Negative
- Slower point lookups (require reassembly)
- Not optimal for OLTP workloads
- Schema evolution requires migration

### Mitigations
- For point lookups, maintain a separate row-based cache or index
- For schema evolution, use additive schema changes where possible

## Alternatives Considered

### Row-Based Storage
- Rejected due to poor scan performance for analytics

### Hybrid (PAX)
- Rejected due to complexity and limited benefits for our use case

### Existing Formats (Parquet, ORC)
- Rejected to maintain control over format and avoid heavy dependencies
- Future: Consider Parquet as an export format

## Implementation

- [storage/segment_writer.hpp](../../include/vectortick/storage/segment_writer.hpp)
- [storage/segment_reader.hpp](../../include/vectortick/storage/segment_reader.hpp)
- [storage/file_format.hpp](../../include/vectortick/storage/file_format.hpp)
