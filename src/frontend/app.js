// =============================================================================
// PASS Unified Engineering Workbench - Application Controller
// =============================================================================

const API_BASE = window.location.origin;

// Tab Management
function switchTab(tabName) {
  document.querySelectorAll('.tab-btn').forEach(btn => btn.classList.remove('active'));
  document.querySelectorAll('.view-panel').forEach(panel => panel.classList.remove('active'));

  const targetPanel = document.getElementById(`view-${tabName}`);
  if (targetPanel) {
    targetPanel.classList.add('active');
  }

  // Highlight button
  const btns = Array.from(document.querySelectorAll('.tab-btn'));
  const match = btns.find(b => b.getAttribute('onclick')?.includes(tabName));
  if (match) match.classList.add('active');

  // Trigger resize on Three.js canvases when made visible
  if (tabName === 'geometry') {
    onStlWindowResize();
  } else if (tabName === 'thermal') {
    onAnsysWindowResize();
    runFeaSolve();
    runFeaConvergence();
  } else if (tabName === 'performance') {
    renderMulticoreChart();
  } else if (tabName === 'backprop') {
    loadBackpropStudy();
  }
}

function switchThermalSubtab(sub) {
  const feaDiv = document.getElementById('thermal-subtab-fea');
  const ansysDiv = document.getElementById('thermal-subtab-ansys');
  const feaBtn = document.getElementById('thermal-subtab-fea-btn');
  const ansysBtn = document.getElementById('thermal-subtab-ansys-btn');

  if (sub === 'fea') {
    feaDiv.style.display = 'block';
    ansysDiv.style.display = 'none';
    feaBtn.className = 'btn';
    ansysBtn.className = 'btn btn-secondary';
    runFeaSolve();
  } else {
    feaDiv.style.display = 'none';
    ansysDiv.style.display = 'block';
    feaBtn.className = 'btn btn-secondary';
    ansysBtn.className = 'btn';
    onAnsysWindowResize();
    loadAnsysPreset();
  }
}

// =============================================================================
// 1. THREE.JS STL GEOMETRY VIEWER
// =============================================================================
let stlScene, stlCamera, stlRenderer, stlMeshObject;
let isStlDragging = false, stlPrevMouse = { x: 0, y: 0 };
let stlWireframe = false;
let currentStlBounds = { center: [0,0,0], radius: 1 };

function initStlViewer() {
  const container = document.getElementById('stl-canvas-container');
  if (!container || typeof THREE === 'undefined') return;

  const width = container.clientWidth || 600;
  const height = container.clientHeight || 520;

  stlScene = new THREE.Scene();
  stlScene.background = new THREE.Color(0x0a0f1d);

  stlCamera = new THREE.PerspectiveCamera(45, width / height, 0.001, 1000);
  stlCamera.position.set(0.1, 0.1, 0.15);

  stlRenderer = new THREE.WebGLRenderer({ antialias: true });
  stlRenderer.setSize(width, height);
  stlRenderer.setPixelRatio(window.devicePixelRatio);
  container.appendChild(stlRenderer.domElement);

  // Lighting
  const ambLight = new THREE.AmbientLight(0xffffff, 0.6);
  stlScene.add(ambLight);

  const dirLight1 = new THREE.DirectionalLight(0x38bdf8, 0.8);
  dirLight1.position.set(1, 1, 1).normalize();
  stlScene.add(dirLight1);

  const dirLight2 = new THREE.DirectionalLight(0xffffff, 0.5);
  dirLight2.position.set(-1, -1, -1).normalize();
  stlScene.add(dirLight2);

  // Grid floor
  const grid = new THREE.GridHelper(0.3, 20, 0x38bdf8, 0x1e293b);
  grid.position.y = -0.02;
  stlScene.add(grid);

  // Mouse interaction
  const dom = stlRenderer.domElement;
  dom.addEventListener('mousedown', (e) => {
    isStlDragging = true;
    stlPrevMouse = { x: e.clientX, y: e.clientY };
  });

  window.addEventListener('mouseup', () => { isStlDragging = false; });

  dom.addEventListener('mousemove', (e) => {
    if (!isStlDragging || !stlMeshObject) return;
    const dx = e.clientX - stlPrevMouse.x;
    const dy = e.clientY - stlPrevMouse.y;

    if (e.buttons === 1) { // Rotate
      stlMeshObject.rotation.y += dx * 0.01;
      stlMeshObject.rotation.x += dy * 0.01;
    } else if (e.buttons === 2) { // Pan
      stlCamera.position.x -= dx * 0.0005 * currentStlBounds.radius;
      stlCamera.position.y += dy * 0.0005 * currentStlBounds.radius;
    }
    stlPrevMouse = { x: e.clientX, y: e.clientY };
  });

  dom.addEventListener('wheel', (e) => {
    e.preventDefault();
    const factor = e.deltaY > 0 ? 1.08 : 0.92;
    stlCamera.position.multiplyScalar(factor);
  }, { passive: false });

  dom.addEventListener('contextmenu', (e) => e.preventDefault());

  window.addEventListener('resize', onStlWindowResize);

  function animate() {
    requestAnimationFrame(animate);
    stlRenderer.render(stlScene, stlCamera);
  }
  animate();
}

