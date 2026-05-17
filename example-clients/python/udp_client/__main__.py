"""K10 Bot UDP Client — package entry point.

Run from the ``example-clients/python`` directory with::

    python -m udp_client

This is equivalent to::

    python main.py
"""

import sys
from pathlib import Path

# Ensure the project root (containing config.py, customize.py, main.py)
# is on sys.path so all sibling-module imports resolve correctly.
_PROJECT_ROOT = Path(__file__).parent.parent
if str(_PROJECT_ROOT) not in sys.path:
    sys.path.insert(0, str(_PROJECT_ROOT))

from main import main  # noqa: E402

if __name__ == "__main__":
    main()
