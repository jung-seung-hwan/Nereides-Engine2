"""단계 4-1 시험 모델을 만든다. 실행: python tools/make_test_model.py

결과: assets/test/arrow.gltf (JSON 하나. 정점·인덱스 등 이진 값은 base64로 안에 넣는다)

모양: 화살표. 몸통은 원점에서 glTF의 +Z(앞)로 뻗고, 끝에 화살촉이 있다.
      몸통 한쪽(glTF의 -X)에만 작은 날개가 붙어 있어 좌우가 구분된다.
      모델이 +Z를 보고 서 있을 때 glTF(오른손)에서 -X는 모델의 오른손 쪽이다. 엔진(왼손)은 X를 뒤집어 읽으므로
      날개는 엔진의 +X, 역시 모델의 오른손 쪽에 온다. 뒤집기를 빠뜨리면 날개가 반대쪽에 보인다.
뼈:   2개. root(원점)와 tip(z=1, 화살촉 쪽). 정점은 z<1이면 root, z>=1이면 tip을 따른다.
클립: "bend" 1초. tip이 X축을 중심으로 0도 → 90도 → 0도로 꺾인다. 4-1은 개수만 읽고 4-2가 재생한다.
      "swing" 1초. tip이 Y축을 중심으로 0도 → 90도 → 0도로 돌고, root가 x로 0 → 0.2 → 0 움직인다(4-2).
      "swing"은 X 뒤집기에 대칭이 아니라서, 로더가 뼈·클립의 좌표 변환을 빠뜨리면 시험이 잡아낸다.
      "bend"만으로는 못 잡는다. X축 회전과 Z 이동은 X를 뒤집어도 같기 때문이다.
받침: 뼈가 없는 둘째 메시. 노드에 이동 (0, -0.3, 0.2)와 크기 (2, 1, 1)가 있어서, 로더가 노드 변환을 정점에 적용하는지 본다.
glTF 규칙: 오른손 좌표계, 앞면은 반시계(CCW). 엔진이 읽을 때 X를 뒤집고 앞면 순서를 바꾼다.
"""
import base64, json, math, struct, os

vertices = []   # (position, normal)
indices = []

def quad(a, b, c, d, n):
    # 네 점 a-b-c-d가 밖에서 볼 때 반시계 방향이 되도록 부른다. 면마다 정점 4개를 둔다(법선이 다르므로).
    base = len(vertices)
    for p in (a, b, c, d):
        vertices.append((p, n))
    indices.extend([base, base + 1, base + 2, base, base + 2, base + 3])

def box(x0, x1, y0, y1, z0, z1):
    # 축에 나란한 상자. 각 면은 바깥에서 볼 때 반시계 방향이다(오른손 좌표계).
    quad((x0, y0, z1), (x1, y0, z1), (x1, y1, z1), (x0, y1, z1), (0, 0, 1))    # +Z
    quad((x1, y0, z0), (x0, y0, z0), (x0, y1, z0), (x1, y1, z0), (0, 0, -1))   # -Z
    quad((x1, y0, z1), (x1, y0, z0), (x1, y1, z0), (x1, y1, z1), (1, 0, 0))    # +X
    quad((x0, y0, z0), (x0, y0, z1), (x0, y1, z1), (x0, y1, z0), (-1, 0, 0))   # -X
    quad((x0, y1, z1), (x1, y1, z1), (x1, y1, z0), (x0, y1, z0), (0, 1, 0))    # +Y
    quad((x0, y0, z0), (x1, y0, z0), (x1, y0, z1), (x0, y0, z1), (0, -1, 0))   # -Y

box(-0.1, 0.1, -0.1, 0.1, 0.0, 1.0)    # 몸통: z 0~1
box(-0.25, 0.25, -0.05, 0.05, 1.0, 1.4) # 화살촉: z 1~1.4, 옆으로 넓다
box(-0.4, -0.1, -0.05, 0.05, 0.3, 0.5)  # 날개: x -0.4~-0.1 (glTF의 -X 쪽에만 있다)

positions = [v[0] for v in vertices]
normals = [v[1] for v in vertices]
joints = [(0, 0, 0, 0) if p[2] < 1.0 else (1, 0, 0, 0) for p in positions]
weights = [(1.0, 0.0, 0.0, 0.0) for _ in positions]

arrow_indices = list(indices)

# 받침: 뼈 없는 둘째 메시. 정점은 원점 둘레의 작은 상자이고, 위치는 노드의 이동·크기가 정한다.
vertices.clear(); indices.clear()
box(-0.15, 0.15, -0.05, 0.05, -0.15, 0.15)
base_positions = [v[0] for v in vertices]
base_normals = [v[1] for v in vertices]
base_indices = list(indices)

# 클립: tip 뼈의 회전. 시각 0, 0.5, 1초. X축 회전 0도, 90도, 0도(사원수 x,y,z,w).
times = [0.0, 0.5, 1.0]
def quat_x(deg):
    h = math.radians(deg) / 2
    return (math.sin(h), 0.0, 0.0, math.cos(h))
rotations = [quat_x(0), quat_x(90), quat_x(0)]

# 둘째 클립 "swing": tip이 Y축으로 0도 → 90도 → 0도, root가 x로 0 → 0.2 → 0.
def quat_y(deg):
    h = math.radians(deg) / 2
    return (0.0, math.sin(h), 0.0, math.cos(h))
swing_rotations = [quat_y(0), quat_y(90), quat_y(0)]
swing_translations = [(0.0, 0.0, 0.0), (0.2, 0.0, 0.0), (0.0, 0.0, 0.0)]

