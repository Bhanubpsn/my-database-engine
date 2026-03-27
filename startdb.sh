#!/bin/bash

# --- Color Palette ---
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
CYAN='\033[0;36m'
BOLD='\033[1m'
NC='\033[0m' # No Color

# --- Configuration ---
CPP_DIR="./db-engine"
GO_MAIN="main.go"
PRIMARY_PORT=50051
REPLICA1_PORT=50052
REPLICA2_PORT=50053
PRIMARY_DB="primary.db"
REPLICA1_DB="replica1.db"
REPLICA2_DB="replica2.db"

echo -e "${BLUE}${BOLD}>>> Starting Distributed DB Cluster Initialization <<<${NC}\n"

# --- 1. Cleanup ---
echo -e "${CYAN}[1/6] Cleaning up old processes...${NC}"
pkill db_engine || true
sleep 1

# --- 2. Build C++ Engine (Conditional) ---
# Only 'make' if the binary doesn't exist or c++ code has a change
echo -e "${CYAN}[2/6] Checking C++ Engine Build...${NC}"
cd $CPP_DIR

if [ ! -f "db_engine" ]; then
    echo -e "${YELLOW}Binary not found. Performing full build...${NC}"
    make clean > /dev/null 2>&1
    if make 2>/dev/null; then
        echo -e "${GREEN}✔ C++ Build Successful.${NC}"
    else
        echo -e "${RED}✘ C++ Build Failed! Check your code.${NC}"
        exit 1
    fi
else
    echo -e "${GREEN}✔ Existing binary found. Skipping build.${NC}"
fi
cd ..

# --- 3. Cold Boot Sync ---
echo -e "${CYAN}[3/6] Synchronizing Replicas...${NC}"
if [ -f "$PRIMARY_DB" ]; then
    echo -e "${GREEN}✔ Found Primary data ($PRIMARY_DB). Mirroring to replicas...${NC}"
    cp "$PRIMARY_DB" "$REPLICA1_DB"
    cp "$PRIMARY_DB" "$REPLICA2_DB"
else
    echo -e "${YELLOW}! No primary data found. Initializing empty cluster.${NC}"
fi

# --- 4. Go Preparation ---
echo -e "${CYAN}[4/6] Preparing Go Orchestrator...${NC}"
go mod tidy > /dev/null 2>&1
echo -e "${GREEN}✔ Go modules ready.${NC}"

# --- 5. Launch C++ Nodes ---
echo -e "${CYAN}[5/6] Launching gRPC Nodes...${NC}"

echo -e "  ${BLUE}→${NC} Primary   (Port $PRIMARY_PORT)..."
$CPP_DIR/db_engine $PRIMARY_DB $PRIMARY_PORT > primary.log 2>&1 &
PRIMARY_PID=$!

echo -e "  ${BLUE}→${NC} Replica 1 (Port $REPLICA1_PORT)..."
$CPP_DIR/db_engine $REPLICA1_DB $REPLICA1_PORT > replica1.log 2>&1 &
REPLICA1_PID=$!

echo -e "  ${BLUE}→${NC} Replica 2 (Port $REPLICA2_PORT)..."
$CPP_DIR/db_engine $REPLICA2_DB $REPLICA2_PORT > replica2.log 2>&1 &
REPLICA2_PID=$!

# Wait for ports to open
sleep 2

# --- 6. Execution ---
echo -e "\n${GREEN}${BOLD}==================================================${NC}"
echo -e "${GREEN}${BOLD}        DISTRIBUTED GO ORCHESTRATOR ACTIVE        ${NC}"
echo -e "${GREEN}${BOLD}==================================================${NC}\n"

# Run Go Orchestrator
go run $GO_MAIN

# --- Cleanup on Exit ---
echo -e "\n${CYAN}[6/6] Shutting down C++ nodes...${NC}"
kill $PRIMARY_PID $REPLICA1_PID $REPLICA2_PID 2>/dev/null
echo -e "${GREEN}${BOLD}✔ Cluster stopped. Goodbye!${NC}"
