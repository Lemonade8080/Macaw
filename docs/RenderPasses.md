# Render Pass 사용법

뷰포트, 에셋 썸네일, 머티리얼 미리보기는 `FRenderer::RenderView()`를 호출한다. `BeginFrame(DeltaTime)`은 모든 View를 그리기 전에 프레임당 한 번 호출한다. ImGui 합성과 Present는 모든 View 렌더링 이후의 프레임 단계다.

```cpp
FRenderView View{};
View.mTarget = &Surface;
View.mCamera = Camera;
View.mSettings = Settings;
View.mRenderMode = ERenderMode::Lit;
View.SetPassEnabled(ERenderPass::Gizmo, false);
View.SetPassEnabled(ERenderPass::OrientationAxis, false);
Renderer.RenderView(View, Probe);
```

기본값은 모든 Pass 활성화다. 썸네일처럼 일부만 필요한 경우 `View.mPasses.reset()` 후 필요한 Pass를 활성화한다. `mRenderMode`는 해당 View의 표현 방식이며 공유 Pipeline 에셋을 수정하지 않는다.

| 순서 | Pass | 입력과 출력 |
| --- | --- | --- |
| 1 | SceneGeometry | 메쉬와 하늘을 씬 색상·깊이·스텐실에 출력 |
| 2 | SelectionOutline | 선택된 메쉬를 씬 깊이·스텐실을 사용해 출력 |
| 3 | SceneGuides | 그리드·축·Bounds 선 데이터를 출력 |
| 4 | Gizmo | 기존 Surface의 깊이를 Clear한 뒤 재사용하여 출력 |
| 5 | Text | 텍스트 출력 |
| 6 | Billboard | 앞선 Pass가 남긴 Surface 깊이를 사용해 빌보드 출력 |
| 7 | OrientationAxis | 현재 Surface 깊이와 작은 뷰포트로 방향축 출력 |

SceneGeometry가 꺼져 있으면 SelectionOutline도 실행되지 않는다. 나머지 Pass는 개별 활성화할 수 있다. View 시작 시 색상·깊이·스텐실을 초기화하며, 각 Pass 시작 시 해당 View의 출력과 뷰포트 및 공용 샘플러를 바인딩한다. View 완료 시 원래 씬 출력과 전체 뷰포트를 다시 바인딩한다.

기즈모 Probe가 있으면 기존 방식대로 `Surface.ClearDepth()`로 깊이 값만 초기화한 뒤 기즈모를 그린다. 기즈모가 없거나 Pass가 꺼져 있으면 깊이를 초기화하지 않는다. 방향축은 별도 초기화 없이 현재 깊이 버퍼를 사용한다. 별도 오버레이 깊이 텍스처는 생성하지 않는다.

World와 Editor는 `FRenderProbe`에 데이터를 수집한다. `EditorViewport::BuildRenderProbes()`는 기즈모와 `FLineRenderData`를 작성하며 GPU Draw를 호출하지 않는다. `FRenderQueue`는 서브메쉬와 머티리얼을 해석하고 배칭 순서로 정렬한다. 선택된 메쉬의 외곽선 목록은 정렬된 씬 목록에서 파생한다. `FMeshRenderer`는 씬·외곽선·기즈모 Pass의 공통 GPU 실행을 담당한다. Probe는 렌더링 중 변경되지 않는다.

Pass 추가 시 `ERenderPass`, 기본 활성화 마스크, `FRenderer::RenderView()`의 실행 순서, `ExecutePass()`의 실행 분기를 함께 갱신한다. 새 Pass가 사용하는 출력, 깊이 버퍼, GPU 상태와 선행 Pass 의존성을 명시한다. 투명 메쉬와 후처리 Render Graph는 이번 구조에 포함하지 않는다.

프로젝트에서 사용하는 Premake의 Visual Studio 생성 명령으로 프로젝트를 갱신한 뒤 `RenderPassTests`의 Debug/x64 구성을 빌드한다. 저장소 루트에서 `bin/Debug/x64/RenderPassTests.exe`를 실행한다. 테스트는 숨겨진 창과 Direct3D 11 디버그 장치를 사용하며, 렌더 타깃 readback으로 Pass 선택, 기즈모의 기존 깊이 초기화 동작, View 간 상태 격리와 GPU 진단 메시지를 확인한다.
