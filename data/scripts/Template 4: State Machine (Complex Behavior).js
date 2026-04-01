//Template 4: State Machine (Complex Behavior)
//Use this when your bot needs different modes (explore, capture, return).

onst LEFT_WHEEL = 0;
const RIGHT_WHEEL = 1;
const ARM = 2;

let currentState = 'idle';

function setState(newState) {
  currentState = newState;
  _scriptLog('→ State: ' + newState);
}

async function explore() {
  if (currentState !== 'explore') return;
  _scriptLog('Exploring...');
  
  setServoSpeeds([[LEFT_WHEEL, 100], [RIGHT_WHEEL, 100]]);
  await delay(2000);
  
  if (currentState === 'explore') {
    setServoSpeeds([[LEFT_WHEEL, 0], [RIGHT_WHEEL, 0]]);
    setState('idle');
  }
}

async function capture() {
  if (currentState !== 'capture') return;
  setServoAngle(ARM, 90);
  await delay(500);
  setServoAngle(ARM, 0);
  
  if (currentState === 'capture') {
    setState('idle');
  }
}

// Attach servos
attachServo(LEFT_WHEEL, SERVO_TYPES.ROTATIONAL);
attachServo(RIGHT_WHEEL, SERVO_TYPES.ROTATIONAL);
attachServo(ARM, SERVO_TYPES.ANGULAR_270);

// Gamepad control
CUSTOMCONTROL.processGamepadInput = function(gamepad) {
  if (gamepad.buttons[XBOX_BUTTONS.A].pressed && currentState !== 'explore') {
    setState('explore');
    explore();
  } else if (gamepad.buttons[XBOX_BUTTONS.B].pressed && currentState !== 'capture') {
    setState('capture');
    capture();
  }
};