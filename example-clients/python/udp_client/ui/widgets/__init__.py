"""
Reusable TUI widget components for the K10 Bot client.

All widgets are re-exported here for convenient imports::

    from udp_client.ui.widgets import (
        ConnectionWizardScreen,
        CurrentActionIndicator,
        ServerConnectionPanel,
        DeviceListPanel,
        JoystickVisualizerPanel,
        KeyboardDisplayPanel,
        StatisticsDisplayPanel,
        ActionHistoryPanel,
        BotStatusPanel,
        BotConfigInspectorPanel,
        VirtualDpadPanel,
        HelpDisplayPanel,
    )
"""

from udp_client.ui.widgets.action_history import ActionHistoryPanel
from udp_client.ui.widgets.action_indicator import CurrentActionIndicator
from udp_client.ui.widgets.bot_status import BotStatusPanel
from udp_client.ui.widgets.config_inspector import BotConfigInspectorPanel
from udp_client.ui.widgets.connection_wizard import ConnectionWizardScreen
from udp_client.ui.widgets.device_list import DeviceListPanel
from udp_client.ui.widgets.help_panel import HelpDisplayPanel
from udp_client.ui.widgets.joystick_visualizer import JoystickVisualizerPanel
from udp_client.ui.widgets.keyboard_display import KeyboardDisplayPanel
from udp_client.ui.widgets.server_panel import ServerConnectionPanel
from udp_client.ui.widgets.statistics import StatisticsDisplayPanel
from udp_client.ui.widgets.virtual_dpad import VirtualDpadPanel

__all__ = [
    "ConnectionWizardScreen",
    "CurrentActionIndicator",
    "ServerConnectionPanel",
    "DeviceListPanel",
    "JoystickVisualizerPanel",
    "KeyboardDisplayPanel",
    "StatisticsDisplayPanel",
    "ActionHistoryPanel",
    "BotStatusPanel",
    "BotConfigInspectorPanel",
    "VirtualDpadPanel",
    "HelpDisplayPanel",
]
