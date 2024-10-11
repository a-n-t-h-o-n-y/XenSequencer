#!/usr/bin/env python3

import json
import random
import argparse
from typing import Dict, List, Union

def generate_note() -> Dict[str, Union[float, int, str]]:
    return {
        "type": "Note",
        "delay": round(random.uniform(0, 0.5), 3),
        "gate": 1.0,
        "pitch": random.randint(-24, 24),
        "velocity": round(random.uniform(0.6, 1.0), 3)
    }

def generate_rest() -> Dict[str, str]:
    return {"type": "Rest"}

def generate_sequence(depth: int = 0, max_depth: int = 3) -> Dict[str, Union[str, List]]:
    if depth >= max_depth:
        return random.choice([generate_note(), generate_rest()])
    
    return {
        "type": "Sequence",
        "cells": [
            generate_sequence(depth + 1, max_depth) for _ in range(random.randint(1, 3))
        ]
    }

def generate_measure() -> Dict:
    complexity = random.choice(["simple", "medium", "complex"])
    max_depth = {"simple": 2, "medium": 3, "complex": 4}[complexity]
    
    return {
        "cell": generate_sequence(max_depth=max_depth),
        "time_signature": {
            "numerator": random.choice([3, 4, 5, 6]),
            "denominator": random.choice([4, 8])
        }
    }

def save_measure(measure: Dict, filename: str):
    with open(filename, 'w') as f:
        json.dump(measure, f, indent=2)

def main():
    parser = argparse.ArgumentParser(description="Generate random musical measures")
    parser.add_argument("num_files", type=int, help="Number of files to generate")
    args = parser.parse_args()

    for i in range(args.num_files):
        measure = generate_measure()
        filename = f"measure_{i+1}.xenseq"
        save_measure(measure, filename)
        print(f"Generated {filename}")

if __name__ == "__main__":
    main()