function onStlWindowResize() {
  const container = document.getElementById('stl-canvas-container');
  if (!container || !stlRenderer || !stlCamera) return;
  const width = container.clientWidth;
  const height = container.clientHeight;
  stlCamera.aspect = width / height;
  stlCamera.updateProjectionMatrix();
  stlRenderer.setSize(width, height);
}

function resetStlCamera() {
  if (!stlCamera || !currentStlBounds) return;
  const r = Math.max(currentStlBounds.radius, 0.05);
  stlCamera.position.set(r * 1.5, r * 1.2, r * 2.0);
  stlCamera.lookAt(new THREE.Vector3(0, 0, 0));
  if (stlMeshObject) {
    stlMeshObject.rotation.set(0, 0, 0);
  }
}

function toggleStlWireframe() {
  if (!stlMeshObject) return;
  stlWireframe = !stlWireframe;
  stlMeshObject.traverse((child) => {
    if (child.isMesh) child.material.wireframe = stlWireframe;
  });
}

function renderStlMesh(data) {
  const errBanner = document.getElementById('stl-error-banner');
  const overlayStatus = document.getElementById('stl-overlay-status');

  if (!data.valid) {
    errBanner.style.display = 'block';
    errBanner.textContent = `STL Validation Error: ${data.error}`;
    overlayStatus.textContent = `Error: ${data.error}`;
    return;
  }

  errBanner.style.display = 'none';
  overlayStatus.textContent = `Loaded: ${data.solidName} (${data.triangleCount} triangles)`;

  document.getElementById('stl-meta-solid').textContent = data.solidName;
  document.getElementById('stl-meta-triangles').textContent = data.triangleCount.toLocaleString();
  document.getElementById('stl-meta-bbox-x').textContent = `[${data.bounds.min[0].toFixed(4)}, ${data.bounds.max[0].toFixed(4)}] m`;
  document.getElementById('stl-meta-bbox-y').textContent = `[${data.bounds.min[1].toFixed(4)}, ${data.bounds.max[1].toFixed(4)}] m`;
  document.getElementById('stl-meta-bbox-z').textContent = `[${data.bounds.min[2].toFixed(4)}, ${data.bounds.max[2].toFixed(4)}] m`;
  document.getElementById('stl-meta-radius').textContent = `${data.bounds.radius.toFixed(4)} m`;

  currentStlBounds = data.bounds;

  if (stlMeshObject) {
    stlScene.remove(stlMeshObject);
  }

  const geometry = new THREE.BufferGeometry();
  const positions = [];
  const normals = [];

  const cx = data.bounds.center[0];
  const cy = data.bounds.center[1];
  const cz = data.bounds.center[2];

  for (const tri of data.triangles) {
    normals.push(tri.n[0], tri.n[1], tri.n[2]);
    normals.push(tri.n[0], tri.n[1], tri.n[2]);
    normals.push(tri.n[0], tri.n[1], tri.n[2]);

    positions.push(tri.v[0] - cx, tri.v[1] - cy, tri.v[2] - cz);
    positions.push(tri.v[3] - cx, tri.v[4] - cy, tri.v[5] - cz);
    positions.push(tri.v[6] - cx, tri.v[7] - cy, tri.v[8] - cz);
  }

  geometry.setAttribute('position', new THREE.Float32BufferAttribute(positions, 3));
  geometry.setAttribute('normal', new THREE.Float32BufferAttribute(normals, 3));

  const material = new THREE.MeshPhongMaterial({
    color: 0x38bdf8,
    specular: 0x111111,
    shininess: 40,
    side: THREE.DoubleSide,
    wireframe: stlWireframe
  });

  stlMeshObject = new THREE.Mesh(geometry, material);
  stlScene.add(stlMeshObject);

  resetStlCamera();
}

async function loadStlPreset() {
  const preset = document.getElementById('stl-preset-select').value;
  try {
    const res = await fetch(preset);
    const content = await res.text();
    const parseRes = await fetch('/api/stl/parse', {
      method: 'POST',
      body: content
    });
    const json = await parseRes.json();
    renderStlMesh(json);
  } catch (err) {
    const errBanner = document.getElementById('stl-error-banner');
    errBanner.style.display = 'block';
    errBanner.textContent = `Failed to load STL: ${err.message}`;
  }
}

function handleStlFileUpload(e) {
  const file = e.target.files[0];
  if (!file) return;
  const reader = new FileReader();
  reader.onload = async (evt) => {
    try {
      const parseRes = await fetch('/api/stl/parse', {
        method: 'POST',
        body: evt.target.result
      });
      const json = await parseRes.json();
      renderStlMesh(json);
    } catch (err) {
      alert(`Parse error: ${err.message}`);
    }
  };
  reader.readAsText(file);
}

// =============================================================================
// 2. ANSYS 3D POINT CLOUD VIEWER
// =============================================================================
let ansysScene, ansysCamera, ansysRenderer, ansysPointsObject;
let isAnsysDragging = false, ansysPrevMouse = { x: 0, y: 0 };
let currentAnsysNodes = [];
let ansysBounds = { center: [0,0,0], radius: 1 };

