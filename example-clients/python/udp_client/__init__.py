"""
K10 Bot UDP Client - A cross-platform Textual TUI for controlling K10 Bot via UDP

Run the client with::

    python -m udp_client
"""

__version__ = "0.1.0"


def main() -> None:
    """Launch the K10 Bot UDP client TUI.

    Delegates to ``main.py`` in the project root, resolving the path
    automatically so this works regardless of the current working directory.
    """
    import sys
    from pathlib import Path

    _root = Path(__file__).parent.parent
    if str(_root) not in sys.path:
        sys.path.insert(0, str(_root))

    from main import main as _main  # noqa: PLC0415

    _main()
