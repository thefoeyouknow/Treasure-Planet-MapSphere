const SERVICE_UUID = "a1b2c3d4-e5f6-7a8b-9c0d-1e2f3a4b5c6d";
const CMD_CHAR_UUID = "a1b2c3d4-e5f6-7a8b-9c0d-1e2f3a4b5c6f";
const CFG_CHAR_UUID = "a1b2c3d4-e5f6-7a8b-9c0d-1e2f3a4b5c70";
const IMU_CHAR_UUID = "a1b2c3d4-e5f6-7a8b-9c0d-1e2f3a4b5c71";

let bleDevice;
let bleServer;
let cmdCharacteristic;
let cfgCharacteristic;
let imuCharacteristic;

const connectBtn = document.getElementById('connectBtn');
const statusDiv = document.getElementById('status');

// Sliders
const brightnessInput = document.getElementById('brightness');
const brightnessVal = document.getElementById('brightness-val');
const sensitivityInput = document.getElementById('sensitivity');
const sensitivityVal = document.getElementById('sensitivity-val');
const timeoutInput = document.getElementById('timeout');
const timeoutVal = document.getElementById('timeout-val');

// Connect
connectBtn.addEventListener('click', async () => {
    if (bleDevice && bleDevice.gatt.connected) {
        bleDevice.gatt.disconnect();
        return;
    }
    
    try {
        console.log('Requesting Bluetooth Device...');
        bleDevice = await navigator.bluetooth.requestDevice({
            filters: [{ services: [SERVICE_UUID] }],
            optionalServices: [SERVICE_UUID]
        });

        bleDevice.addEventListener('gattserverdisconnected', onDisconnected);

        console.log('Connecting to GATT Server...');
        statusDiv.textContent = 'Connecting...';
        bleServer = await bleDevice.gatt.connect();

        console.log('Getting Service...');
        const service = await bleServer.getPrimaryService(SERVICE_UUID);

        console.log('Getting Characteristics...');
        cmdCharacteristic = await service.getCharacteristic(CMD_CHAR_UUID);
        cfgCharacteristic = await service.getCharacteristic(CFG_CHAR_UUID);
        imuCharacteristic = await service.getCharacteristic(IMU_CHAR_UUID);

        // Read initial config
        const cfgValue = await cfgCharacteristic.readValue();
        const timeout = cfgValue.getUint8(0);
        const brightness = cfgValue.getUint8(1);
        const sensitivity = cfgValue.getUint8(2);

        timeoutInput.value = timeout;
        timeoutVal.textContent = timeout;
        brightnessInput.value = brightness;
        brightnessVal.textContent = brightness;
        sensitivityInput.value = sensitivity;
        sensitivityVal.textContent = sensitivity;

        // Start IMU notifications
        await imuCharacteristic.startNotifications();
        imuCharacteristic.addEventListener('characteristicvaluechanged', handleIMUData);

        statusDiv.textContent = 'Connected to MapSphere';
        connectBtn.textContent = 'Disconnect';

    } catch (error) {
        console.error('Argh! ' + error);
        statusDiv.textContent = 'Connection failed: ' + error;
    }
});

function onDisconnected() {
    statusDiv.textContent = 'Disconnected';
    connectBtn.textContent = 'Connect BLE';
}

// Config Updates
async function updateConfig() {
    if (!cfgCharacteristic) return;
    
    const timeout = parseInt(timeoutInput.value);
    const brightness = parseInt(brightnessInput.value);
    const sensitivity = parseInt(sensitivityInput.value);
    
    timeoutVal.textContent = timeout;
    brightnessVal.textContent = brightness;
    sensitivityVal.textContent = sensitivity;
    
    const data = new Uint8Array([timeout, brightness, sensitivity]);
    try {
        await cfgCharacteristic.writeValue(data);
    } catch (e) {
        console.error("Failed to write config", e);
    }
}

brightnessInput.addEventListener('change', updateConfig);
sensitivityInput.addEventListener('change', updateConfig);
timeoutInput.addEventListener('change', updateConfig);

// Command Buttons
document.querySelectorAll('.actions button').forEach(btn => {
    btn.addEventListener('click', async (e) => {
        if (!cmdCharacteristic) return;
        const cmd = parseInt(e.target.dataset.cmd);
        const data = new Uint8Array([cmd]);
        try {
            await cmdCharacteristic.writeValue(data);
        } catch (err) {
            console.error("Failed to send command", err);
        }
    });
});

