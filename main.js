const SERVICE_UUID = "a1b2c3d4-e5f6-7a8b-9c0d-1e2f3a4b5c6d";
const CMD_CHAR_UUID = "a1b2c3d4-e5f6-7a8b-9c0d-1e2f3a4b5c6f";
const CFG_CHAR_UUID = "a1b2c3d4-e5f6-7a8b-9c0d-1e2f3a4b5c70";
const IMU_CHAR_UUID = "a1b2c3d4-e5f6-7a8b-9c0d-1e2f3a4b5c71";

let bleDevice;
let bleServer;
let cmdCharacteristic;
let cfgCharacteristic;
let imuCharacteristic;
let modeCharacteristic;
let colorCharacteristic;

const connectBtn = document.getElementById('connectBtn');
const statusDiv = document.getElementById('status');

// New Controls
const modeSelect = document.getElementById('mode-select');
const colorPicker = document.getElementById('color-picker');
const gyroSensInput = document.getElementById('gyro-sens');
const gyroSensVal = document.getElementById('gyro-sens-val');

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
        modeCharacteristic = await service.getCharacteristic("a1b2c3d4-e5f6-7a8b-9c0d-1e2f3a4b5c72");
        colorCharacteristic = await service.getCharacteristic("a1b2c3d4-e5f6-7a8b-9c0d-1e2f3a4b5c73");

        // Read initial config
        const cfgValue = await cfgCharacteristic.readValue();
        const timeout = cfgValue.getUint8(0);
        const brightness = cfgValue.getUint8(1);
        const sensitivity = cfgValue.getUint8(2);
        let gyroSens = 50;
        if (cfgValue.byteLength >= 4) {
            gyroSens = cfgValue.getUint8(3);
        }

        timeoutInput.value = timeout;
        timeoutVal.textContent = timeout;
        brightnessInput.value = brightness;
        brightnessVal.textContent = brightness;
        sensitivityInput.value = sensitivity;
        sensitivityVal.textContent = sensitivity;
        gyroSensInput.value = gyroSens;
        gyroSensVal.textContent = gyroSens;

        // Read initial mode and color
        const modeValue = await modeCharacteristic.readValue();
        modeSelect.value = modeValue.getUint8(0);

        const colorValue = await colorCharacteristic.readValue();
        const r = colorValue.getUint8(0);
        const g = colorValue.getUint8(1);
        const b = colorValue.getUint8(2);
        // Convert to hex for color picker
        const hex = "#" + (1 << 24 | r << 16 | g << 8 | b).toString(16).slice(1);
        colorPicker.value = hex;

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
    const gyroSens = parseInt(gyroSensInput.value);
    
    timeoutVal.textContent = timeout;
    brightnessVal.textContent = brightness;
    sensitivityVal.textContent = sensitivity;
    gyroSensVal.textContent = gyroSens;
    
    const data = new Uint8Array([timeout, brightness, sensitivity, gyroSens]);
    try {
        await cfgCharacteristic.writeValue(data);
    } catch (e) {
        console.error("Failed to write config", e);
    }
}

modeSelect.addEventListener('change', async () => {
    if (!modeCharacteristic) return;
    const mode = parseInt(modeSelect.value);
    try {
        await modeCharacteristic.writeValue(new Uint8Array([mode]));
    } catch (e) {
        console.error("Failed to write mode", e);
    }
});

colorPicker.addEventListener('change', async (e) => {
    if (!colorCharacteristic) return;
    const hex = e.target.value;
    const r = parseInt(hex.slice(1, 3), 16);
    const g = parseInt(hex.slice(3, 5), 16);
    const b = parseInt(hex.slice(5, 7), 16);
    try {
        await colorCharacteristic.writeValue(new Uint8Array([r, g, b]));
        // Auto-switch to solid color mode when color is picked
        if (modeSelect.value != "1") {
            modeSelect.value = "1";
            modeSelect.dispatchEvent(new Event('change'));
        }
    } catch (err) {
        console.error("Failed to write color", err);
    }
});

brightnessInput.addEventListener('change', updateConfig);
sensitivityInput.addEventListener('change', updateConfig);
gyroSensInput.addEventListener('change', updateConfig);
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
const container = document.getElementById('viewer-container');
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

// Auto-fit camera to object
function fitCameraToObject(camera, object, offset = 1.25) {
    const boundingBox = new THREE.Box3();
    boundingBox.setFromObject(object);
    const center = boundingBox.getCenter(new THREE.Vector3());
    const size = boundingBox.getSize(new THREE.Vector3());
    
    const maxDim = Math.max(size.x, size.y, size.z);
    const fov = camera.fov * (Math.PI / 180);
    let cameraZ = Math.abs((maxDim / 2) / Math.tan(fov / 2));
    
    cameraZ *= offset;
    
    camera.position.z = center.z + cameraZ;
    
    const minZ = boundingBox.min.z;
    const cameraToFarEdge = (minZ < 0) ? -minZ + cameraZ : cameraZ - minZ;
    camera.far = cameraToFarEdge * 3;
    camera.updateProjectionMatrix();
    camera.lookAt(center);
}

// Load default STL model
let targetObject = new THREE.Group(); // Placeholder until loaded
scene.add(targetObject);

const loader = new THREE.STLLoader();
loader.load('./webAppSphere.stl', function (geometry) {
    geometry.computeVertexNormals();
    geometry.center();
    
    const material = new THREE.MeshStandardMaterial({ color: 0xcd7f32, roughness: 0.5, metalness: 0.8 });
    const mesh = new THREE.Mesh(geometry, material);
    
    scene.remove(targetObject);
    targetObject = mesh;
    scene.add(targetObject);
    
    fitCameraToObject(camera, targetObject);
}, undefined, function (error) {
    console.error("Failed to load webAppSphere.stl, using placeholder", error);
    scene.remove(targetObject);
    targetObject = new THREE.Mesh(
        new THREE.SphereGeometry(1.5, 32, 32),
        new THREE.MeshStandardMaterial({ color: 0xcd7f32, wireframe: true })
    );
    scene.add(targetObject);
    fitCameraToObject(camera, targetObject);
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
        
        scene.add(targetObject);
        fitCameraToObject(camera, targetObject);
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
