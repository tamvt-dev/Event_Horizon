#!/usr/bin/env python3
"""
EventHorizon Engine - Python Package Setup
"""

from setuptools import setup, find_packages
import os

# Read README
def read_readme():
    readme_path = os.path.join(os.path.dirname(__file__), "README.md")
    if os.path.exists(readme_path):
        with open(readme_path, encoding="utf-8") as f:
            return f.read()
    return "EventHorizon Engine Python Bindings"

setup(
    name="eventhorizon",
    version="1.0.0",
    author="EventHorizon Engine Contributors",
    description="High-performance edge AI inference engine with Python bindings",
    long_description=read_readme(),
    long_description_content_type="text/markdown",
    url="https://github.com/yourusername/eventhorizon",
    py_modules=["eventhorizon"],
    python_requires=">=3.7",
    install_requires=[
        "numpy>=1.19.0",
    ],
    extras_require={
        "dev": [
            "pytest>=6.0",
            "pytest-cov",
            "black",
            "flake8",
        ],
    },
    classifiers=[
        "Development Status :: 3 - Alpha",
        "Intended Audience :: Developers",
        "Intended Audience :: Science/Research",
        "License :: OSI Approved :: Apache Software License",
        "Programming Language :: Python :: 3",
        "Programming Language :: Python :: 3.7",
        "Programming Language :: Python :: 3.8",
        "Programming Language :: Python :: 3.9",
        "Programming Language :: Python :: 3.10",
        "Programming Language :: Python :: 3.11",
        "Programming Language :: C",
        "Topic :: Scientific/Engineering :: Artificial Intelligence",
        "Topic :: Software Development :: Libraries",
    ],
    keywords="ai, inference, edge-ai, machine-learning, neural-networks",
)
