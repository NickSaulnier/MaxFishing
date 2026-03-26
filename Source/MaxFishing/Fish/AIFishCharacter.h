// Copyright MaxFishing Project

#pragma once

#include "CoreMinimal.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/Character.h"
#include "AIFishCharacter.generated.h"

class UWaterBodyComponent;

UCLASS()
class MAXFISHING_API AAIFishCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	AAIFishCharacter();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Fish")
	TObjectPtr<UStaticMeshComponent> TroutMesh;

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	/** Wander + velocity steering while staying inside the lake volume (matches runtime lake placement). */
	void TickWaterSwimming(float DeltaSeconds, class UCharacterMovementComponent* Move);
	bool ResolveWaterBody();
	void PickNewSwimTarget();
	float QueryWaterSurfaceZAtXY(float X, float Y) const;

	/** Quaternion frame so Z-up OBJ (nose +Z) swims along capsule +X with dorsal toward +Z; optional Euler delta multiplies after. */
	void ApplyTroutMeshSwimOrientation();

	/** Applied when the imported mesh bounds are very small (common for OBJ in meter units). */
	UPROPERTY(EditAnywhere, Category = "Fish", meta = (ClampMin = "0.01"))
	float TinyMeshAutoScale = 120.f;

	UPROPERTY(EditAnywhere, Category = "Fish", meta = (ClampMin = "0.0"))
	float TinyMeshMaxExtentCm = 8.f;

	/** Extra relative rotation after the quaternion import fix (usually zero; tune if your OBJ axis differs). */
	UPROPERTY(EditAnywhere, Category = "Fish")
	FRotator TroutMeshBaseRotation = FRotator::ZeroRotator;

	/** Subtle whole-mesh pitch wobble to complement material WPO swim (static mesh only). */
	UPROPERTY(EditAnywhere, Category = "Fish|Swim")
	bool bEnableSwimWobble = true;

	UPROPERTY(EditAnywhere, Category = "Fish|Swim", meta = (ClampMin = "0.0"))
	float SwimWobblePitchAmplitudeDegrees = 1.5f;

	UPROPERTY(EditAnywhere, Category = "Fish|Swim", meta = (ClampMin = "0.0"))
	float SwimWobbleFrequencyHz = 0.45f;

	/** Move horizontally toward wander targets and follow wave height (see MaxFishingFishSpawn lake constants). */
	UPROPERTY(EditAnywhere, Category = "Fish|Swim|Water")
	bool bEnableWaterSwimming = true;

	UPROPERTY(EditAnywhere, Category = "Fish|Swim|Water", meta = (ClampMin = "0.0"))
	float SwimSpeedCmPerSec = 115.f;

	/** Depth below the wave surface (cm); matches demo trout spawn. */
	UPROPERTY(EditAnywhere, Category = "Fish|Swim|Water", meta = (ClampMin = "0.0"))
	float SwimDepthBelowSurfaceCm = 25.f;

	UPROPERTY(EditAnywhere, Category = "Fish|Swim|Water", meta = (ClampMin = "10.0"))
	float SwimRetargetDistanceCm = 450.f;

	UPROPERTY(EditAnywhere, Category = "Fish|Swim|Water", meta = (ClampMin = "0.0"))
	float LakeBoundaryMarginCm = 800.f;

	/** Default circular lake spline radius (cm); must match MaxFishingWaterPlacement / fish spawn. */
	UPROPERTY(EditAnywhere, Category = "Fish|Swim|Water", meta = (ClampMin = "100.0"))
	float LakeSwimRadiusCm = 15000.f;

	UPROPERTY(EditAnywhere, Category = "Fish|Swim|Water", meta = (ClampMin = "0.1"))
	float SwimZStabilize = 1.35f;

	UPROPERTY(EditAnywhere, Category = "Fish|Swim|Water", meta = (ClampMin = "0.0"))
	float MaxVerticalSwimSpeedCm = 90.f;

	TWeakObjectPtr<UWaterBodyComponent> CachedWaterBody;
	FVector LakeCenterWorld = FVector::ZeroVector;
	FVector SwimTargetWorld = FVector::ZeroVector;

	FRotator BaseTroutMeshRelativeRotation = FRotator::ZeroRotator;
	float SwimWobblePhase = 0.f;
	float SwimWobbleTime = 0.f;
};
