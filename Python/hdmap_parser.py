# -*- coding: utf-8 -*-
import os
import sqlite3
import struct
import json
import argparse
from pathlib import Path

# ==============================================================================
# PySHP (shapefile) 라이브러리를 이용한 SHP 파싱
# ==============================================================================
try:
    import shapefile
except ImportError:
    shapefile = None

# ==============================================================================
# GPKG WKB Geometry Parser (Point, LineString, Polygon, Multi-Geometries)
# ==============================================================================

def parse_gpkg_geom(geom_bytes):
    if not geom_bytes:
        return None
        
    magic = geom_bytes[0:2]
    if magic != b'GP':
        return None
        
    version = geom_bytes[2]
    flags = geom_bytes[3]
    
    envelope_indicator = (flags & 0x0E) >> 1
    envelope_sizes = {0: 0, 1: 32, 2: 48, 3: 48, 4: 64}
    envelope_size = envelope_sizes.get(envelope_indicator, 0)
    
    header_size = 8 + envelope_size
    wkb_bytes = geom_bytes[header_size:]
    
    return parse_wkb(wkb_bytes)

def parse_wkb(wkb_bytes):
    if len(wkb_bytes) < 5:
        return None
        
    byte_order_byte = wkb_bytes[0]
    endian = '<' if byte_order_byte == 1 else '>'
    
    geom_type = struct.unpack(endian + 'I', wkb_bytes[1:5])[0]
    offset = 5
    
    # 1. Point / PointZ (1 / 1001)
    if geom_type in (1, 1001):
        has_z = (geom_type == 1001)
        point_size = 24 if has_z else 16
        fmt = endian + ('ddd' if has_z else 'dd')
        pt = struct.unpack(fmt, wkb_bytes[offset:offset+point_size])
        return {"type": "Point", "points": [pt]}
        
    # 2. LineString / LineStringZ (2 / 1002)
    elif geom_type in (2, 1002):
        points, offset = parse_linestring(wkb_bytes, offset, endian, has_z=(geom_type==1002))
        return {"type": "LineString", "points": points}
        
    # 3. Polygon / PolygonZ (3 / 1003)
    elif geom_type in (3, 1003):
        points, offset = parse_polygon(wkb_bytes, offset, endian, has_z=(geom_type==1003))
        return {"type": "Polygon", "points": points}
        
    # 4. MultiPoint / MultiPointZ (4 / 1004)
    elif geom_type in (4, 1004):
        num_points = struct.unpack(endian + 'I', wkb_bytes[offset:offset+4])[0]
        offset += 4
        all_pts = []
        for _ in range(num_points):
            sub_endian = '<' if wkb_bytes[offset] == 1 else '>'
            sub_geom_type = struct.unpack(sub_endian + 'I', wkb_bytes[offset+1:offset+5])[0]
            offset += 5
            has_z = (sub_geom_type in (1, 1001))
            point_size = 24 if has_z else 16
            fmt = sub_endian + ('ddd' if has_z else 'dd')
            pt = struct.unpack(fmt, wkb_bytes[offset:offset+point_size])
            all_pts.append(pt)
            offset += point_size
        return {"type": "MultiPoint", "points": all_pts}
        
    # 5. MultiLineString / MultiLineStringZ (5 / 1005)
    elif geom_type in (5, 1005):
        num_linestrings = struct.unpack(endian + 'I', wkb_bytes[offset:offset+4])[0]
        offset += 4
        
        all_lines = []
        for _ in range(num_linestrings):
            sub_endian = '<' if wkb_bytes[offset] == 1 else '>'
            sub_geom_type = struct.unpack(sub_endian + 'I', wkb_bytes[offset+1:offset+5])[0]
            offset += 5
            
            points, offset = parse_linestring(wkb_bytes, offset, sub_endian, has_z=(sub_geom_type in (2, 1002)))
            all_lines.append(points)
            
        merged_points = []
        for line in all_lines:
            merged_points.extend(line)
            
        return {"type": "LineString", "points": merged_points}
        
    # 6. MultiPolygon / MultiPolygonZ (6 / 1006)
    elif geom_type in (6, 1006):
        num_polygons = struct.unpack(endian + 'I', wkb_bytes[offset:offset+4])[0]
        offset += 4
        
        all_polys = []
        for _ in range(num_polygons):
            sub_endian = '<' if wkb_bytes[offset] == 1 else '>'
            sub_geom_type = struct.unpack(sub_endian + 'I', wkb_bytes[offset+1:offset+5])[0]
            offset += 5
            
            points, offset = parse_polygon(wkb_bytes, offset, sub_endian, has_z=(sub_geom_type in (3, 1003)))
            all_polys.append(points)
            
        merged_points = []
        for poly in all_polys:
            merged_points.extend(poly)
            
        return {"type": "Polygon", "points": merged_points}
        
    return None