# tip의 역바인드 행렬: tip은 z=1에 있으므로 (0,0,-1)만큼 옮기는 행렬. root는 단위 행렬. glTF는 열 우선이다.
def translation_matrix(x, y, z):
    return [1,0,0,0, 0,1,0,0, 0,0,1,0, x,y,z,1]
inverse_bind = [translation_matrix(0, 0, 0), translation_matrix(0, 0, -1)]

# 이진 버퍼를 차례로 채우고, 각 구간을 bufferView와 accessor로 가리킨다.
blob = bytearray()
views, accessors = [], []
def add(data, fmt, count, ctype, atype, comp, target=None, minmax=False):
    global blob
    while len(blob) % 4:
        blob += b'\0'
    offset = len(blob)
    flat = [x for item in data for x in (item if isinstance(item, (tuple, list)) else (item,))]
    blob += struct.pack('<' + fmt * len(flat), *flat)
    view = {"buffer": 0, "byteOffset": offset, "byteLength": len(blob) - offset}
    if target: view["target"] = target
    views.append(view)
    acc = {"bufferView": len(views) - 1, "componentType": ctype, "count": count, "type": atype}
    if minmax:
        acc["min"] = [min(p[i] for p in data) for i in range(comp)]
        acc["max"] = [max(p[i] for p in data) for i in range(comp)]
    accessors.append(acc)
    return len(accessors) - 1

FLOAT, USHORT, UINT = 5126, 5123, 5125
ARRAY_BUFFER, ELEMENT_ARRAY_BUFFER = 34962, 34963
a_pos = add(positions, 'f', len(positions), FLOAT, "VEC3", 3, ARRAY_BUFFER, minmax=True)
a_nrm = add(normals, 'f', len(normals), FLOAT, "VEC3", 3, ARRAY_BUFFER)
a_jnt = add(joints, 'H', len(joints), USHORT, "VEC4", 4, ARRAY_BUFFER)
a_wgt = add(weights, 'f', len(weights), FLOAT, "VEC4", 4, ARRAY_BUFFER)
a_idx = add(arrow_indices, 'I', len(arrow_indices), UINT, "SCALAR", 1, ELEMENT_ARRAY_BUFFER)
b_pos = add(base_positions, 'f', len(base_positions), FLOAT, "VEC3", 3, ARRAY_BUFFER, minmax=True)
b_nrm = add(base_normals, 'f', len(base_normals), FLOAT, "VEC3", 3, ARRAY_BUFFER)
b_idx = add(base_indices, 'I', len(base_indices), UINT, "SCALAR", 1, ELEMENT_ARRAY_BUFFER)
a_ibm = add(inverse_bind, 'f', 2, FLOAT, "MAT4", 16)
a_time = add(times, 'f', len(times), FLOAT, "SCALAR", 1)
accessors[a_time]["min"] = [times[0]]; accessors[a_time]["max"] = [times[-1]]
a_rot = add(rotations, 'f', len(rotations), FLOAT, "VEC4", 4)
a_swing_rot = add(swing_rotations, 'f', len(swing_rotations), FLOAT, "VEC4", 4)
a_swing_tr = add(swing_translations, 'f', len(swing_translations), FLOAT, "VEC3", 3)

gltf = {
    "asset": {"version": "2.0", "generator": "Nereides Engine2 tools/make_test_model.py"},
    "scene": 0,
    "scenes": [{"nodes": [0, 2, 3]}],
    "nodes": [
        {"name": "root", "children": [1]},
        {"name": "tip", "translation": [0, 0, 1]},
        {"name": "arrow", "mesh": 0, "skin": 0},
        {"name": "base", "mesh": 1, "translation": [0, -0.3, 0.2], "scale": [2, 1, 1]},
    ],
    "meshes": [
        {"name": "arrow", "primitives": [{
            "attributes": {"POSITION": a_pos, "NORMAL": a_nrm, "JOINTS_0": a_jnt, "WEIGHTS_0": a_wgt},
            "indices": a_idx, "mode": 4}]},
        {"name": "base", "primitives": [{
            "attributes": {"POSITION": b_pos, "NORMAL": b_nrm},
            "indices": b_idx, "mode": 4}]},
    ],
    "skins": [{"name": "arrow-skin", "joints": [0, 1], "inverseBindMatrices": a_ibm, "skeleton": 0}],
    "animations": [{"name": "bend",
        "samplers": [{"input": a_time, "output": a_rot, "interpolation": "LINEAR"}],
        "channels": [{"sampler": 0, "target": {"node": 1, "path": "rotation"}}]},
        {"name": "swing",
        "samplers": [{"input": a_time, "output": a_swing_rot, "interpolation": "LINEAR"},
                     {"input": a_time, "output": a_swing_tr, "interpolation": "LINEAR"}],
        "channels": [{"sampler": 0, "target": {"node": 1, "path": "rotation"}},
                     {"sampler": 1, "target": {"node": 0, "path": "translation"}}]}],
    "buffers": [{"byteLength": len(blob),
                 "uri": "data:application/octet-stream;base64," + base64.b64encode(bytes(blob)).decode()}],
    "bufferViews": views,
    "accessors": accessors,
}
out = os.path.join(os.path.dirname(__file__), "..", "assets", "test", "arrow.gltf")
with open(out, "w", encoding="utf-8", newline="\n") as f:
    json.dump(gltf, f, indent=1)
    f.write("\n")
print("wrote", os.path.normpath(out), "vertices", len(positions) + len(base_positions),
      "indices", len(arrow_indices) + len(base_indices), "bytes", len(blob))
