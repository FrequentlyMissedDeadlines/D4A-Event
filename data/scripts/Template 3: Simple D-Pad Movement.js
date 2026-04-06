// Template 3: Simple D-Pad Movement
// Use this for basic forward/back/left/right control with the gamepad D-Pad.

const LEFT_WHEEL = 0;
const RIGHT_WHEEL = 1;

function moveForward() {
  setServoSpeeds([[LEFT_WHEEL, 100], [RIGHT_WHEEL, 100]]);
  _scriptLog('→ Moving forward');
}

function moveBackward() {
  setServoSpeeds([[LEFT_WHEEL, -100], [RIGHT_WHEEL, -100]]);
  _scriptLog('← Moving backward');
}

function turnLeft() {
  setServoSpeeds([[LEFT_WHEEL, -100], [RIGHT_WHEEL, 100]]);
  _scriptLog('↙ Turning left');
}

function turnRight() {
  setServoSpeeds([[LEFT_WHEEL, 100], [RIGHT_WHEEL, -100]]);
  _scriptLog('↘ Turning right');
}

function stop() {
  setServoSpeeds([[LEFT_WHEEL, 0], [RIGHT_WHEEL, 0]]);
  _scriptLog('⏸ Stopped');
}

// Attach servos
attachServo(LEFT_WHEEL, SERVO_TYPES.ROTATIONAL);
attachServo(RIGHT_WHEEL, SERVO_TYPES.ROTATIONAL);
_scriptLog('✓ Servos ready for D-Pad control');

// Handle D-Pad input
CUSTOMCONTROL.processGamepadInput = function(gamepad) {
  if (gamepad.buttons[XBOX_BUTTONS.DPAD_UP].pressed) {
    moveForward();
  } else if (gamepad.buttons[XBOX_BUTTONS.DPAD_DOWN].pressed) {
    moveBackward();
  } else if (gamepad.buttons[XBOX_BUTTONS.DPAD_LEFT].pressed) {
    turnLeft();
  } else if (gamepad.buttons[XBOX_BUTTONS.DPAD_RIGHT].pressed) {
    turnRight();
  } else {
    stop();
  }
};