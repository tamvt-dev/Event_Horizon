"""
EventHorizon Engine - Python Bindings
Proof-of-Concept using ctypes

Usage:
    from eventhorizon import EventHorizon
    
    engine = EventHorizon()
    output = engine.inference(input_vector)
"""

import ctypes
import numpy as np
from typing import Optional, Tuple
import os
import platform

class EventHorizon:
    """Python wrapper for EventHorizon Engine C library."""
    
    def __init__(self, library_path: Optional[str] = None):
        """
        Initialize EventHorizon engine.
        
        Args:
            library_path: Path to shared library. If None, searches common locations.
        """
        if library_path is None:
            library_path = self._find_library()
        
        self.lib = ctypes.CDLL(library_path)
        self._setup_functions()
        
        # Engine state
        self.arena = None
        self.root_node = None
        self.scorer = None
        self.context = None
        
    def _find_library(self) -> str:
        """Find EventHorizon shared library."""
        system = platform.system()
        
        # Library name based on platform
        if system == "Windows":
            lib_name = "eventhorizon.dll"
        elif system == "Darwin":
            lib_name = "libeventhorizon.dylib"
        else:
            lib_name = "libeventhorizon.so"
        
        # Search paths
        search_paths = [
            os.path.join(os.path.dirname(__file__), "..", "..", lib_name),
            os.path.join("/usr/local/lib", lib_name),
            os.path.join("/usr/lib", lib_name),
        ]
        
        for path in search_paths:
            if os.path.exists(path):
                return path
        
        raise FileNotFoundError(
            f"EventHorizon library not found. Searched: {search_paths}"
        )
    
    def _setup_functions(self):
        """Setup C function signatures."""
        
        # Arena functions
        self.lib.eh_arena_create.argtypes = [ctypes.c_size_t]
        self.lib.eh_arena_create.restype = ctypes.c_void_p
        
        self.lib.eh_arena_destroy.argtypes = [ctypes.c_void_p]
        self.lib.eh_arena_destroy.restype = None
        
        # DAG functions
        self.lib.eh_dag_create_node.argtypes = [
            ctypes.c_int,  # node_id
            ctypes.c_int,  # rows
            ctypes.c_int   # cols
        ]
        self.lib.eh_dag_create_node.restype = ctypes.c_void_p
        
        self.lib.eh_dag_connect_nodes.argtypes = [
            ctypes.c_void_p,  # parent
            ctypes.c_void_p   # child
        ]
        self.lib.eh_dag_connect_nodes.restype = ctypes.c_bool
        
        # Scoring functions
        self.lib.eh_scoring_init.argtypes = [ctypes.c_int]
        self.lib.eh_scoring_init.restype = ctypes.c_void_p
        
        # Engine functions
        self.lib.eh_engine_setup.argtypes = [
            ctypes.c_void_p,  # root
            ctypes.c_void_p,  # scorer
            ctypes.c_float    # critical_limit
        ]
        self.lib.eh_engine_setup.restype = ctypes.c_void_p
        
        self.lib.eh_engine_inference.argtypes = [
            ctypes.c_void_p,              # context
            ctypes.POINTER(ctypes.c_float),  # input_vector
            ctypes.c_int,                 # input_dim
            ctypes.POINTER(ctypes.c_float),  # output_vector
            ctypes.c_int                  # output_dim
        ]
        self.lib.eh_engine_inference.restype = ctypes.c_bool
        
        self.lib.eh_engine_shutdown.argtypes = [ctypes.c_void_p]
        self.lib.eh_engine_shutdown.restype = None
    
    def create(self, input_dim: int = 128, output_dim: int = 128,
               arena_size: int = 64 * 1024 * 1024) -> None:
        """
        Create and initialize the engine.
        
        Args:
            input_dim: Input vector dimension
            output_dim: Output vector dimension
            arena_size: Memory arena size in bytes (default 64MB)
        """
        # Create arena
        self.arena = self.lib.eh_arena_create(arena_size)
        if not self.arena:
            raise RuntimeError("Failed to create memory arena")
        
        # Create simple DAG (root node only for POC)
        self.root_node = self.lib.eh_dag_create_node(0, output_dim, input_dim)
        if not self.root_node:
            raise RuntimeError("Failed to create DAG node")
        
        # Create scoring core
        self.scorer = self.lib.eh_scoring_init(input_dim)
        if not self.scorer:
            raise RuntimeError("Failed to create scoring core")
        
        # Setup engine context
        self.context = self.lib.eh_engine_setup(
            self.root_node,
            self.scorer,
            0.5  # critical_limit
        )
        if not self.context:
            raise RuntimeError("Failed to setup engine context")
        
        self.input_dim = input_dim
        self.output_dim = output_dim
    
    def inference(self, input_vector: np.ndarray) -> np.ndarray:
        """
        Run inference on input vector.
        
        Args:
            input_vector: Input numpy array of shape (input_dim,)
            
        Returns:
            Output numpy array of shape (output_dim,)
        """
        if self.context is None:
            raise RuntimeError("Engine not initialized. Call create() first.")
        
        # Validate input
        if input_vector.shape[0] != self.input_dim:
            raise ValueError(
                f"Input dimension mismatch: expected {self.input_dim}, "
                f"got {input_vector.shape[0]}"
            )
        
        # Prepare input/output buffers
        input_arr = input_vector.astype(np.float32)
        output_arr = np.zeros(self.output_dim, dtype=np.float32)
        
        # Convert to ctypes pointers
        input_ptr = input_arr.ctypes.data_as(ctypes.POINTER(ctypes.c_float))
        output_ptr = output_arr.ctypes.data_as(ctypes.POINTER(ctypes.c_float))
        
        # Run inference
        success = self.lib.eh_engine_inference(
            self.context,
            input_ptr,
            self.input_dim,
            output_ptr,
            self.output_dim
        )
        
        if not success:
            raise RuntimeError("Inference failed")
        
        return output_arr
    
    def __enter__(self):
        """Context manager entry."""
        return self
    
    def __exit__(self, exc_type, exc_val, exc_tb):
        """Context manager exit - cleanup resources."""
        self.cleanup()
    
    def cleanup(self):
        """Release all resources."""
        if self.context:
            self.lib.eh_engine_shutdown(self.context)
            self.context = None
        
        if self.arena:
            self.lib.eh_arena_destroy(self.arena)
            self.arena = None
        
        self.root_node = None
        self.scorer = None
    
    def __del__(self):
        """Destructor - ensure cleanup."""
        self.cleanup()


# Convenience function
def create_engine(input_dim: int = 128, output_dim: int = 128) -> EventHorizon:
    """
    Create and initialize an EventHorizon engine.
    
    Args:
        input_dim: Input vector dimension
        output_dim: Output vector dimension
        
    Returns:
        Initialized EventHorizon engine
    """
    engine = EventHorizon()
    engine.create(input_dim, output_dim)
    return engine
