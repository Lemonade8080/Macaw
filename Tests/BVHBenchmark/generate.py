from pathlib import Path
import hashlib
import json

here = Path(__file__).resolve().parent
root = here.parent.parent
header = (root / 'Asset/UMesh.h').read_text(encoding='utf-8-sig')
source = (root / 'Asset/UMesh.cpp').read_text(encoding='utf-8-sig')
cls = header[header.index('class FRaycastAccelerationStructure {'):header.index('class UMesh :')]
defs = source[source.index('bool FRaycastAccelerationStructure::BuildStructure('):]
generated = ['#include "support.h"\n']
for slices in (4, 8, 16, 32, 64):
    name = f'BVH{slices}'
    body = cls.replace('private:', 'public:').replace('static constexpr Uint32 Slice = 8;', f'static constexpr Uint32 Slice = {slices};').replace('static constexpr float TraversalCostOverInternalCost = 1.2f;', 'inline static float TraversalCostOverInternalCost = 1.2f;')
    generated.append((body + defs).replace('FRaycastAccelerationStructure', name))
(here / 'generated.h').write_text('\n'.join(generated), encoding='utf-8')
(here / 'source_hashes.json').write_text(json.dumps({p: hashlib.sha256((root / p).read_bytes()).hexdigest() for p in ['Asset/UMesh.h', 'Asset/UMesh.cpp', 'Core/Memory/Memory.cpp', 'Core/Stat/Stat.cpp']}, indent=2), encoding='utf-8')