function initAnsysViewer() {
  const container = document.getElementById('ansys-canvas-container');
  if (!container || typeof THREE === 'undefined') return;

  const width = container.clientWidth || 600;
  const height = container.clientHeight || 520;

  ansysScene = new THREE.Scene();
  ansysScene.background = new THREE.Color(0x0a0f1d);

  ansysCamera = new THREE.PerspectiveCamera(45, width / height, 0.001, 1000);
  ansysCamera.position.set(0.1, 0.1, 0.15);

  ansysRenderer = new THREE.WebGLRenderer({ antialias: true });
  ansysRenderer.setSize(width, height);
  ansysRenderer.setPixelRatio(window.devicePixelRatio);
  container.appendChild(ansysRenderer.domElement);

  const ambLight = new THREE.AmbientLight(0xffffff, 0.8);
  ansysScene.add(ambLight);

  const grid = new THREE.GridHelper(0.3, 20, 0x06b6d4, 0x1e293b);
  grid.position.y = -0.02;
  ansysScene.add(grid);

  const dom = ansysRenderer.domElement;
  dom.addEventListener('mousedown', (e) => {
    isAnsysDragging = true;
    ansysPrevMouse = { x: e.clientX, y: e.clientY };
  });

  window.addEventListener('mouseup', () => { isAnsysDragging = false; });

  dom.addEventListener('mousemove', (e) => {
    if (!isAnsysDragging || !ansysPointsObject) {
      // Raycasting for inspection
      inspectAnsysNode(e);
      return;
    }
    const dx = e.clientX - ansysPrevMouse.x;
    const dy = e.clientY - ansysPrevMouse.y;

    if (e.buttons === 1) {
      ansysPointsObject.rotation.y += dx * 0.01;
      ansysPointsObject.rotation.x += dy * 0.01;
    } else if (e.buttons === 2) {
      ansysCamera.position.x -= dx * 0.0005 * ansysBounds.radius;
      ansysCamera.position.y += dy * 0.0005 * ansysBounds.radius;
    }
    ansysPrevMouse = { x: e.clientX, y: e.clientY };
  });

  dom.addEventListener('wheel', (e) => {
    e.preventDefault();
    const factor = e.deltaY > 0 ? 1.08 : 0.92;
    ansysCamera.position.multiplyScalar(factor);
  }, { passive: false });

  dom.addEventListener('contextmenu', (e) => e.preventDefault());

  window.addEventListener('resize', onAnsysWindowResize);

  function animate() {
    requestAnimationFrame(animate);
    ansysRenderer.render(ansysScene, ansysCamera);
  }
  animate();
}

function onAnsysWindowResize() {
  const container = document.getElementById('ansys-canvas-container');
  if (!container || !ansysRenderer || !ansysCamera) return;
  const width = container.clientWidth;
  const height = container.clientHeight;
  ansysCamera.aspect = width / height;
  ansysCamera.updateProjectionMatrix();
  ansysRenderer.setSize(width, height);
}

function resetAnsysCamera() {
  if (!ansysCamera || !ansysBounds) return;
  const r = Math.max(ansysBounds.radius, 0.05);
  ansysCamera.position.set(r * 1.5, r * 1.2, r * 2.0);
  ansysCamera.lookAt(new THREE.Vector3(0, 0, 0));
  if (ansysPointsObject) {
    ansysPointsObject.rotation.set(0, 0, 0);
  }
}

function tempToColor(t, minT, maxT) {
  const norm = Math.max(0, Math.min(1, (t - minT) / Math.max(1e-5, (maxT - minT))));
  // Rainbow: blue -> cyan -> green -> yellow -> red
  let r = 0, g = 0, b = 0;
  if (norm < 0.25) {
    r = 0; g = norm * 4; b = 1;
  } else if (norm < 0.5) {
    r = 0; g = 1; b = 1 - (norm - 0.25) * 4;
  } else if (norm < 0.75) {
    r = (norm - 0.5) * 4; g = 1; b = 0;
  } else {
    r = 1; g = 1 - (norm - 0.75) * 4; b = 0;
  }
  return [r, g, b];
}

