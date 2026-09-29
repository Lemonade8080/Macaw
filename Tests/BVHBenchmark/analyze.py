from pathlib import Path
import csv
import math
import statistics as st
from collections import defaultdict

here = Path(__file__).resolve().parent
rows = list(csv.DictReader((here / 'refined.csv').open()))
assert len(rows) == 1152, f'Expected completed refinement, got {len(rows)} rows'
groups = defaultdict(list)
for r in rows:
    groups[r['mesh'], r['rays'], int(r['slice']), float(r['ratio'])].append(r)
values = {}
for key, group in groups.items():
    assert len(group) == 3
    values[key] = {field: st.median(float(r[field]) for r in group) for field in ['median_ns', 'build_ms', 'nodes', 'leaves', 'depth', 'tris_per_leaf', 'triangles', 'rays_count', 'hits']}
meshes = sorted({key[0] for key in values})
configs = sorted({key[2:] for key in values})
def gm(v):
    return math.exp(st.mean(math.log(x) for x in v))
def speed(mesh, cohort, config):
    return values[mesh, cohort, 8, 1.2]['median_ns'] / values[mesh, cohort, *config]['median_ns']
ranking = []
for config in configs:
    camera = gm(speed(m, 'camera', config) for m in meshes)
    surface = gm(speed(m, 'surface', config) for m in meshes)
    ranking.append((math.sqrt(camera * surface), *config, camera, surface))
ranking.sort(reverse=True)
print('global (speedup,slice,ratio,camera,surface):')
for r in ranking[:12]:
    print(r)
out = ['# BVH 파라미터 실험', '', '측정일: 2026-09-29. 엔진 기본값 `Slice=8`, `CT/CI=1.2`는 변경하지 않았다.', '',
       '## 측정 조건', '',
       '- CPU: AMD Ryzen 9 7940HS. MSVC x64 `/O2 /MD /DNDEBUG`, 단일 프로세스·단일 스레드.',
       '- 현재 `UMesh.h/.cpp`에서 빌드·순회 코드를 추출했다. 실제 DirectX 교차 검사, `FVector::ToSimpleMath`, `TArray`, 엔진 Memory/Stat 구현을 사용했다.',
       '- GPU나 애셋 레지스트리 대신 정점·인덱스만 보유한 작은 테스트용 UMesh를 사용한다. 월드→로컬 변환, broadphase, 렌더링, stat fps는 측정하지 않는다.',
       '- 실험용 복사본에서 Slice별 클래스를 만들고 CT/CI만 변경한다. 제품 코드는 수정하지 않았다. 원본 해시는 source_hashes.json에 기록했다.',
       '- camera: 메시를 둘러싼 6개 축 방향 카메라의 32×32 화면 격자. 루트 AABB와 교차하는 레이만 남겼다.',
       '- surface: 삼각형을 균등하게 골라 내부 점을 뽑고, 메시 바깥 구면의 임의 방향에서 해당 점을 향하는 레이 4096개. 면적 비례 표면 샘플링은 아니다.',
       '- 실제 사용자 카메라 궤적을 기록한 데이터가 아니므로, 결과는 위 두 합성 레이 분포에 대한 값이다.',
       '- 1차: Slice 4/8/16/32/64 × CT/CI 0.25/0.5/1/1.2/2/4/8 × 8개 메시 = 280개 빌드.',
       '- 2차: Slice 8/16/32/64 × CT/CI 1.2/1.5/2/2.5/3/4 × 8개 메시 × 3회 = 576개 빌드. 순서를 고정 시드로 섞었다.',
       '- 각 빌드 후 각 레이 세트의 결과를 검증하고, 같은 세트를 5회 측정했다. 표는 각 회의 중앙값 3개의 중앙값이다. ns/ray는 해당 세트 전체 시간 ÷ 레이 수이다.',
       '- 빌드 시간은 메시 파일 읽기를 제외한다. 순회는 반복 접근으로 캐시가 데워진 조건이다. CPU 고정 클럭·코어 고정은 하지 않았다.',
       '- 모든 설정의 모든 레이 결과를 기본값 결과와 비교했다. 기본값 결과는 메시·레이 세트마다 약 128개를 전체 삼각형 검사와 비교했다. 전부 통과했다.',
       '- 몇 퍼센트 차이는 실행 환경과 측정 순서 영향을 받을 수 있다. 순위 1위를 유일한 최적값으로 보지 않는다.', '',
       '## 전체 비교', '', '각 메시·레이 세트의 기본값 대비 속도비를 동일 가중치로 기하평균했다. 큰 메시나 많이 등장하는 애셋에 추가 가중치를 주지 않았다. 1.05×는 처리율 5% 향상, 시간 약 4.8% 감소를 뜻한다.', '',
       '| Slice | CT/CI | 전체 속도비 | camera | surface |', '|---:|---:|---:|---:|---:|']
for total,s,q,c,t in ranking:
    out.append(f'| {s} | {q:g} | {total:.3f}× | {c:.3f}× | {t:.3f}× |')