def parse_linestring(wkb_bytes, offset, endian, has_z=False):
    num_points = struct.unpack(endian + 'I', wkb_bytes[offset:offset+4])[0]
    offset += 4
    
    points = []
    point_size = 24 if has_z else 16
    fmt = endian + ('ddd' if has_z else 'dd')
    
    for _ in range(num_points):
        pt = struct.unpack(fmt, wkb_bytes[offset:offset+point_size])
        points.append(pt)
        offset += point_size
        
    return points, offset

def parse_polygon(wkb_bytes, offset, endian, has_z=False):
    num_rings = struct.unpack(endian + 'I', wkb_bytes[offset:offset+4])[0]
    offset += 4
    
    outer_ring_points = []
    
    for r in range(num_rings):
        num_points = struct.unpack(endian + 'I', wkb_bytes[offset:offset+4])[0]
        offset += 4
        
        ring_points = []
        point_size = 24 if has_z else 16
        fmt = endian + ('ddd' if has_z else 'dd')
        
        for _ in range(num_points):
            pt = struct.unpack(fmt, wkb_bytes[offset:offset+point_size])
            ring_points.append(pt)
            offset += point_size
            
        if r == 0:
            outer_ring_points = ring_points
            
    return outer_ring_points, offset

# ==============================================================================
# Batch Processing & Format Router
# ==============================================================================

def extract_type_from_filename(filename):
    import unicodedata
    norm_filename = unicodedata.normalize('NFC', filename)
    base = norm_filename.split('.')[0].upper()
    if "도로 병합" in norm_filename or "도로병합" in norm_filename or "도로 병합" in filename or "도로병합" in filename:
        return "HDMapData"
        
    parts = base.split('_')
    
    for p in [base] + parts:
        if p in ('A1', 'A2', 'A3', 'A4', 'B1', 'B2', 'B3', 'C1', 'C2', 'C3', 'C4', 'C5', 'C6'):
            # 매칭 테이블 명명
            type_mapping = {
                'A1': 'A1_NODE', 'A2': 'A2_LINK', 'A3': 'A3_DRIVEWAYSECTION', 'A4': 'A4_SUBSIDIARYSECTION',
                'B1': 'B1_SAFETYSIGN', 'B2': 'B2_SURFACELINEMARK', 'B3': 'B3_SURFACEMARK',
                'C1': 'C1_TRAFFICLIGHT', 'C3': 'C3_VEHICLEPROTECTIONSAFETY', 'C4': 'C4_SPEEDBUMP',
                'C5': 'C5_HEIGHTBARRIER', 'C6': 'C6_POSTPOINT'
            }
            return type_mapping.get(p, p)
            
    # 매핑 키워드가 파일 중간에 들어있는 경우
    for key, val in [('NODE', 'A1_NODE'), ('LINK', 'A2_LINK'), ('DRIVEWAY', 'A3_DRIVEWAYSECTION'), 
                     ('SURFACELINE', 'B2_SURFACELINEMARK'), ('SAFETYSIGN', 'B1_SAFETYSIGN'), 
                     ('SURFACEMARK', 'B3_SURFACEMARK'), ('TRAFFICLIGHT', 'C1_TRAFFICLIGHT'),
                     ('VEHICLEPROTECTION', 'C3_VEHICLEPROTECTIONSAFETY')]:
        if key in base:
            return val
            
    return "UNKNOWN"

def get_field_val(record_dict, possible_names, default=""):
    for name in possible_names:
        name_upper = name.upper()
        for k, v in record_dict.items():
            if k.upper() == name_upper:
                if isinstance(v, bytes):
                    try:
                        return v.decode('cp949').strip()
                    except:
                        try:
                            return v.decode('utf-8').strip()
                        except:
                            return str(v).strip()
                return str(v).strip() if v is not None else default
    return default

