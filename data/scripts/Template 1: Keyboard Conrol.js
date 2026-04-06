// Template 1: Keyboard Control (WASD)
// Use this for keyboard-based movement on a diff-drive (tank) robot.
// W/S = forward/backward, A/D = turn left/right.
const LEFT_WHEEL = 0;
const RIGHT_WHEEL = 1;
const keys = {};

function updateMovement() {
  const w = keys['w'];
  const a = keys['a'];
  const s = keys['s'];
  const d = keys['d'];

  let leftSpeed = 0;
  let rightSpeed = 0;

  if (w) { leftSpeed += 100; rightSpeed += 100; }
  if (s) { leftSpeed -= 100; rightSpeed -= 100; }
  if (a) { leftSpeed -= 50;  rightSpeed += 50; }
  if (d) { leftSpeed += 50;  rightSpeed -= 50; }

  setServoSpeeds([[LEFT_WHEEL, leftSpeed], [RIGHT_WHEEL, rightSpeed]]);
}

// Step 1 — attach servos
attachServo(LEFT_WHEEL,  SERVO_TYPES.ROTATIONAL);
attachServo(RIGHT_WHEEL, SERVO_TYPES.ROTATIONAL);
_scriptLog('✓ Keyboard control ready (W=forward, S=back, A=left, D=right)');

// Step 2 — override keyboard hooks
CUSTOMCONTROL.onKeyDown = function(event) {
  const key = event.key.toLowerCase();
  keys[key] = true;
  updateMovement();
};

CUSTOMCONTROL.onKeyUp = function(event) {
  const key = event.key.toLowerCase();
  delete keys[key];
  updateMovement();
};