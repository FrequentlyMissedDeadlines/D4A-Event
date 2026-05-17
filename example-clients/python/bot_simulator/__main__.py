"""Bot Simulator — package entry point.

Run with::

    python -m bot_simulator
"""

from .app import BotSimulatorApp


def main() -> None:
    """Launch the Bot Simulator GUI."""
    app = BotSimulatorApp()
    app.mainloop()


if __name__ == "__main__":
    main()