def get_field_val_int(record_dict, possible_names, default=0):
    val_str = get_field_val(record_dict, possible_names, None)
    if val_str is not None:
        try:
            return int(float(val_str))
        except:
            return default
    return default

def process_hdmap_files(input_dir, output_json, origin_x=None, origin_y=None, origin_z=0.0, swap_xy=False, invert_y=False, prefix="", scale=1.0):
    input_path = Path(input_dir)
    
    if not input_path.exists():
        print(f"Error: Input directory {input_path} does not exist.")
        return
        
    all_files = os.listdir(str(input_path))
    
    # 디버그 로그 파일 작성
    debug_log_path = Path(output_json).parent / "parser_debug.txt"
    try:
        import unicodedata
        with open(str(debug_log_path), "w", encoding="utf-8") as df:
            df.write(f"Input dir: {input_dir}\n")
            df.write(f"Prefix: {prefix}\n")
            df.write("All files list:\n")
            for f in all_files:
                # 일반 표현 및 NFC 정규화 버전, NFD 여부 기록
                nfc_f = unicodedata.normalize('NFC', f)
                is_nfd = (f != nfc_f)
                df.write(f"  - Original: {repr(f)} | NFC: {repr(nfc_f)} | is_NFD: {is_nfd}\n")
    except Exception as ex:
        pass

    # prefix가 지정된 경우 필터링 적용 (도로 병합 예외 처리)
    if prefix:
        import unicodedata
        filtered_files = []
        for f in all_files:
            norm_f = unicodedata.normalize('NFC', f)
            if f.startswith(prefix) or "도로 병합" in norm_f or "도로병합" in norm_f or "도로 병합" in f or "도로병합" in f:
                filtered_files.append(f)
        all_files = filtered_files
        print(f"Filtering files with prefix: '{prefix}' (Found {len(all_files)} files)")
    
    # 1. 포맷 판단 및 로드 (SHP와 GPKG 파일이 혼재된 경우 개별 처리)
    # 12개 속성 레이어 맵 초기화
    layer_keys = ("A1", "A2", "A3", "A4", "B2", "B3", "C1", "C3", "C4", "C5", "C6", "HDMapData")
    layer_records = {k: [] for k in layer_keys}
    
    print(f"Start parsing {len(all_files)} files in: {input_path}")
    
    for filename in all_files:
        file_type = extract_type_from_filename(filename)
        
        # 파일명에서 매칭되는 레이어 키 찾기
        layer_key = None
        for k in layer_keys:
            if file_type.startswith(k):
                layer_key = k
                break
        if not layer_key:
            continue
            
        filepath = input_path / filename
        
        # 1) SHP 파일 파싱
        if filename.lower().endswith('.shp'):
            if not shapefile:
                print(f"Warning: 'pyshp' library is missing. Skipping SHP file: {filename}")
                continue
            try:
                sf = shapefile.Reader(str(filepath), encoding='cp949')
                shapes = sf.shapes()
                records = sf.records()
                fields = [f[0] for f in sf.fields[1:]]
                
                # ID 필드 인덱스 찾기
                id_idx = 0
                for idx, f_name in enumerate(fields):
                    if f_name.upper() in ('ID', 'LINKID', 'NODEID', 'LINEID', 'SECTIONID'):
                        id_idx = idx
                        break
                        
                for i, (shape, record) in enumerate(zip(shapes, records)):
                    # ID 추출
                    rec_id = str(record[id_idx]) if id_idx < len(record) else f"{file_type}_{i}"
                    if not rec_id.strip():
                        rec_id = f"{file_type}_{i}"
                        
                    # 속성 레코드를 딕셔너리로 맵핑
                    properties = {}
                    for idx, f_name in enumerate(fields):
                        if idx < len(record):
                            properties[f_name] = record[idx]
                            
                    # Geometry 추출
                    raw_pts = shape.points
                    has_z = hasattr(shape, 'z') and len(shape.z) == len(raw_pts)
                    
                    points_3d = []
                    for j, pt in enumerate(raw_pts):
                        x, y = pt[0], pt[1]
                        z = shape.z[j] if has_z else 0.0
                        points_3d.append((x, y, z))
                        
                    if points_3d:
                        layer_records[layer_key].append({
                            "id": rec_id,
                            "properties": properties,
                            "raw_points": points_3d
                        })
            except Exception as e:
                print(f"Error reading shapefile {filename}: {e}")
                
        # 2) GPKG 파일 파싱
        elif filename.lower().endswith('.gpkg'):
            try:
                conn = sqlite3.connect(str(filepath))
                cursor = conn.cursor()
                
                cursor.execute("SELECT name FROM sqlite_master WHERE type='table';")
                tables = [t[0] for t in cursor.fetchall()]
                data_tables = [t for t in tables if not t.startswith("gpkg_") and not t.startswith("sqlite_") and not t.startswith("rtree_")]
                
                if not data_tables:
                    conn.close()
                    continue
                    
                table_name = data_tables[0]
                cursor.execute(f"SELECT * FROM \"{table_name}\";")
                rows = cursor.fetchall()
                columns = [col[0] for col in cursor.description]
                
                # ID 컬럼 및 Geometry 컬럼 인덱스 찾기
                id_idx = 0
                geom_idx = 1
                
                # Geometry 컬럼 먼저 찾기
                for idx, col_name in enumerate(columns):
                    if col_name.lower() in ('geom', 'geometry'):
                        geom_idx = idx
                        break
                        
                # 우선순위 리스트 기반으로 ID 컬럼 찾기 (덮어쓰기 방지)
                id_candidates = ('ID', 'LINKID', 'NODEID', 'LINEID', 'SECTIONID')
                found_id = False
                for cand in id_candidates:
                    for idx, col_name in enumerate(columns):
                        if col_name.upper() == cand:
                            id_idx = idx
                            found_id = True
                            break
                    if found_id:
                        break
                        
                for row in rows:
                    row_id = row[id_idx]
                    geom_blob = row[geom_idx]
                    
                    properties = {}
                    for idx, col_name in enumerate(columns):
                        if idx != geom_idx:
                            properties[col_name] = row[idx]
                            
                    parsed = parse_gpkg_geom(geom_blob)
                    if parsed and parsed["points"]:
                        layer_records[layer_key].append({
                            "id": row_id,
                            "properties": properties,
                            "raw_points": parsed["points"]
                        })
                conn.close()
            except Exception as e:
                print(f"Error reading GPKG {filename}: {e}")
                
    has_data = any(len(layer_records[k]) > 0 for k in layer_keys)
    if not has_data:
        print("No valid geometry records found matching the target road layers.")
        return
        
    # 2. 오프셋 설정을 위한 전체 레이어 통합 바운딩 박스 계산
    min_x, min_y = float('inf'), float('inf')
    max_x, max_y = float('-inf'), float('-inf')
    for k in layer_keys:
        for rec in layer_records[k]:
            for pt in rec["raw_points"]:
                x, y = pt[0], pt[1]
                if x < min_x: min_x = x
                if x > max_x: max_x = x
                if y < min_y: min_y = y
                if y > max_y: max_y = y
            
    if origin_x is None:
        origin_x = (min_x + max_x) / 2.0
    if origin_y is None:
        origin_y = (min_y + max_y) / 2.0
        
    print(f"\n--- Origin Reference Offset ---")
    print(f"Calculated Center Point (m): X = {origin_x:.3f}, Y = {origin_y:.3f}")
    print(f"UTM Bounding Box (m):")
    print(f"  X: {min_x:.3f} ~ {max_x:.3f} (Delta: {max_x-min_x:.3f}m)")
    print(f"  Y: {min_y:.3f} ~ {max_y:.3f} (Delta: {max_y-min_y:.3f}m)")
    print(f"--------------------------------\n")
    
    # 3. 레이어별 상대 좌표 변환, 보간 및 속성 매칭 적용 후 개별 JSON 파일로 저장
    output_dir = Path(output_json).parent
    os.makedirs(str(output_dir), exist_ok=True)
    
    for k in layer_keys:
        records = layer_records[k]
        json_data = []
        
        # METADATA_ORIGIN 행 생성 시 구조체 속성에 맞게 기본값들을 채워넣음
        metadata_item = {
            "Name": "METADATA_ORIGIN",
            "ID": "ORIGIN",
            "Points": [
                {"X": origin_x, "Y": origin_y, "Z": origin_z}
            ]
        }
        if k == "A1":
            metadata_item["NodeType"] = ""
        elif k == "A2":
            metadata_item["LinkType"] = ""
            metadata_item["MaxSpeed"] = 0
            metadata_item["R_LinkID"] = ""
            metadata_item["L_LinkID"] = ""
        elif k in ("A3", "A4", "B3"):
            metadata_item["Kind"] = ""
        elif k == "B2":
            metadata_item["LaneType"] = ""
            metadata_item["Color"] = ""
            metadata_item["Kind"] = ""
        elif k == "C1":
            metadata_item["LightType"] = ""
        elif k == "C3":
            metadata_item["ProtectionType"] = ""
        elif k == "C5":
            metadata_item["BarrierType"] = ""
            metadata_item["Height"] = 0.0
        elif k == "C6":
            metadata_item["PostType"] = ""
        elif k == "HDMapData":
            metadata_item["Type"] = ""
            
        json_data.append(metadata_item)
        
        existing_names = set()
        
        for i, rec in enumerate(records):
            raw_pts = rec["raw_points"]
            props = rec["properties"]
            
            if k in ("A2", "B2") and len(raw_pts) > 0:
                interpolated_raw = []
                interpolated_raw.append(raw_pts[0])
                for step_idx in range(1, len(raw_pts)):
                    p1 = raw_pts[step_idx-1]
                    p2 = raw_pts[step_idx]
                    
                    dx = p2[0] - p1[0]
                    dy = p2[1] - p1[1]
                    dz = p2[2] if len(p2) > 2 else 0.0
                    if len(p1) > 2:
                        dz -= p1[2]
                    else:
                        dz = 0.0
                        
                    dist = (dx*dx + dy*dy + dz*dz)**0.5
                    max_step = 2.0  # 2미터 간격 보간
                    if dist > max_step:
                        steps = int(dist / 1.5)
                        if steps < 2:
                            steps = 2
                        for s in range(1, steps):
                            t = s / steps
                            pi = (
                                p1[0] + dx * t,
                                p1[1] + dy * t,
                                p1[2] + (p2[2] - p1[2] if len(p2) > 2 and len(p1) > 2 else 0.0) * t
                            )
                            interpolated_raw.append(pi)
                    interpolated_raw.append(p2)
            else:
                interpolated_raw = raw_pts

            relative_points = []
            for pt in interpolated_raw:
                raw_x, raw_y = pt[0], pt[1]
                raw_z = pt[2] if len(pt) > 2 else 0.0
                
                if swap_xy:
                    ue_x = (raw_y - origin_y) * 100.0 * scale
                    ue_y = (raw_x - origin_x) * 100.0 * scale
                else:
                    ue_x = (raw_x - origin_x) * 100.0 * scale
                    ue_y = (raw_y - origin_y) * 100.0 * scale
                    
                if invert_y:
                    ue_y = -ue_y
                    
                ue_z = (raw_z - origin_z) * 100.0 * scale
                
                relative_points.append({
                    "X": round(ue_x, 2),
                    "Y": round(ue_y, 2),
                    "Z": round(ue_z, 2)
                })
                
            # Name, ID가 항상 문자열이 되도록 강제 및 중복 방지 접미사 처리
            base_name = str(rec["id"]) if rec["id"] is not None else ""
            if not base_name.strip():
                base_name = f"{k}_{i}"
                
            name = base_name
            dup_counter = 1
            while name in existing_names:
                name = f"{base_name}_dup{dup_counter}"
                dup_counter += 1
            existing_names.add(name)
                
            item_data = {
                "Name": name,
                "ID": name,
                "Points": relative_points
            }
            
            if k == "A1":
                item_data["NodeType"] = get_field_val(props, ["NodeType", "NODE_TYPE", "TYPE", "KIND"])
            elif k == "A2":
                item_data["LinkType"] = get_field_val(props, ["LinkType", "LINK_TYPE", "TYPE"])
                item_data["MaxSpeed"] = get_field_val_int(props, ["MaxSpeed", "MAX_SPEED", "SPEED", "MAXSPEED"], 0)
                item_data["R_LinkID"] = get_field_val(props, ["R_LinkID", "R_LINK_ID", "RLINKID"])
                item_data["L_LinkID"] = get_field_val(props, ["L_LinkID", "L_LINK_ID", "LLINKID"])
            elif k == "A3":
                item_data["Kind"] = get_field_val(props, ["Kind", "SECTIONKIND", "KIND", "TYPE"])
            elif k == "A4":
                item_data["Kind"] = get_field_val(props, ["Kind", "SECTKIND", "KIND", "TYPE"])
            elif k == "B2":
                item_data["LaneType"] = get_field_val(props, ["Type", "LINETYPE", "LANETYPE", "TYPE_CODE"])
                item_data["Color"] = get_field_val(props, ["Color", "LINECOLOR", "COLOR_CODE"])
                item_data["Kind"] = get_field_val(props, ["Kind", "MARK_KIND", "KIND"])
            elif k == "B3":
                item_data["Kind"] = get_field_val(props, ["Kind", "MARK_KIND", "KIND", "TYPE"])
            elif k == "C1":
                item_data["LightType"] = get_field_val(props, ["Type", "LIGHTTYPE", "KIND"])
            elif k == "C3":
                item_data["ProtectionType"] = get_field_val(props, ["Type", "PROTECTTYPE", "PROTECTIONTYPE", "KIND"])
            elif k == "C5":
                item_data["BarrierType"] = get_field_val(props, ["BarrierType", "TYPE", "BARRIER_TYPE", "KIND"])
                val_str = get_field_val(props, ["Height", "LIMITHEIGHT", "LIMIT_HEIGHT"], "0.0")
                try:
                    item_data["Height"] = float(val_str)
                except:
                    item_data["Height"] = 0.0
            elif k == "C6":
                item_data["PostType"] = get_field_val(props, ["PostType", "TYPE", "POST_TYPE", "KIND"])
            elif k == "HDMapData":
                item_data["Type"] = get_field_val(props, ["Type", "TYPE", "KIND"], "")
                
            json_data.append(item_data)
            
        if k == "HDMapData":
            layer_output_path = output_dir / "HDMapData.json"
        else:
            layer_output_path = output_dir / f"HDMap_{k}.json"
            
        with open(str(layer_output_path), 'w', encoding='utf-8') as f:
            json.dump(json_data, f, indent=2, ensure_ascii=False)
            
        print(f"Successfully exported {len(json_data)-1} items to {layer_output_path}")