// --- Three.js 3D Setup ---
const container = document.getElementById('3d-container');
const scene = new THREE.Scene();
const camera = new THREE.PerspectiveCamera(45, container.clientWidth / container.clientHeight, 0.1, 100);
camera.position.z = 5;

const renderer = new THREE.WebGLRenderer({ antialias: true, alpha: true });
renderer.setSize(container.clientWidth, container.clientHeight);
container.appendChild(renderer.domElement);

const ambientLight = new THREE.AmbientLight(0xffffff, 0.5);
scene.add(ambientLight);
const dirLight = new THREE.DirectionalLight(0xffffff, 0.8);
dirLight.position.set(10, 10, 10);
scene.add(dirLight);

// Load default STL model
let targetObject = new THREE.Group(); // Placeholder until loaded
scene.add(targetObject);

const loader = new THREE.STLLoader();
loader.load('./webAppSphere.stl', function (geometry) {
    geometry.computeVertexNormals();
    geometry.center();
    
    const material = new THREE.MeshStandardMaterial({ color: 0xcd7f32, roughness: 0.5, metalness: 0.8 });
    const mesh = new THREE.Mesh(geometry, material);
    mesh.scale.set(0.05, 0.05, 0.05); // Adjust scale based on common STL sizes
    
    scene.remove(targetObject);
    targetObject = mesh;
    scene.add(targetObject);
}, undefined, function (error) {
    console.error("Failed to load webAppSphere.stl, using placeholder", error);
    scene.remove(targetObject);
    targetObject = new THREE.Mesh(
        new THREE.SphereGeometry(1.5, 32, 32),
        new THREE.MeshStandardMaterial({ color: 0xcd7f32, wireframe: true })
    );
    scene.add(targetObject);
});

const stlUpload = document.getElementById('stl-upload');
stlUpload.addEventListener('change', (e) => {
    const file = e.target.files[0];
    if (!file) return;
    const reader = new FileReader();
    reader.onload = function(event) {
        const contents = event.target.result;
        const loader = new THREE.STLLoader();
        const geometry = loader.parse(contents);
        
        geometry.computeVertexNormals();
        geometry.center();
        
        // Remove old object
        scene.remove(targetObject);
        
        const material = new THREE.MeshStandardMaterial({ color: 0xcd7f32, roughness: 0.5, metalness: 0.8 });
        targetObject = new THREE.Mesh(geometry, material);
        
        // Scale to fit
        targetObject.scale.set(0.05, 0.05, 0.05); // Adjust scale based on common STL sizes
        
        scene.add(targetObject);
    };
    reader.readAsArrayBuffer(file);
});

function animate() {
    requestAnimationFrame(animate);
    renderer.render(scene, camera);
}
animate();

window.addEventListener('resize', () => {
    camera.aspect = container.clientWidth / container.clientHeight;
    camera.updateProjectionMatrix();
    renderer.setSize(container.clientWidth, container.clientHeight);
});

// IMU Handling
// Gravity vector maps to pitch and roll
function handleIMUData(event) {
    const value = event.target.value;
    const gx = value.getFloat32(0, true);
    const gy = value.getFloat32(4, true);
    const gz = value.getFloat32(8, true);

    // Basic complementary or direct mapping
    // If device rests flat on Y axis (as per seed doc),
    // Gravity is [0, 1, 0].
    
    // Calculate pitch and roll from gravity vector
    // atan2(x, sqrt(y^2 + z^2))
    const pitch = Math.atan2(gx, Math.sqrt(gy * gy + gz * gz));
    const roll = Math.atan2(gz, Math.sqrt(gx * gx + gy * gy));
    
    // Note: This simple mapping doesn't track yaw. 
    // For full 6DOF you need a fusion algorithm (Madgwick/Mahony) running on the device, sending quaternions.
    // This provides a cool reactive visual based on tilt.
    
    // Smooth transition
    targetObject.rotation.x += (roll - targetObject.rotation.x) * 0.2;
    targetObject.rotation.z += (-pitch - targetObject.rotation.z) * 0.2;
}