function renderAnsysCloud(data) {
  const errBanner = document.getElementById('ansys-error-banner');
  const overlayStatus = document.getElementById('ansys-overlay-status');

  if (!data.valid) {
    errBanner.style.display = 'block';
    errBanner.textContent = `ANSYS CSV Error: ${data.error}`;
    overlayStatus.textContent = `Error: ${data.error}`;
    return;
  }

  errBanner.style.display = 'none';
  overlayStatus.textContent = `Loaded ${data.nodeCount} nodes. Min: ${data.temperatureRange.min.toFixed(1)}°C, Max: ${data.temperatureRange.max.toFixed(1)}°C`;

  document.getElementById('ansys-meta-provenance').textContent = data.provenance;
  document.getElementById('ansys-meta-nodes').textContent = data.nodeCount.toLocaleString();
  document.getElementById('ansys-meta-tmin').textContent = `${data.temperatureRange.min.toFixed(2)} °C`;
  document.getElementById('ansys-meta-tmax').textContent = `${data.temperatureRange.max.toFixed(2)} °C`;
  document.getElementById('ansys-meta-tavg').textContent = `${data.temperatureRange.avg.toFixed(2)} °C`;
  document.getElementById('ansys-meta-extents').textContent =
    `X:[${data.bounds.min[0].toFixed(3)}, ${data.bounds.max[0].toFixed(3)}], Y:[${data.bounds.min[1].toFixed(3)}, ${data.bounds.max[1].toFixed(3)}], Z:[${data.bounds.min[2].toFixed(3)}, ${data.bounds.max[2].toFixed(3)}] m`;

  document.getElementById('legend-min-t').textContent = `${data.temperatureRange.min.toFixed(1)}°C`;
  document.getElementById('legend-mid-t').textContent = `${((data.temperatureRange.min + data.temperatureRange.max)/2).toFixed(1)}°C`;
  document.getElementById('legend-max-t').textContent = `${data.temperatureRange.max.toFixed(1)}°C`;

  currentAnsysNodes = data.nodes;

  const dx = data.bounds.max[0] - data.bounds.min[0];
  const dy = data.bounds.max[1] - data.bounds.min[1];
  const dz = data.bounds.max[2] - data.bounds.min[2];
  ansysBounds = {
    center: data.bounds.center,
    radius: 0.5 * Math.sqrt(dx*dx + dy*dy + dz*dz)
  };

  if (ansysPointsObject) {
    ansysScene.remove(ansysPointsObject);
  }

  const geometry = new THREE.BufferGeometry();
  const positions = [];
  const colors = [];

  const cx = data.bounds.center[0];
  const cy = data.bounds.center[1];
  const cz = data.bounds.center[2];
  const minT = data.temperatureRange.min;
  const maxT = data.temperatureRange.max;

  for (const n of data.nodes) {
    positions.push(n.x - cx, n.y - cy, n.z - cz);
    const rgb = tempToColor(n.t, minT, maxT);
    colors.push(rgb[0], rgb[1], rgb[2]);
  }

  geometry.setAttribute('position', new THREE.Float32BufferAttribute(positions, 3));
  geometry.setAttribute('color', new THREE.Float32BufferAttribute(colors, 3));

  const material = new THREE.PointsMaterial({
    size: 0.004,
    vertexColors: true,
    sizeAttenuation: true
  });

  ansysPointsObject = new THREE.Points(geometry, material);
  ansysScene.add(ansysPointsObject);

  resetAnsysCamera();
}

function inspectAnsysNode(e) {
  if (!ansysPointsObject || currentAnsysNodes.length === 0 || !ansysCamera) return;
  const rect = ansysRenderer.domElement.getBoundingClientRect();
  const mouse = new THREE.Vector2(
    ((e.clientX - rect.left) / rect.width) * 2 - 1,
    -((e.clientY - rect.top) / rect.height) * 2 + 1
  );

  const raycaster = new THREE.Raycaster();
  raycaster.params.Points.threshold = 0.003;
  raycaster.setFromCamera(mouse, ansysCamera);

  const intersects = raycaster.intersectObject(ansysPointsObject);
  const infoDiv = document.getElementById('ansys-inspected-node');
  if (intersects.length > 0) {
    const idx = intersects[0].index;
    const node = currentAnsysNodes[idx];
    if (node) {
      infoDiv.innerHTML = `
        <strong>Node ID:</strong> ${node.id}<br>
        <strong>Coordinates:</strong> X = ${node.x.toFixed(4)} m, Y = ${node.y.toFixed(4)} m, Z = ${node.z.toFixed(4)} m<br>
        <strong>Temperature:</strong> <span style="color: var(--accent-orange); font-size: 1.05rem; font-weight: bold;">${node.t.toFixed(2)} °C</span>
      `;
    }
  }
}

async function loadAnsysPreset() {
  const preset = document.getElementById('ansys-preset-select').value;
  try {
    const res = await fetch(preset);
    const content = await res.text();
    const parseRes = await fetch('/api/ansys/parse', {
      method: 'POST',
      body: content
    });
    const json = await parseRes.json();
    renderAnsysCloud(json);
  } catch (err) {
    const errBanner = document.getElementById('ansys-error-banner');
    errBanner.style.display = 'block';
    errBanner.textContent = `Failed to load ANSYS CSV: ${err.message}`;
  }
}

function handleAnsysFileUpload(e) {
  const file = e.target.files[0];
  if (!file) return;
  const reader = new FileReader();
  reader.onload = async (evt) => {
    try {
      const parseRes = await fetch('/api/ansys/parse', {
        method: 'POST',
        body: evt.target.result
      });
      const json = await parseRes.json();
      renderAnsysCloud(json);
    } catch (err) {
      alert(`Parse error: ${err.message}`);
    }
  };
  reader.readAsText(file);
}

// =============================================================================
// 3. THERMAL 1D FEA ROD SIMULATION
// =============================================================================
async function runFeaSolve() {
  const L = document.getElementById('fea-param-L').value;
  const A = document.getElementById('fea-param-A').value;
  const k = document.getElementById('fea-param-k').value;
  const q = document.getElementById('fea-param-q').value;
  const T0 = document.getElementById('fea-param-T0').value;
  const TL = document.getElementById('fea-param-TL').value;
  const N = document.getElementById('fea-param-N').value;

  try {
    const res = await fetch(`/api/fea/solve?L=${L}&A=${A}&k=${k}&q=${q}&T0=${T0}&TL=${TL}&N=${N}`);
    const data = await res.json();
    if (!data.success) {
      alert("FEA error: " + data.errorMessage);
      return;
    }
    renderFeaCanvas(data);
  } catch (err) {
    console.error("FEA solve failed:", err);
  }
}