# ==============================================================================
# Main Execution Entry
# ==============================================================================

if __name__ == "__main__":
    script_dir = Path(__file__).parent.resolve()
    default_output = script_dir / "HDMapData.json"

    # 바탕화면 한글 경로 안전 조립 (pathlib 적용)
    user_profile = Path(os.environ.get('USERPROFILE', r"C:\Users\user"))
    default_input = user_profile / "Desktop" / "남산 지형" / "잘라낸 정밀도로지도"

    parser = argparse.ArgumentParser(description="Convert Shapefile/GeoPackage HDMap data to Unreal DataTable JSON format")
    parser.add_argument("--input", default=str(default_input), help="Input directory containing GIS files")
    parser.add_argument("--output", default=str(default_output), help="Output JSON path")
    parser.add_argument("--origin_x", type=float, default=None, help="Origin Easting (X) in meters")
    parser.add_argument("--origin_y", type=float, default=None, help="Origin Northing (Y) in meters")
    parser.add_argument("--swap_xy", action="store_true", help="Swap X and Y coordinates (Northing->X, Easting->Y)")
    parser.add_argument("--invert_y", action="store_true", help="Invert Y coordinates")
    parser.add_argument("--prefix", default="", help="Filter input files by prefix (e.g. Cliped_)")
    parser.add_argument("--scale", type=float, default=1.0, help="Coordinate scale correction factor (e.g. 0.79263 for Seoul Web Mercator)")
    
    args = parser.parse_args()
    
    process_hdmap_files(args.input, args.output, args.origin_x, args.origin_y, 
                        swap_xy=args.swap_xy, invert_y=args.invert_y, 
                        prefix=args.prefix, scale=args.scale)
