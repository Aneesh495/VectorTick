# Architecture Decision Records (ADRs)

This directory contains Architecture Decision Records for VectorTick.

## What is an ADR?

An ADR is a document that captures an important architectural decision along with its context and consequences. ADRs help future maintainers understand why certain decisions were made.

## Index

| Number | Title | Status | Date |
|--------|-------|--------|------|
| [001](001-columnar-storage.md) | Columnar Storage Format | Accepted | 2025-01-05 |
| [002](002-custom-jit.md) | Custom JIT Backend (No LLVM) | Accepted | 2025-01-08 |
| [003](003-ssa-ir.md) | SSA Intermediate Representation | Accepted | 2025-01-07 |
| [004](004-custom-protocol.md) | Custom VTP1 Wire Protocol | Accepted | 2025-01-03 |
| [005](005-no-exceptions.md) | No Exceptions, No RTTI | Accepted | 2025-01-01 |

## ADR Template

When adding a new ADR, use this template:

```markdown
# ADR-NNN: Title

## Status

[Proposed | Accepted | Deprecated | Superseded]

## Context

What is the issue that we're seeing that is motivating this decision or change?

## Decision

What is the change that we're proposing and/or doing?

## Rationale

Why is this the best solution?

## Consequences

What becomes easier or more difficult to do because of this change?

### Positive
- ...

### Negative
- ...

### Mitigations
- ...

## Alternatives Considered

What other options were considered and why were they rejected?

## Implementation

Links to relevant files.
```

## References

- [Documenting Architecture Decisions (Michael Nygard)](https://cognitect.com/blog/2011/11/15/documenting-architecture-decisions)
- [ADR GitHub Organization](https://adr.github.io/)