function renderFeaCanvas(sol) {
  const canvas = document.getElementById('fea-canvas');
  if (!canvas) return;
  const ctx = canvas.getContext('2d');
  const w = canvas.width;
  const h = canvas.height;

  ctx.clearRect(0, 0, w, h);

  // Background grid
  ctx.fillStyle = '#0f172a';
  ctx.fillRect(0, 0, w, h);

  const padLeft = 60, padRight = 30, padTop = 30, padBottom = 50;
  const plotW = w - padLeft - padRight;
  const plotH = h - padTop - padBottom;

  // Find min/max T
  let minT = 1e9, maxT = -1e9;
  for (const n of sol.nodes) {
    if (n.t_exact < minT) minT = n.t_exact;
    if (n.t_exact > maxT) maxT = n.t_exact;
    if (n.t_fea < minT) minT = n.t_fea;
    if (n.t_fea > maxT) maxT = n.t_fea;
  }
  minT = Math.floor(minT / 10) * 10 - 10;
  maxT = Math.ceil(maxT / 10) * 10 + 10;

  const toX = (x) => padLeft + (x / sol.nodes[sol.nodes.length - 1].x) * plotW;
  const toY = (t) => padTop + plotH - ((t - minT) / (maxT - minT)) * plotH;

  // Grid lines
  ctx.strokeStyle = '#1e293b';
  ctx.lineWidth = 1;
  ctx.fillStyle = '#64748b';
  ctx.font = '11px sans-serif';

  for (let t = minT; t <= maxT; t += 20) {
    const y = toY(t);
    ctx.beginPath();
    ctx.moveTo(padLeft, y);
    ctx.lineTo(w - padRight, y);
    ctx.stroke();
    ctx.fillText(`${t}°C`, 15, y + 4);
  }

  for (let x = 0; x <= 1.0; x += 0.2) {
    const px = toX(x);
    ctx.beginPath();
    ctx.moveTo(px, padTop);
    ctx.lineTo(px, padTop + plotH);
    ctx.stroke();
    ctx.fillText(`${x.toFixed(1)}m`, px - 12, padTop + plotH + 20);
  }

  // Draw Exact Analytical Curve (smooth parabola)
  ctx.strokeStyle = '#f8fafc';
  ctx.lineWidth = 2.5;
  ctx.beginPath();
  const steps = 100;
  for (let i = 0; i <= steps; ++i) {
    const x = i / steps;
    // T_exact(x) = T0 + (TL-T0)*x/L + [q/(2k)]*x*(L-x)
    const T_exact = sol.nodes[0].t_exact; // use exact from first node
    const T0 = sol.nodes[0].t_exact;
    const TL = sol.nodes[sol.nodes.length - 1].t_exact;
    const t = T0 + (TL - T0) * x + (10000.0 / (2.0 * 10.0)) * x * (1.0 - x);
    const px = toX(x);
    const py = toY(t);
    if (i === 0) ctx.moveTo(px, py);
    else ctx.lineTo(px, py);
  }
  ctx.stroke();

  // Draw FEA Piecewise Linear Curve
  ctx.strokeStyle = '#38bdf8';
  ctx.lineWidth = 2;
  ctx.setLineDash([4, 4]);
  ctx.beginPath();
  for (let i = 0; i < sol.nodes.length; ++i) {
    const px = toX(sol.nodes[i].x);
    const py = toY(sol.nodes[i].t_fea);
    if (i === 0) ctx.moveTo(px, py);
    else ctx.lineTo(px, py);
  }
  ctx.stroke();
  ctx.setLineDash([]);

  // Draw Nodes
  ctx.fillStyle = '#38bdf8';
  for (const n of sol.nodes) {
    const px = toX(n.x);
    const py = toY(n.t_fea);
    ctx.beginPath();
    ctx.arc(px, py, 4, 0, Math.PI * 2);
    ctx.fill();
  }

  // Draw Midpoint Errors
  ctx.fillStyle = '#f59e0b';
  for (const m of sol.midpoints) {
    const px = toX(m.x);
    const py = toY(m.t_fea);
    ctx.beginPath();
    ctx.arc(px, py, 3, 0, Math.PI * 2);
    ctx.fill();
  }

  // Legend
  ctx.fillStyle = '#f8fafc';
  ctx.font = '11px sans-serif';
  ctx.fillText('— Exact Analytical T_exact(x)', padLeft + 15, padTop + 20);

  ctx.fillStyle = '#38bdf8';
  ctx.fillText(`-- FEA Linear (N=${sol.N})`, padLeft + 180, padTop + 20);

  ctx.fillStyle = '#f59e0b';
  ctx.fillText(`• Element Midpoints (Max Err = ${sol.maxMidpointError.toFixed(6)} °C)`, padLeft + 310, padTop + 20);
}

async function runFeaConvergence() {
  try {
    const res = await fetch('/api/fea/convergence');
    const data = await res.json();
    const tbody = document.getElementById('fea-convergence-body');
    tbody.innerHTML = '';

    for (const s of data.studies) {
      const tr = document.createElement('tr');
      tr.innerHTML = `
        <td style="font-weight: 700; color: var(--accent-cyan);">${s.N}</td>
        <td>${s.h.toFixed(4)}</td>
        <td>${s.maxNodeError.toExponential(2)}</td>
        <td style="font-weight: 700; color: #f59e0b;">${s.maxMidpointError.toFixed(6)}</td>
        <td>${s.theoreticalMidpointError.toFixed(6)}</td>
        <td style="font-weight: 700; color: var(--accent-green);">${s.observedRate > 0 ? s.observedRate.toFixed(4) : '—'}</td>
      `;
      tbody.appendChild(tr);
    }
  } catch (err) {
    console.error("Convergence study failed:", err);
  }
}

