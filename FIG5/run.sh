#!/bin/bash
# Install dependencies and run Fig 5
# Note: --break-system-packages may be needed on some systems

pip install --break-system-packages -r requirements.txt 2>/dev/null || pip install -r requirements.txt
python3 fig5.py