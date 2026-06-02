# EventHorizon Engine - Docker Image
# Reproducible build environment for cross-platform development

FROM gcc:12 AS builder

# Install build dependencies
RUN apt-get update && apt-get install -y \
    make \
    cmake \
    git \
    && rm -rf /var/lib/apt/lists/*

# Set working directory
WORKDIR /eventhorizon

# Copy source code
COPY include/ ./include/
COPY src/ ./src/
COPY bench_all.c ./
COPY Makefile ./
COPY LICENSE ./
COPY NOTICE ./
COPY README.md ./

# Build all targets
RUN make build-all

# Verify build
RUN ls -lh eh_* && \
    file eh_engine_ultimate_bench

# Final stage - minimal runtime image
FROM debian:bookworm-slim

# Install runtime dependencies (libc6 already includes libm)
RUN apt-get update && apt-get install -y \
    libc6 \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app

# Copy built executables from builder
COPY --from=builder /eventhorizon/eh_engine_ultimate_bench ./
COPY --from=builder /eventhorizon/eh_neuro_test ./
COPY --from=builder /eventhorizon/eh_test ./
COPY --from=builder /eventhorizon/README.md ./
COPY --from=builder /eventhorizon/LICENSE ./

# Make executables runnable
RUN chmod +x eh_*

# Default command: run benchmark
CMD ["./eh_engine_ultimate_bench"]

# Labels
LABEL maintainer="EventHorizon Engine Contributors"
LABEL description="EventHorizon Engine - High-performance edge AI inference engine"
LABEL version="1.0"
LABEL license="Apache-2.0"