function exportFeaCsv() {
  window.location.href = '/api/export/csv?type=convergence';
}

// =============================================================================
// 4. MULTICORE PERFORMANCE BENCHMARK
// =============================================================================
let lastBenchmarkData = null;

async function runMulticoreBenchmark() {
  const cases = document.getElementById('mc-param-cases').value;
  const meshN = document.getElementById('mc-param-mesh').value;
  const seed = document.getElementById('mc-param-seed').value;
  const runBtn = document.getElementById('mc-run-btn');
  const progContainer = document.getElementById('mc-progress-container');
  const progBar = document.getElementById('mc-progress-bar');

  runBtn.disabled = true;
  runBtn.textContent = '⏳ Benchmarking...';
  progContainer.style.display = 'block';
  progBar.style.width = '30%';

  try {
    progBar.style.width = '60%';
    const res = await fetch(`/api/multicore/benchmark?cases=${cases}&meshN=${meshN}&seed=${seed}`, {
      method: 'POST'
    });
    progBar.style.width = '90%';
    const data = await res.json();
    lastBenchmarkData = data;
    progBar.style.width = '100%';

    renderBenchmarkResults(data);
    renderMulticoreChart();
  } catch (err) {
    alert("Benchmark failed: " + err.message);
  } finally {
    runBtn.disabled = false;
    runBtn.textContent = '▶ Run Benchmark';
    setTimeout(() => { progContainer.style.display = 'none'; }, 800);
  }
}

function renderBenchmarkResults(data) {
  document.getElementById('mc-sys-cpu').textContent = data.system.cpu;
  document.getElementById('mc-sys-cores').textContent = `${data.system.physicalCores} Physical / ${data.system.logicalCores} Logical Cores`;
  document.getElementById('mc-sys-ram').textContent = `${data.system.ramGB.toFixed(2)} GB Usable`;
  document.getElementById('mc-sys-compiler').textContent = data.system.compiler;
  document.getElementById('mc-sys-flags').textContent = data.system.flags;
  document.getElementById('mc-sys-warmup').textContent = `${data.workload.warmupMs.toFixed(2)} ms (Excluded from timings)`;
  document.getElementById('mc-sys-launch').textContent = `${data.workload.launchToReadyMs.toFixed(2)} ms`;

  const badge = document.getElementById('mc-agreement-badge');
  const badgeText = document.getElementById('mc-agreement-text');
  if (data.overallAgreementVerified) {
    badge.className = 'status-pill';
    badgeText.textContent = 'Agreement Verified: Max Diff <= 1e-8 °C';
  } else {
    badge.className = 'status-pill error';
    badgeText.textContent = 'Agreement Check Failed';
  }

  const tbody = document.getElementById('mc-results-body');
  tbody.innerHTML = '';

  for (const r of data.results) {
    const tr = document.createElement('tr');
    const repsStr = r.repetitionsMs.map(t => t.toFixed(2)).join(', ');
    tr.innerHTML = `
      <td style="font-weight: 700; color: var(--accent-cyan);">${r.workers} Worker${r.workers > 1 ? 's' : ''}</td>
      <td style="font-size: 0.78rem; color: var(--text-secondary);">${repsStr}</td>
      <td style="font-weight: 700; font-family: var(--mono-family);">${r.medianMs.toFixed(2)}</td>
      <td style="font-weight: 700; color: var(--accent-green);">${r.speedup.toFixed(2)}x</td>
      <td>${(r.efficiency * 100).toFixed(1)}%</td>
      <td><span class="badge ${r.passesAgreement ? 'green' : 'red'}">${r.passesAgreement ? 'PASS' : 'FAIL'}</span></td>
    `;
    tbody.appendChild(tr);
  }
}

