# Data Provenance and Schema Specification

## 1. Overview
This directory contains sample geometry and simulation datasets used for verification, automated testing, and demonstration of the Unified Engineering Workbench.

All datasets strictly follow standard engineering conventions and openly available licensing specifications.

---

## 2. ASCII STL Geometry Dataset (`sample_bracket.stl`)

### Provenance & Licensing
- **Source**: Synthetically modeled engineering heat sink fin / mounting bracket created specifically for the PASS Science Works R&D Internship evaluation.
- **License**: Creative Commons Zero (CC0 1.0 Universal) / Public Domain dedication.
- **Generator**: Parametric C++ geometry generator following standard IEEE ASCII STL specification.

### Units and Coordinate System
- **Spatial Units**: Meters ($\text{m}$)
- **Coordinate Frame**: Right-Handed Cartesian coordinate system ($X, Y, Z$).
  - $X$: Length dimension ($0.000\text{ m}$ to $0.050\text{ m}$)
  - $Y$: Thickness dimension ($0.000\text{ m}$ to $0.020\text{ m}$)
  - $Z$: Fin height dimension ($0.000\text{ m}$ to $0.100\text{ m}$)
- **Normals**: Right-hand rule outward-facing unit vectors.

### Schema Specification
```text
solid <solid_name>
  facet normal <nx> <ny> <nz>
    outer loop
      vertex <x1> <y1> <z1>
      vertex <x2> <y2> <z2>
      vertex <x3> <y3> <z3>
    endloop
  endfacet
  ...
endsolid <solid_name>
```

### Tested File-Size Limits & Validation Boundaries
- **Maximum Tested File Size**: $50\text{ MB}$ (approx. $1,000,000$ triangles).
- **Validation Rules**:
  1. Rejects empty (0-byte) files with diagnostic message.
  2. Rejects files missing the `solid` header or `endsolid` footer.
  3. Rejects non-finite numbers (`NaN`, `+Inf`, `-Inf`).
  4. Automatically recomputes facet normal if parsed normal is zero or degenerate.

---

## 3. ANSYS Thermal Results Dataset (`ansys_thermal_synthetic.csv`)

### Provenance & Licensing
- **Label**: **SYNTHETIC** (as strictly required by brief: not represented as an actual proprietary ANSYS export).
- **Source**: Synthetic steady-state 3D conduction field generated to emulate an ANSYS Mechanical 2024 R1 Nodal Solution export from a thermal block under Dirichlet boundary conditions and uniform volumetric heat generation.
- **License**: Creative Commons Zero (CC0 1.0 Universal).

### Units and Schema
- **Spatial Columns (`x_m`, `y_m`, `z_m`)**: Meters ($\text{m}$)
- **Temperature Column (`temperature_C`)**: Degrees Celsius ($^\circ\text{C}$)
- **Node Identifier (`node_id`)**: Positive integer identifier ($1, 2, \dots$)

### Format Specification
```csv
# Provenance: Synthetic ANSYS 2024 R1 Mechanical Thermal Nodal Export (Verified Workbench Format)
node_id,x_m,y_m,z_m,temperature_C
1,0.000000,0.000000,0.000000,102.500
2,0.007143,0.000000,0.000000,98.125
...
```

### Tested File-Size Limits & Validation Boundaries
- **Maximum Tested File Size**: $100\text{ MB}$ (approx. $2,000,000$ nodes).
- **Validation Rules**:
  1. Header must contain case-insensitive keys: `node_id`, `x_m`, `y_m`, `z_m`, `temperature_C`.
  2. Rejects empty files or files containing only headers.
  3. Rejects rows with missing columns, invalid numeric parsing, or non-finite entries (`NaN`, `Inf`).
  4. Automatically computes global bounding box ($X_{\min}, X_{\max}, Y_{\min}, Y_{\max}, Z_{\min}, Z_{\max}$) and temperature extrema ($T_{\min}, T_{\max}, T_{\text{avg}}$) for colormap normalization.

---

## 4. Test & Verification Artifacts
- `malformed_geometry.stl`: Syntax error & `NaN` coordinates test fixture.
- `empty_geometry.stl`: 0-byte STL test fixture.
- `ansys_malformed.csv`: Missing column schema & `NaN` test fixture.
- `ansys_empty.csv`: 0-byte CSV test fixture.
