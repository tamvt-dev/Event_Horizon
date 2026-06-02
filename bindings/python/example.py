#!/usr/bin/env python3
"""
EventHorizon Engine - Python Binding Example
Demonstrates basic usage of the Python API.
"""

import numpy as np
import time
from eventhorizon import EventHorizon, create_engine

def basic_example():
    """Basic inference example."""
    print("=" * 50)
    print("EventHorizon Engine - Python Binding Example")
    print("=" * 50)
    print()
    
    # Create engine
    print("Creating engine...")
    engine = create_engine(input_dim=128, output_dim=128)
    print("✓ Engine created")
    print()
    
    # Prepare input
    input_vector = np.random.randn(128).astype(np.float32)
    print(f"Input shape: {input_vector.shape}")
    print(f"Input range: [{input_vector.min():.3f}, {input_vector.max():.3f}]")
    print()
    
    # Run inference
    print("Running inference...")
    output = engine.inference(input_vector)
    print(f"Output shape: {output.shape}")
    print(f"Output range: [{output.min():.3f}, {output.max():.3f}]")
    print("✓ Inference complete")
    print()
    
    # Cleanup
    engine.cleanup()
    print("✓ Engine cleaned up")


def benchmark_example():
    """Benchmark throughput."""
    print()
    print("=" * 50)
    print("Benchmark Test")
    print("=" * 50)
    print()
    
    with EventHorizon() as engine:
        engine.create(input_dim=128, output_dim=128)
        
        # Warmup
        input_vec = np.random.randn(128).astype(np.float32)
        for _ in range(10):
            engine.inference(input_vec)
        
        # Benchmark
        num_inferences = 1000
        print(f"Running {num_inferences} inferences...")
        
        start = time.time()
        for _ in range(num_inferences):
            output = engine.inference(input_vec)
        elapsed = time.time() - start
        
        throughput = num_inferences / elapsed
        latency_ms = (elapsed / num_inferences) * 1000
        
        print(f"Elapsed time: {elapsed:.3f} sec")
        print(f"Throughput: {throughput:.0f} inferences/sec")
        print(f"Latency: {latency_ms:.3f} ms/inference")


def batch_example():
    """Batch processing example."""
    print()
    print("=" * 50)
    print("Batch Processing Example")
    print("=" * 50)
    print()
    
    with EventHorizon() as engine:
        engine.create(input_dim=128, output_dim=128)
        
        # Generate batch of inputs
        batch_size = 100
        inputs = np.random.randn(batch_size, 128).astype(np.float32)
        
        print(f"Processing batch of {batch_size} inputs...")
        outputs = []
        
        for i, input_vec in enumerate(inputs):
            output = engine.inference(input_vec)
            outputs.append(output)
            
            if (i + 1) % 20 == 0:
                print(f"  Processed {i + 1}/{batch_size}...")
        
        outputs = np.array(outputs)
        print(f"✓ Batch complete")
        print(f"Output batch shape: {outputs.shape}")


if __name__ == "__main__":
    try:
        basic_example()
        benchmark_example()
        batch_example()
        
        print()
        print("=" * 50)
        print("All examples completed successfully!")
        print("=" * 50)
        
    except FileNotFoundError as e:
        print(f"Error: {e}")
        print()
        print("To use Python bindings:")
        print("1. Build shared library: make lib")
        print("2. Or set LD_LIBRARY_PATH to the library location")
        
    except Exception as e:
        print(f"Error: {e}")
        import traceback
        traceback.print_exc()
