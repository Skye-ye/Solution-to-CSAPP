#!/usr/bin/env python3
#
# driver.py - The driver tests the correctness of the student's cache
#     simulator and the correctness and performance of their transpose
#     function. It uses ./test-csim to check the correctness of the
#     simulator and it runs ./test-trans on three different sized
#     matrices (32x32, 64x64, and 61x67) to test the correctness and
#     performance of the transpose function.
#
import subprocess
import re
import os
import sys
import optparse

#
# computeMissScore - compute the score depending on the number of
# cache misses
#
def computeMissScore(miss, lower, upper, full_score):
    if miss <= lower:
        return full_score
    if miss >= upper: 
        return 0

    score = (miss - lower) * 1.0 
    range = (upper - lower) * 1.0
    return round((1 - score / range) * full_score, 1)

def run_cmd(cmd):
    """Run a command and return its output, with error handling"""
    try:
        p = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        stdout, stderr = p.communicate()
        return stdout.decode('utf-8'), stderr.decode('utf-8'), p.returncode
    except Exception as e:
        print(f"Error running command '{cmd}': {str(e)}")
        return "", str(e), -1

#
# main - Main function
#
def main():
    # Configure maxscores here
    maxscore= {}
    maxscore['csim'] = 27
    maxscore['transc'] = 1
    maxscore['trans32'] = 8
    maxscore['trans64'] = 8
    maxscore['trans61'] = 10

    # Parse the command line arguments 
    p = optparse.OptionParser()
    p.add_option("-A", action="store_true", dest="autograde", 
                 help="emit autoresult string for Autolab")
    opts, args = p.parse_args()
    autograde = opts.autograde

    # Check the correctness of the cache simulator
    print("Part A: Testing cache simulator")
    print("Running ./test-csim")
    stdout_data, stderr_data, returncode = run_cmd("./test-csim")
    
    if returncode != 0:
        print("Error: test-csim failed to run correctly")
        print("stderr output:", stderr_data)
        sys.exit(1)

    # Initialize resultsim with a default value
    resultsim = ['0']  # Default to zero score if no results found
    
    # Process test-csim output
    stdout_lines = stdout_data.split('\n')
    for line in stdout_lines:
        if re.match("TEST_CSIM_RESULTS", line):
            resultsim = re.findall(r'(\d+)', line)
            break  # Found the results, no need to continue
        print("%s" % (line))

    if not resultsim:
        print("Warning: No TEST_CSIM_RESULTS found in output")
        resultsim = ['0']  # Default to zero score if no results found

    # Check the correctness and performance of the transpose function
    print("Part B: Testing transpose function")
    
    # Function to safely get test results
    def get_test_results(M, N):
        cmd = f"./test-trans -M {M} -N {N} | grep TEST_TRANS_RESULTS"
        stdout, stderr, rc = run_cmd(cmd)
        if rc != 0:
            print(f"Warning: test-trans failed for {M}x{N} matrix")
            return ['0', str(2**31-1)]  # Return default values on error
        results = re.findall(r'(\d+)', stdout)
        return results if results else ['0', str(2**31-1)]

    # Run transpose tests
    print("Running ./test-trans -M 32 -N 32")
    result32 = get_test_results(32, 32)
    
    print("Running ./test-trans -M 64 -N 64")
    result64 = get_test_results(64, 64)
    
    print("Running ./test-trans -M 61 -N 67")
    result61 = get_test_results(61, 67)
    
    # Compute the scores for each step
    csim_cscore = int(resultsim[0])
    trans_cscore = int(result32[0]) * int(result64[0]) * int(result61[0])
    miss32 = int(result32[1])
    miss64 = int(result64[1])
    miss61 = int(result61[1])
    trans32_score = computeMissScore(miss32, 300, 600, maxscore['trans32']) * int(result32[0])
    trans64_score = computeMissScore(miss64, 1300, 2000, maxscore['trans64']) * int(result64[0])
    trans61_score = computeMissScore(miss61, 2000, 3000, maxscore['trans61']) * int(result61[0])
    total_score = csim_cscore + trans32_score + trans64_score + trans61_score

    # Summarize the results
    print("\nCache Lab summary:")
    print("%-22s%8s%10s%12s" % ("", "Points", "Max pts", "Misses"))
    print("%-22s%8.1f%10d" % ("Csim correctness", csim_cscore, 
                              maxscore['csim']))

    misses = str(miss32)
    if miss32 == 2**31-1:
        misses = "invalid"
    print("%-22s%8.1f%10d%12s" % ("Trans perf 32x32", trans32_score, 
                                  maxscore['trans32'], misses))

    misses = str(miss64)
    if miss64 == 2**31-1:
        misses = "invalid"
    print("%-22s%8.1f%10d%12s" % ("Trans perf 64x64", trans64_score, 
                                  maxscore['trans64'], misses))

    misses = str(miss61)
    if miss61 == 2**31-1:
        misses = "invalid"
    print("%-22s%8.1f%10d%12s" % ("Trans perf 61x67", trans61_score, 
                                  maxscore['trans61'], misses))

    print("%-22s%8.1f%10d" % ("Total points", total_score,
                             maxscore['csim'] + 
                             maxscore['trans32'] + 
                             maxscore['trans64'] +
                             maxscore['trans61']))
    
    # Emit autoresult string for Autolab if called with -A option
    if autograde:
        autoresult="%.1f:%d:%d:%d" % (total_score, miss32, miss64, miss61)
        print("\nAUTORESULT_STRING=%s" % autoresult)
    
# execute main only if called as a script
if __name__ == "__main__":
    main()