function renderMulticoreChart() {
  const canvas = document.getElementById('mc-chart-canvas');
  if (!canvas || !lastBenchmarkData) return;
  const ctx = canvas.getContext('2d');
  const w = canvas.width;
  const h = canvas.height;

  ctx.clearRect(0, 0, w, h);
  ctx.fillStyle = '#0f172a';
  ctx.fillRect(0, 0, w, h);

  const padLeft = 60, padRight = 60, padTop = 30, padBottom = 40;
  const plotW = w - padLeft - padRight;
  const plotH = h - padTop - padBottom;

  const results = lastBenchmarkData.results;
  const maxWorkers = 4;
  const maxSpeedup = 4.2;

  const toX = (workers) => padLeft + ((workers - 1) / (maxWorkers - 1)) * plotW;
  const toY = (sp) => padTop + plotH - (sp / maxSpeedup) * plotH;

  // Grid
  ctx.strokeStyle = '#1e293b';
  ctx.lineWidth = 1;
  ctx.fillStyle = '#64748b';
  ctx.font = '11px sans-serif';

  for (let s = 1; s <= 4; s += 1) {
    const y = toY(s);
    ctx.beginPath();
    ctx.moveTo(padLeft, y);
    ctx.lineTo(w - padRight, y);
    ctx.stroke();
    ctx.fillText(`${s}.0x`, 25, y + 4);
  }

  // Ideal linear speedup line
  ctx.strokeStyle = '#475569';
  ctx.lineWidth = 1.5;
  ctx.setLineDash([4, 4]);
  ctx.beginPath();
  ctx.moveTo(toX(1), toY(1));
  ctx.lineTo(toX(4), toY(4));
  ctx.stroke();
  ctx.setLineDash([]);

  // Observed speedup line
  ctx.strokeStyle = '#38bdf8';
  ctx.lineWidth = 2.5;
  ctx.beginPath();
  for (let i = 0; i < results.length; ++i) {
    const px = toX(results[i].workers);
    const py = toY(results[i].speedup);
    if (i === 0) ctx.moveTo(px, py);
    else ctx.lineTo(px, py);
  }
  ctx.stroke();

  // Draw points
  for (const r of results) {
    const px = toX(r.workers);
    const py = toY(r.speedup);
    ctx.fillStyle = '#38bdf8';
    ctx.beginPath();
    ctx.arc(px, py, 6, 0, Math.PI * 2);
    ctx.fill();

    ctx.fillStyle = '#f8fafc';
    ctx.font = 'bold 11px sans-serif';
    ctx.fillText(`${r.speedup.toFixed(2)}x`, px - 12, py - 12);
    ctx.fillText(`${r.workers}p`, px - 8, padTop + plotH + 20);
  }

  // Legend
  ctx.fillStyle = '#94a3b8';
  ctx.fillText('-- Ideal Linear Speedup', padLeft + 15, padTop + 15);
  ctx.fillStyle = '#38bdf8';
  ctx.fillText('— Observed Speedup T1/Tp', padLeft + 180, padTop + 15);
}

function exportMulticoreCsv() {
  window.location.href = '/api/export/csv?type=multicore';
}

// =============================================================================
// 5. EMBEDDED HARDWARE DEVICE MANAGEMENT
// =============================================================================
async function pollEmbeddedTelemetry() {
  try {
    const res = await fetch('/api/devices');
    const data = await res.json();
    for (const dev of data.devices) {
      updateDeviceUI(dev);
    }
  } catch (err) {
    console.warn("Telemetry polling failed:", err);
  }
}

function updateDeviceUI(dev) {
  if (dev.id === 'mcu-stm32h7') {
    const badge = document.getElementById('badge-mcu');
    badge.textContent = dev.state;
    badge.className = `badge ${dev.state === 'CONNECTED' ? 'green' : dev.state === 'ERROR_TIMEOUT' ? 'red' : 'orange'}`;
    if (dev.state === 'CONNECTED' && dev.telemetry.telemetry) {
      const t = dev.telemetry.telemetry;
      document.getElementById('mcu-temp').textContent = `${t.coreTemperatureC.toFixed(1)} °C`;
      document.getElementById('mcu-vdd').textContent = `${t.supplyVoltageV.toFixed(2)} V`;
      document.getElementById('mcu-load').textContent = `${t.cpuUtilizationPercent.toFixed(1)} %`;
      document.getElementById('mcu-packets').textContent = t.packetCounter;
    } else {
      document.getElementById('mcu-temp').textContent = '—';
      document.getElementById('mcu-vdd').textContent = '—';
      document.getElementById('mcu-load').textContent = '—';
      document.getElementById('mcu-packets').textContent = '—';
    }
  } else if (dev.id === 'fpga-artix7') {
    const badge = document.getElementById('badge-fpga');
    badge.textContent = dev.state;
    badge.className = `badge ${dev.state === 'CONNECTED' ? 'green' : dev.state === 'ERROR_TIMEOUT' ? 'red' : 'orange'}`;
    if (dev.state === 'CONNECTED' && dev.telemetry.telemetry) {
      const t = dev.telemetry.telemetry;
      document.getElementById('fpga-temp').textContent = `${t.dieTemperatureC.toFixed(1)} °C`;
      document.getElementById('fpga-vint').textContent = `${t.vccIntV.toFixed(3)} V`;
      document.getElementById('fpga-lut').textContent = `${t.lutUtilizationPercent.toFixed(1)} %`;
      document.getElementById('fpga-frames').textContent = t.processedFrames;
    } else {
      document.getElementById('fpga-temp').textContent = '—';
      document.getElementById('fpga-vint').textContent = '—';
      document.getElementById('fpga-lut').textContent = '—';
      document.getElementById('fpga-frames').textContent = '—';
    }
  } else if (dev.id === 'jtag-probe') {
    const badge = document.getElementById('badge-jtag');
    badge.textContent = dev.state;
    badge.className = `badge ${dev.state === 'CONNECTED' ? 'green' : dev.state === 'ERROR_TIMEOUT' ? 'red' : 'orange'}`;
    if (dev.state === 'CONNECTED' && dev.telemetry.telemetry) {
      const t = dev.telemetry.telemetry;
      document.getElementById('jtag-vref').textContent = `${t.targetVrefSensedV.toFixed(3)} V`;
      document.getElementById('jtag-tck').textContent = `${(t.tckSpeedkHz / 1000).toFixed(1)} MHz`;
      document.getElementById('jtag-tap').textContent = t.tapState;
      document.getElementById('jtag-chain').textContent = `${t.scanChainDevicesDetected} TAPs`;
    } else {
      document.getElementById('jtag-vref').textContent = '—';
      document.getElementById('jtag-tck').textContent = '—';
      document.getElementById('jtag-tap').textContent = '—';
      document.getElementById('jtag-chain').textContent = '—';
    }
  }
}

