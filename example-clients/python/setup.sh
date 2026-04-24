#!/bin/bash
# Setup script for K10 Bot UDP Client

set -e

echo "🤖 K10 Bot UDP Client - Setup"
echo "=============================="

# Check Python version
python_version=$(python3 --version 2>&1 | awk '{print $2}')
echo "✓ Python version: $python_version"

# Create virtual environment if it doesn't exist
if [ ! -d "venv" ]; then
    echo "📦 Creating virtual environment..."
    python3 -m venv venv
fi

# Activate virtual environment
echo "✓ Activating virtual environment..."
source venv/bin/activate

# Upgrade pip
echo "📥 Upgrading pip..."
pip install --upgrade pip setuptools wheel

# Install dependencies
echo "📥 Installing dependencies..."
pip install -r requirements.txt

echo ""
echo "✅ Setup complete!"
echo ""
echo "To run the client:"
echo "  source venv/bin/activate"
echo "  python main.py"
echo ""
