# EventHorizon Engine - Python Bindings

Python bindings for the EventHorizon high-performance edge AI inference engine.

## Features

- ✅ Zero-copy numpy array integration
- ✅ Context manager support for automatic cleanup
- ✅ Simple, Pythonic API
- ✅ Type hints for better IDE support
- ✅ Cross-platform (Windows, Linux, macOS)

## Installation

### Prerequisites

1. Build the EventHorizon shared library:
```bash
# From the eventhorizon root directory
make lib
```

2. Install Python package:
```bash
cd bindings/python
pip install -e .
```

## Quick Start

```python
import numpy as np
from eventhorizon import create_engine

# Create engine
engine = create_engine(input_dim=128, output_dim=128)

# Run inference
input_vec = np.random.randn(128).astype(np.float32)
output = engine.inference(input_vec)

print(f"Output: {output.shape}")

# Cleanup
engine.cleanup()
```

## Context Manager

Use the context manager for automatic cleanup:

```python
from eventhorizon import EventHorizon
import numpy as np

with EventHorizon() as engine:
    engine.create(input_dim=128, output_dim=128)
    
    input_vec = np.random.randn(128).astype(np.float32)
    output = engine.inference(input_vec)
    
# Engine automatically cleaned up
```

## Batch Processing

```python
import numpy as np
from eventhorizon import create_engine

engine = create_engine(input_dim=128, output_dim=128)

# Process batch
batch_size = 100
inputs = np.random.randn(batch_size, 128).astype(np.float32)

outputs = []
for input_vec in inputs:
    output = engine.inference(input_vec)
    outputs.append(output)

outputs = np.array(outputs)
print(f"Batch output shape: {outputs.shape}")

engine.cleanup()
```

## Benchmark

```python
import numpy as np
import time
from eventhorizon import create_engine

engine = create_engine(input_dim=128, output_dim=128)
input_vec = np.random.randn(128).astype(np.float32)

# Warmup
for _ in range(10):
    engine.inference(input_vec)

# Benchmark
num_inferences = 1000
start = time.time()

for _ in range(num_inferences):
    output = engine.inference(input_vec)

elapsed = time.time() - start
throughput = num_inferences / elapsed

print(f"Throughput: {throughput:.0f} inferences/sec")
print(f"Latency: {elapsed/num_inferences*1000:.3f} ms")

engine.cleanup()
```

## API Reference

### `EventHorizon`

Main engine class.

#### Methods

**`__init__(library_path=None)`**
- Initialize engine wrapper
- Auto-detects library location if not provided

**`create(input_dim=128, output_dim=128, arena_size=64*1024*1024)`**
- Create and initialize the engine
- `input_dim`: Input vector dimension
- `output_dim`: Output vector dimension
- `arena_size`: Memory arena size in bytes

**`inference(input_vector) -> np.ndarray`**
- Run inference on input vector
- `input_vector`: Numpy array of shape `(input_dim,)` dtype=float32
- Returns: Numpy array of shape `(output_dim,)` dtype=float32

**`cleanup()`**
- Release all resources
- Called automatically by context manager

### `create_engine(input_dim, output_dim) -> EventHorizon`

Convenience function to create and initialize engine in one call.

## Examples

See `example.py` for comprehensive examples:

```bash
python example.py
```

## Requirements

- Python 3.7+
- NumPy 1.19+
- EventHorizon shared library (libeventhorizon.so/dylib/dll)

## Performance

Expected performance (on standard hardware):
- **Throughput**: 50K-100K inferences/sec (Python overhead included)
- **Latency**: 10-20 μs per inference
- **Memory**: 64 MB (configurable)

## Troubleshooting

### Library Not Found

If you get `FileNotFoundError: EventHorizon library not found`:

**Linux/macOS:**
```bash
export LD_LIBRARY_PATH=/path/to/eventhorizon:$LD_LIBRARY_PATH
```

**Windows:**
Add the library directory to PATH or copy the DLL to the script directory.

### Dimension Mismatch

Ensure input vector dimension matches the `input_dim` specified during creation:

```python
engine.create(input_dim=128, output_dim=128)

# This will raise ValueError:
input_vec = np.random.randn(64)  # Wrong size!
output = engine.inference(input_vec)

# Correct:
input_vec = np.random.randn(128)  # Correct size
output = engine.inference(input_vec)
```

## Development

### Running Tests

```bash
pip install -e ".[dev]"
pytest tests/
```

### Code Formatting

```bash
black eventhorizon.py
flake8 eventhorizon.py
```

## License

Apache-2.0 License. See LICENSE file in the root directory.

## Contributing

See CONTRIBUTING.md in the root directory for guidelines.

## Links

- [Main Repository](https://github.com/yourusername/eventhorizon)
- [Documentation](https://github.com/yourusername/eventhorizon/blob/main/README.md)
- [Issues](https://github.com/yourusername/eventhorizon/issues)
