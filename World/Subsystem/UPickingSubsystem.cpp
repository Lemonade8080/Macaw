#include "pch.h"

#include "UPickingSubsystem.h"

#include "Core/Stat/Stat.h"
#include "World/Component/UMeshComponent.h"
#include "World/Component/UBillboardComponent.h"
#include "World/Component/UPrimitiveComponent.h"

#include <chrono>
#include <cmath>

// Temporary picking breakdown: set to 0 to disable the extra timers and overlay rows.
#ifndef MACAW_PICKING_PHASE_TIMING
#define MACAW_PICKING_PHASE_TIMING 1
#endif

void UPickingSubsystem::RegisterComponent(UPrimitiveComponent* Component) {
    if (Component == nullptr || ContainsComponent(Component)) {
        return;
    }

    mComponents.emplace_back(Component);
}

void UPickingSubsystem::UnregisterComponent(UPrimitiveComponent* Component) {
    std::erase_if(mComponents, [Component](const TObjectRef<UPrimitiveComponent>& ComponentRef) {
        return ComponentRef.Get() == Component;
    });
}

bool UPickingSubsystem::Raycast(const FRay& Ray, UPrimitiveComponent*& OutComponent, float& OutDistance, const FMatrix* CameraWorld) const {
    const std::chrono::steady_clock::time_point StartTime{std::chrono::steady_clock::now()};
#if MACAW_PICKING_PHASE_TIMING
    std::chrono::steady_clock::duration NarrowPhaseTime{};
#endif
    OutComponent = nullptr;
    OutDistance = std::numeric_limits<float>::max();

    for (const TObjectRef<UPrimitiveComponent>& ComponentRef : mComponents) {
        UPrimitiveComponent* Component{ComponentRef.Get()};
        if (Component == nullptr || !Component->IsActive() || !Component->IsVisible()) {
            continue;
        }

        if (Component->GetTypeInfo()->IsA(UBillboardComponent::StaticTypeInfo())) {
            if (CameraWorld == nullptr) {
                continue;
            }
            const UBillboardComponent* Billboard{static_cast<const UBillboardComponent*>(Component)};
            std::array<FVector3, 4> Corners{};
            if (!Billboard->GetWorldCorners(*CameraWorld, Corners)) {
                continue;
            }
            const FVector3 Right{Corners[2] - Corners[0]};
            const FVector3 Down{Corners[1] - Corners[0]};
            const FVector3 Normal{Right.Cross(Down)};
            const FVector3 Direction{Ray.direction};
            const float Denominator{Direction.Dot(Normal)};
            if (std::abs(Denominator) <= 0.000001f) {
                continue;
            }
            const float HitDistance{(Corners[0] - FVector3{Ray.position}).Dot(Normal) / Denominator};
            if (HitDistance < 0.0f || HitDistance >= OutDistance) {
                continue;
            }
            const FVector3 HitOffset{FVector3{Ray.position} + Direction * HitDistance - Corners[0]};
            const float Horizontal{HitOffset.Dot(Right) / Right.LengthSquared()};
            const float Vertical{HitOffset.Dot(Down) / Down.LengthSquared()};
            if (Horizontal >= 0.0f && Horizontal <= 1.0f && Vertical >= 0.0f && Vertical <= 1.0f) {
                OutComponent = Component;
                OutDistance = HitDistance;
            }
            continue;
        }

        DirectX::BoundingOrientedBox WorldBox{};
        Component->GetPickingBox().Transform(WorldBox, Component->GetComponentToWorld().ToSimpleMath());

        float BroadPhaseDistance{0.0f};
        if (!WorldBox.Intersects(Ray.position, Ray.direction, BroadPhaseDistance)) {
            continue;
        }

        float HitDistance{BroadPhaseDistance};
        if (Component->GetTypeInfo()->IsA(UMeshComponent::StaticTypeInfo())) {
            auto* MeshComponent{static_cast<UMeshComponent*>(Component)};
#if MACAW_PICKING_PHASE_TIMING
            const auto NarrowPhaseStart{std::chrono::steady_clock::now()};
#endif
            const bool HitMesh{MeshComponent->RaycastMesh(Ray, HitDistance)};
#if MACAW_PICKING_PHASE_TIMING
            NarrowPhaseTime += std::chrono::steady_clock::now() - NarrowPhaseStart;
#endif
            if (!HitMesh) {
                continue;
            }
        }

        if (HitDistance < OutDistance) {
            OutComponent = Component;
            OutDistance = HitDistance;
        }
    }

    const double Milliseconds{std::chrono::duration<double, std::milli>{std::chrono::steady_clock::now() - StartTime}.count()};
#if MACAW_PICKING_PHASE_TIMING
    // All remaining work (including billboard picking) belongs to broad phase here.
    Stat::RecordPickingTime(Milliseconds, std::chrono::duration<double, std::milli>{NarrowPhaseTime}.count());
#else
    Stat::RecordPickingTime(Milliseconds);
#endif
    return OutComponent != nullptr;
}

bool UPickingSubsystem::ContainsComponent(const UPrimitiveComponent* Component) const {
    return std::ranges::any_of(mComponents, [Component](const TObjectRef<UPrimitiveComponent>& ComponentRef) {
        return ComponentRef.Get() == Component;
    });
}

const TArray<TObjectRef<UPrimitiveComponent>>& UPickingSubsystem::GetRegisteredComponents() const {
    return mComponents;
}

void UPickingSubsystem::OnDeinitialize() {
    mComponents.clear();
}
