
        async function setAngle() {
            const ch = document.getElementById('angle_ch').value;
            const angle = document.getElementById('angle_val').value;
            try {
                const response = await fetch(`/api/servos/v1/setServoAngle?channel=${ch}&angle=${angle}`, { method: 'POST' });
                const data = await response.json();
                document.getElementById('angle_result').innerHTML = 
                    response.ok ? `✓ Channel ${ch} set to ${angle}°` : `✗ Error: ${data.message || data.result}`;
            } catch(e) {
                document.getElementById('angle_result').innerHTML = `✗ Error: ${e.message}`;
            }
        }
        
        async function setSpeed() {
            const ch = document.getElementById('speed_ch').value;
            const speed = document.getElementById('speed_val').value;
            try {
                const response = await fetch(`/api/servos/v1/setServoSpeed?channel=${ch}&speed=${speed}`, { method: 'POST' });
                const data = await response.json();
                document.getElementById('speed_result').innerHTML = 
                    response.ok ? `✓ Channel ${ch} speed set to ${speed}%` : `✗ Error: ${data.message || data.result}`;
            } catch(e) {
                document.getElementById('speed_result').innerHTML = `✗ Error: ${e.message}`;
            }
        }
        
        async function centerAll() {
            try {
                const response = await fetch('/api/servos/v1/setAllServoAngle?angle=90', { method: 'POST' });
                const data = await response.json();
                document.getElementById('action_result').innerHTML = 
                    response.ok ? '✓ All servos centered' : `✗ Error: ${data.message || data.result}`;
            } catch(e) {
                document.getElementById('action_result').innerHTML = `✗ Error: ${e.message}`;
            }
        }
        
        async function stopAll() {
            try {
                const response = await fetch('/api/servos/v1/stopAll', { method: 'POST' });
                const data = await response.json();
                document.getElementById('action_result').innerHTML = 
                    response.ok ? '✓ All servos stopped' : `✗ Error: ${data.message || data.result}`;
            } catch(e) {
                document.getElementById('action_result').innerHTML = `✗ Error: ${e.message}`;
            }
        }