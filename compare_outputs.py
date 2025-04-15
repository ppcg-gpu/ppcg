#!/usr/bin/env python3
import sys
import os
import re

def extract_numeric_values(filename):
    """Extract numeric values from a file."""
    values = []
    with open(filename, 'r') as f:
        for line in f:
            # Find all numeric tokens in the line
            for token in line.split():
                if re.match(r'^[-+]?[0-9]*\.?[0-9]+([eE][-+]?[0-9]+)?$', token):
                    values.append(float(token))
    return values

def compare_with_tolerance(ref_file, test_file, is_chemv):
    """Compare output files with tolerance for floating point values."""
    try:
        if is_chemv:
            # For chemv, use tolerance-based comparison
            ref_values = extract_numeric_values(ref_file)
            test_values = extract_numeric_values(test_file)

            if len(ref_values) != len(test_values):
                print(f"Warning: Different number of values: ref={len(ref_values)}, test={len(test_values)}")

            max_diff = 0.0
            max_rel = 0.0
            line_num = 1
            has_large_diff = False

            # Compare the minimum number of values between the two files
            for i in range(min(len(ref_values), len(test_values))):
                ref_val = ref_values[i]
                test_val = test_values[i]

                # Calculate absolute difference
                abs_diff = abs(ref_val - test_val)

                # Calculate relative difference
                if ref_val != 0:
                    rel_diff = abs_diff / abs(ref_val)
                else:
                    rel_diff = abs_diff

                # Update maximum differences
                max_diff = max(max_diff, abs_diff)
                max_rel = max(max_rel, rel_diff)

                # Check for large differences
                if abs_diff > 0.05 or rel_diff > 0.0001:
                    has_large_diff = True
                    print(f"Large difference at value {i+1}: ref={ref_val} test={test_val} diff={abs_diff} rel={rel_diff}")

            print(f"Maximum absolute difference: {max_diff}")
            print(f"Maximum relative difference: {max_rel}")

            if has_large_diff:
                print("Test failed: numerical differences exceed tolerance")
                return 1
            return 0
        else:
            # For other examples, use exact comparison
            with open(ref_file, 'r') as f1, open(test_file, 'r') as f2:
                if f1.read() != f2.read():
                    print(f"Files are different: {ref_file} {test_file}")
                    return 1
            return 0
    except Exception as e:
        print(f"Error comparing files: {e}")
        return 1

if __name__ == "__main__":
    if len(sys.argv) < 3:
        print("Usage: python compare_outputs.py <reference_file> <test_file> [is_chemv]")
        sys.exit(1)

    ref_file = sys.argv[1]
    test_file = sys.argv[2]
    is_chemv = len(sys.argv) > 3 and str(sys.argv[3]).lower() in ("true", "1", "yes", "y", "t")

    # Check if files exist
    if not os.path.exists(ref_file):
        print(f"Reference file {ref_file} does not exist")
        sys.exit(1)
    if not os.path.exists(test_file):
        print(f"Test file {test_file} does not exist")
        sys.exit(1)

    sys.exit(compare_with_tolerance(ref_file, test_file, is_chemv))
