#!/bin/bash

# Exit immediately if any command fails
set -e

# ---------------------------------------------------------
# Input Parsing & Validation
# ---------------------------------------------------------
if [ "$#" -ne 2 ]; then
    echo "Usage: $0 <PATH_TO_TRACE_DIR> <PATH_TO_BINARIES_DIR>"
    echo "Example: $0 /home/traces/SPEC17 /home/ChampSim/bin"
    exit 1
fi

TRACE_DIR="$1"
BINARIES_DIR="$2"

# ---------------------------------------------------------
# Absolute Path Resolution
# ---------------------------------------------------------
ROOT_DIR="$(pwd)"
ABS_TRACE_DIR="$(realpath "$TRACE_DIR")"

if [ ! -d "$TRACE_DIR" ]; then
    echo "ERROR: Trace directory '$TRACE_DIR' does not exist."
    exit 1
fi

if [ ! -d "$BINARIES_DIR" ]; then
    echo "ERROR: Binaries directory '$BINARIES_DIR' does not exist."
    exit 1
fi

# ---------------------------------------------------------
# Clean up old job files
# ---------------------------------------------------------
rm -f phase_1_jobs.txt phase_2_jobs.txt
echo "Cleaned up old job files."

# ---------------------------------------------------------
# Interactive Prompt for CVP-1 Traces
# ---------------------------------------------------------
IS_CVP=false
CSV_PATH=""

# Ask the user (read -p prints the prompt and waits for input)
read -p "Are you running CVP-1 traces with varying lengths? (y/n): " IS_CVP_PROMPT

# If the user types y or Y
if [[ "$IS_CVP_PROMPT" =~ ^[Yy]$ ]]; then
    IS_CVP=true
    read -p "Please enter the absolute path to the CSV file with length information: " CSV_PATH
    
    if [ ! -f "$CSV_PATH" ]; then
        echo "ERROR: CSV file '$CSV_PATH' does not exist."
        exit 1
    fi
    echo "CVP-1 Mode ENABLED. Reading lengths from: $CSV_PATH"
else
    echo "Standard Mode ENABLED. Using default 250,000,000 simulation instructions."
fi

echo "---------------------------------------------------------"
echo "Building directory structure in: $(pwd)"
echo "Using trace directory: $TRACE_DIR"
echo "---------------------------------------------------------"

# ---------------------------------------------------------
# Directory Creation & Binary Copying
# ---------------------------------------------------------

for BINARY_PATH in "$BINARIES_DIR"/*; do
    if [ -f "$BINARY_PATH" ]; then
        
        CONFIG_NAME=$(basename "$BINARY_PATH")
        mkdir -p "$CONFIG_NAME/bin"
        mkdir -p "$CONFIG_NAME/results"
        cp "$BINARY_PATH" "$CONFIG_NAME/bin/"
        
        PHASE_1_RUN_FILE="phase_1_jobs.txt"
        PHASE_2_RUN_FILE="phase_2_jobs.txt"
        
        for TRACE_PATH in "$ABS_TRACE_DIR"/*; do
            if [ -f "$TRACE_PATH" ]; then
                
                TRACE_FILENAME=$(basename "$TRACE_PATH")
                mkdir -p "$CONFIG_NAME/results/$TRACE_FILENAME"
                
                # ---------------------------------------------------------
                # Length Calculation Logic
                # ---------------------------------------------------------
                WARMUP_INST=50000000
                SIM_INST=250000000 # Defaults

                # Define absolute paths for the binary and the output file
                ABS_BIN="$ROOT_DIR/$CONFIG_NAME/bin/$CONFIG_NAME"
                OUT="log.txt"
                TARGET_RESULT_DIR="$ROOT_DIR/$CONFIG_NAME/results/$TRACE_FILENAME/"
                
                if [ "$IS_CVP" = true ]; then
                    # 1. grep looks for the trace name followed by a space
                    # 2. awk uses default whitespace separation and prints the 3rd column (number1)
                    # 3. tr removes any hidden carriage returns
                    EXTRACTED_LENGTH=$(grep "^$TRACE_FILENAME " "$CSV_PATH" | awk '{print $2}' | tr -d '\r')
                    
                    if [ -n "$EXTRACTED_LENGTH" ]; then
                        # Bash integer math: calculate 20% and 80% of number1
                        WARMUP_INST=$(( EXTRACTED_LENGTH * 20 / 100 ))
                        SIM_INST=$(( EXTRACTED_LENGTH * 80 / 100 ))
                    else
                        echo "[WARNING] Length not found in CSV for $TRACE_FILENAME. Defaulting to Warmup: $WARMUP_INST, Sim: $SIM_INST."
                    fi
                fi                

                # Append the dynamically generated command
                if [[ "$CONFIG_NAME" == *"min"* || "$CONFIG_NAME" == *"max"* ]]; then
                    ONE_LINER="cd \"$TARGET_RESULT_DIR\" && \
                    for i in {1..12}; do \
                        echo \"Running Iteration \$i ...\"; \
                        \"$ABS_BIN\" --warmup-instructions $WARMUP_INST --simulation-instructions $SIM_INST \"$TRACE_PATH\" > log_\$i.txt && \
                        cp cache_phy_acc.txt cache_phy_acc_\$i.txt && \
                        cp oracle_pf_timing.txt oracle_pf_timing_\$i.txt; \
                    done"
                    echo "$ONE_LINER" >> "$PHASE_2_RUN_FILE" 
                else
                    echo "cd $TARGET_RESULT_DIR && $ABS_BIN --warmup-instructions $WARMUP_INST --simulation-instructions $SIM_INST $TRACE_PATH > $OUT" >> "$PHASE_1_RUN_FILE" 
                fi
            fi
        done
        
        echo "[✔] Setup completed for configuration: $CONFIG_NAME"
    fi
done

echo "---------------------------------------------------------"
echo "All configurations and traces are successfully set up!"