async function toggleDevice(id) {
  try {
    const devRes = await fetch('/api/devices');
    const devData = await devRes.json();
    const dev = devData.devices.find(d => d.id === id);
    const endpoint = (dev && dev.state === 'CONNECTED') ? '/api/devices/disconnect' : '/api/devices/connect';
    await fetch(`${endpoint}?id=${id}`, { method: 'POST' });
    pollEmbeddedTelemetry();
  } catch (err) {
    alert("Device toggle error: " + err.message);
  }
}

async function injectFault(id) {
  try {
    await fetch(`/api/devices/fault?id=${id}`, { method: 'POST' });
    pollEmbeddedTelemetry();
  } catch (err) {
    alert("Fault injection error: " + err.message);
  }
}

async function recoverDevice(id) {
  try {
    await fetch(`/api/devices/recover?id=${id}`, { method: 'POST' });
    pollEmbeddedTelemetry();
  } catch (err) {
    alert("Device recovery error: " + err.message);
  }
}

// =============================================================================
// 6. BACKPROPAGATION STUDY
// =============================================================================
async function loadBackpropStudy() {
  try {
    const res = await fetch('/api/backprop/study');
    const data = await res.json();
    renderBackpropCanvas(data.history);
  } catch (err) {
    console.error("Backprop study failed:", err);
  }
}

function renderBackpropCanvas(history) {
  const canvas = document.getElementById('backprop-canvas');
  if (!canvas || !history) return;
  const ctx = canvas.getContext('2d');
  const w = canvas.width;
  const h = canvas.height;

  ctx.clearRect(0, 0, w, h);
  ctx.fillStyle = '#0f172a';
  ctx.fillRect(0, 0, w, h);

  const padLeft = 60, padRight = 30, padTop = 30, padBottom = 40;
  const plotW = w - padLeft - padRight;
  const plotH = h - padTop - padBottom;

  const maxSteps = history.length - 1;
  const maxLoss = 1.25;

  const toX = (step) => padLeft + (step / maxSteps) * plotW;
  const toY = (loss) => padTop + plotH - (loss / maxLoss) * plotH;

  // Grid
  ctx.strokeStyle = '#1e293b';
  ctx.lineWidth = 1;
  ctx.fillStyle = '#64748b';
  ctx.font = '11px sans-serif';

  for (let l = 0.0; l <= 1.2; l += 0.2) {
    const y = toY(l);
    ctx.beginPath();
    ctx.moveTo(padLeft, y);
    ctx.lineTo(w - padRight, y);
    ctx.stroke();
    ctx.fillText(l.toFixed(1), 30, y + 4);
  }

  for (let s = 0; s <= 100; s += 20) {
    const x = toX(s);
    ctx.beginPath();
    ctx.moveTo(x, padTop);
    ctx.lineTo(x, padTop + plotH);
    ctx.stroke();
    ctx.fillText(s, x - 6, padTop + plotH + 20);
  }

  // Draw Loss curve
  ctx.strokeStyle = '#f59e0b';
  ctx.lineWidth = 2.5;
  ctx.beginPath();
  for (let i = 0; i < history.length; ++i) {
    const px = toX(history[i].step);
    const py = toY(history[i].loss);
    if (i === 0) ctx.moveTo(px, py);
    else ctx.lineTo(px, py);
  }
  ctx.stroke();

  // Draw prediction evolution curve (scaled)
  ctx.strokeStyle = '#a855f7';
  ctx.lineWidth = 1.8;
  ctx.setLineDash([3, 3]);
  ctx.beginPath();
  for (let i = 0; i < history.length; ++i) {
    // scale y_hat (0 to 2) to fit loss axis
    const px = toX(history[i].step);
    const py = toY(history[i].y_hat * 0.5); // scale 2.0 to 1.0
    if (i === 0) ctx.moveTo(px, py);
    else ctx.lineTo(px, py);
  }
  ctx.stroke();
  ctx.setLineDash([]);

  // Legend
  ctx.fillStyle = '#f59e0b';
  ctx.fillText('— Training Loss J = 0.5*(y_hat - y)^2', padLeft + 15, padTop + 15);
  ctx.fillStyle = '#a855f7';
  ctx.fillText('-- Prediction y_hat (scaled to y=2.0)', padLeft + 250, padTop + 15);
}

function exportBackpropCsv() {
  window.location.href = '/api/export/csv?type=backprop';
}

// =============================================================================
// GLOBAL INITIALIZATION
// =============================================================================
function refreshAllViews() {
  loadStlPreset();
  loadAnsysPreset();
  runFeaSolve();
  runFeaConvergence();
  runMulticoreBenchmark();
  pollEmbeddedTelemetry();
  loadBackpropStudy();
}

window.addEventListener('DOMContentLoaded', () => {
  initStlViewer();
  initAnsysViewer();
  loadStlPreset();
  runFeaSolve();
  runFeaConvergence();
  runMulticoreBenchmark();
  pollEmbeddedTelemetry();
  setInterval(pollEmbeddedTelemetry, 2000);
});
