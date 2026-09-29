$ErrorActionPreference = 'Stop'
$root = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$className = 'FMeshRaycastAccelerationStructure'
$generated = '#include "support.h"' + "`n"
$variants = @(@('Baseline32', 32, 'Tests/BVHBenchmark/partition-baseline'), @('Partition16', 16, 'Asset/UMesh'), @('Partition32', 32, 'Asset/UMesh'), @('Partition64', 64, 'Asset/UMesh'))
foreach ($variant in $variants) {
    $header = [IO.File]::ReadAllText((Join-Path $root ($variant[2] + '.h')))
    $source = [IO.File]::ReadAllText((Join-Path $root ($variant[2] + '.cpp')))
    $start = $header.IndexOf("class $className {")
    $body = $header.Substring($start, $header.IndexOf('class UMesh :') - $start).Replace('private:', 'public:')
    $body = $body -replace 'static constexpr Uint32 Slice = \d+;', ('static constexpr Uint32 Slice = ' + $variant[1] + ';')
    $body = $body -replace 'static constexpr float TraversalCostOverInternalCost = [\d.]+f;', 'inline static float TraversalCostOverInternalCost = 2.5f;'
    $defs = $source.Substring($source.IndexOf("bool ${className}::BuildStructure("))
    $generated += ($body + $defs).Replace($className, $variant[0]) + "`n"
}
[IO.File]::WriteAllText((Join-Path $PSScriptRoot 'partition-generated.h'), $generated)
$main = [IO.File]::ReadAllText((Join-Path $PSScriptRoot 'main.cpp'))
$common = $main.Substring(0, $main.IndexOf('template<class B> void Reference')).Replace('#include "generated.h"', '#include "partition-generated.h"')
[IO.File]::WriteAllText((Join-Path $PSScriptRoot 'partition-common.h'), $common)
Get-FileHash (Join-Path $root 'Asset/UMesh.h'), (Join-Path $root 'Asset/UMesh.cpp'), (Join-Path $PSScriptRoot 'partition-baseline.h'), (Join-Path $PSScriptRoot 'partition-baseline.cpp'), (Join-Path $root 'Content/Prisoner_Capoeira.bin') | Select-Object Path, Hash | ConvertTo-Json | Set-Content (Join-Path $PSScriptRoot 'partition-hashes.json')
