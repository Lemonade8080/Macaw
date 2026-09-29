$ErrorActionPreference = 'Stop'
$root = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$className = 'FMeshRaycastAccelerationStructure'
$generated = '#include "support.h"' + "`n"
foreach ($variant in @(@('Before', 'Tests/BVHBenchmark/traversal-baseline'), @('After', 'Asset/UMesh'))) {
    $header = [IO.File]::ReadAllText((Join-Path $root ($variant[1] + '.h')))
    $source = [IO.File]::ReadAllText((Join-Path $root ($variant[1] + '.cpp')))
    $start = $header.IndexOf("class $className {")
    $body = $header.Substring($start, $header.IndexOf('class UMesh :') - $start).Replace('private:', 'public:')
    $defs = $source.Substring($source.IndexOf("bool ${className}::BuildStructure("))
    $generated += ($body + $defs).Replace($className, $variant[0]) + "`n"
}
[IO.File]::WriteAllText((Join-Path $PSScriptRoot 'traversal-generated.h'), $generated)
$main = [IO.File]::ReadAllText((Join-Path $PSScriptRoot 'main.cpp'))
$common = $main.Substring(0, $main.IndexOf('template<class B> void Reference')).Replace('#include "generated.h"', '#include "traversal-generated.h"')
[IO.File]::WriteAllText((Join-Path $PSScriptRoot 'traversal-common.h'), $common)
$component = [IO.File]::ReadAllText((Join-Path $root 'World/Component/UMeshComponent.cpp'))
$start = $component.IndexOf('bool UMeshComponent::RaycastMesh(')
$component = $component.Substring($start, $component.IndexOf('void UMeshComponent::Serialize(') - $start).Replace('UMeshComponent', 'TestComponent').Replace('const UMesh*', 'const TestMesh*')
[IO.File]::WriteAllText((Join-Path $PSScriptRoot 'traversal-component.h'), $component)
Get-FileHash (Join-Path $root 'Asset/UMesh.h'), (Join-Path $root 'Asset/UMesh.cpp'), (Join-Path $root 'World/Component/UMeshComponent.cpp'), (Join-Path $PSScriptRoot 'traversal-baseline.h'), (Join-Path $PSScriptRoot 'traversal-baseline.cpp') | Select-Object Path, Hash | ConvertTo-Json | Set-Content (Join-Path $PSScriptRoot 'traversal-hashes.json')
