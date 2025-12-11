#!/bin/bash

# K10 UDP Receiver - Upload Helper Script
# This script provides convenient aliases for uploading firmware and filesystem to the K10 device

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Get the project directory (where this script is located)
PROJECT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

echo -e "${BLUE}=== K10 UDP Receiver Upload Helper ===${NC}"
echo "Project directory: $PROJECT_DIR"
echo ""

# Check if device is connected
if ! ls /dev/ttyACM0 >/dev/null 2>&1; then
    echo -e "${RED}Error: Device not found at /dev/ttyACM0${NC}"
    echo "Available serial ports:"
    ls /dev/tty* | grep -E "USB|ACM|usbserial"
    exit 1
fi

echo -e "${GREEN}✓ Device found at /dev/ttyACM0${NC}"
echo ""

# Parse command line arguments
COMMAND=${1:-help}

case "$COMMAND" in
    firmware)
        echo -e "${BLUE}Uploading firmware...${NC}"
        python ~/.platformio/packages/tool-esptoolpy/esptool.py --port /dev/ttyACM0 write_flash \
            0x0 "$PROJECT_DIR/.pio/build/unihiker_k10/bootloader.bin" \
            0x8000 "$PROJECT_DIR/.pio/build/unihiker_k10/partitions.bin" \
            0x10000 "$PROJECT_DIR/.pio/build/unihiker_k10/firmware.bin"
        echo -e "${GREEN}✓ Firmware upload complete${NC}"
        ;;
    
    full)
        echo -e "${BLUE}Uploading firmware (full system)...${NC}"
        python ~/.platformio/packages/tool-esptoolpy/esptool.py --port /dev/ttyACM0 write_flash \
            0x0 "$PROJECT_DIR/.pio/build/unihiker_k10/bootloader.bin" \
            0x8000 "$PROJECT_DIR/.pio/build/unihiker_k10/partitions.bin" \
            0x10000 "$PROJECT_DIR/.pio/build/unihiker_k10/firmware.bin"
        echo -e "${GREEN}✓ Full system upload complete${NC}"
        ;;
    
    build)
        echo -e "${BLUE}Building firmware...${NC}"
        cd "$PROJECT_DIR"
        source venv/bin/activate
        pio run
        echo -e "${GREEN}✓ Build complete${NC}"
        ;;
    
    build-upload)
        echo -e "${BLUE}Building and uploading...${NC}"
        cd "$PROJECT_DIR"
        source venv/bin/activate
        pio run
        python ~/.platformio/packages/tool-esptoolpy/esptool.py --port /dev/ttyACM0 write_flash \
            0x0 "$PROJECT_DIR/.pio/build/unihiker_k10/bootloader.bin" \
            0x8000 "$PROJECT_DIR/.pio/build/unihiker_k10/partitions.bin" \
            0x10000 "$PROJECT_DIR/.pio/build/unihiker_k10/firmware.bin"
        echo -e "${GREEN}✓ Build and upload complete${NC}"
        ;;
    
    monitor)
        echo -e "${BLUE}Opening serial monitor...${NC}"
        cd "$PROJECT_DIR"
        source venv/bin/activate
        pio run --monitor
        ;;
    
    help|*)
        echo "Usage: ./upload.sh <command>"
        echo ""
        echo "Commands:"
        echo -e "  ${GREEN}firmware${NC}       - Upload firmware only"
        echo -e "  ${GREEN}full${NC}           - Upload firmware (full system)"
        echo -e "  ${GREEN}build${NC}          - Build firmware"
        echo -e "  ${GREEN}build-upload${NC}   - Build and upload"
        echo -e "  ${GREEN}monitor${NC}        - Open serial monitor"
        echo -e "  ${GREEN}help${NC}           - Show this help message"
        echo ""
        echo "Examples:"
        echo "  ./upload.sh build-upload      # Build and upload"
        echo "  ./upload.sh firmware          # Upload only"
        echo "  ./upload.sh monitor           # Open serial monitor"
        ;;
esac