out += ['', '## 메시별 관측 최상위 조합', '', '두 레이 세트의 속도비 기하평균이 가장 높은 조합이다. 여러 후보 중 최솟값을 고른 표이므로 성능 향상을 낙관적으로 보일 수 있다. Cube의 약 1% 차이는 같은 리프 수에서도 발생하므로 최적화 이득으로 해석하지 않는다.', '',
        '| 메시 | 삼각형 | Slice / (CT/CI) | 기본 camera ns | 후보 camera ns | 기본 surface ns | 후보 surface ns | 속도비 | 빌드 ms 기본→후보 | 리프 평균 삼각형 기본→후보 |',
        '|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|']
for m in meshes:
    config = max(configs, key=lambda p: gm(speed(m,c,p) for c in ['camera','surface']))
    b = values[m,'camera',8,1.2]; a = values[m,'camera',*config]
    bs = values[m,'surface',8,1.2]; ass = values[m,'surface',*config]
    ratio = gm(speed(m,c,config) for c in ['camera','surface'])
    out.append(f'| {Path(m).stem} | {int(b["triangles"])} | {config[0]} / {config[1]:g} | {b["median_ns"]:.1f} | {a["median_ns"]:.1f} | {bs["median_ns"]:.1f} | {ass["median_ns"]:.1f} | {ratio:.3f}× | {b["build_ms"]:.2f} → {a["build_ms"]:.2f} | {b["tris_per_leaf"]:.2f} → {a["tris_per_leaf"]:.2f} |')
out += ['', '## Slice의 빌드 비용 — CT/CI=2 고정', '', '| 메시 | Slice 8 ms | 16 ms | 32 ms | 64 ms |', '|---|---:|---:|---:|---:|']
for m in meshes:
    out.append('| '+Path(m).stem+' | '+' | '.join(f'{values[m,"camera",s,2]["build_ms"]:.2f}' for s in [8,16,32,64])+' |')
out += ['', '## 레이 표본', '', '| 메시 | camera 레이 수 | camera hit 비율 | surface 레이 수 | surface hit 비율 |', '|---|---:|---:|---:|---:|']
for m in meshes:
    a=values[m,'camera',8,1.2]; b=values[m,'surface',8,1.2]
    out.append(f'| {Path(m).stem} | {int(a["rays_count"])} | {100*a["hits"]/a["rays_count"]:.1f}% | {int(b["rays_count"])} | {100*b["hits"]/b["rays_count"]:.1f}% |')
out += ['', '## 해석', '',
        '- Slice는 후보 절단 위치의 해상도이다. 삼각형 중심의 분포, 빈 공간, 박스의 겹침과 메시의 방향 등에 따라 큰 값의 이득이 달라진다. 메시가 길거나 삼각형이 많다는 이유만으로 최적 Slice가 정해지지는 않는다.',
        '- 현재 구현은 각 절단 후보마다 왼쪽·오른쪽 bin을 다시 순회한다. 노드 평가의 bin 합산 비용이 O(Slice²)이므로 큰 Slice의 빌드 비용이 높다. 누적 합 방식으로 바꾸면 이 비용 곡선이 달라진다.',
        '- CT/CI는 이 코드에서 분할을 계속할지 리프로 끝낼지를 결정한다. 같은 노드에서 가장 좋은 절단면을 고르는 항에는 상수로 더해지므로 절단면 순위 자체는 바꾸지 않는다.',
        '- 비율을 높이면 리프에 삼각형을 더 남기는 쪽으로 기운다. 실제 노드 처리에는 자식 OBB 검사, 스택 조작, 분기 등이 들어가므로 단일 교차 함수 시간의 비율과 최적 튜닝값이 같다고 보장할 수 없다.',
        '- CT/CI도 메시와 레이 분포의 영향을 받는다. 한 변수라 탐색은 쉽지만 성능 함수가 매끈하지 않다. 임계값을 넘을 때만 트리가 바뀌므로 같거나 비슷한 결과의 구간이 생긴다.',
        '- 이 결과로 제품 기본값을 자동 변경하지 않았다. 실제 장면에서 자주 피킹되는 메시의 가중치와 카메라 레이를 적용한 검증이 다음 단계다.', '',
        '## 재실행', '',
        '프로젝트 루트에서 Python으로 `Tests/BVHBenchmark/generate.py`를 실행한 뒤 `Tests\\BVHBenchmark\\run.cmd`를 실행한다. 세밀한 반복 실험은 `Tests\\BVHBenchmark\\run.cmd --refine`, 보고서 생성은 `python Tests/BVHBenchmark/analyze.py`이다. run.cmd의 Visual Studio 경로는 이 PC 기준이다.', '',
        '파일: results.csv(1차), refined.csv(2차), aggregate.csv(2차 집계), source_hashes.json(원본 버전).', '']
(here / 'report.md').write_text('\n'.join(out),encoding='utf-8')
with (here / 'aggregate.csv').open('w',newline='') as f:
    w=csv.writer(f); w.writerow(['mesh','cohort','slice','ratio',*next(iter(values.values())).keys()])
    for key,v in sorted(values.items()): w.writerow([*key,*v.values()])
