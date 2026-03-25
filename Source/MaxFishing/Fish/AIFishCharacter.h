// Copyright MaxFishing Project

#pragma once

#include "CoreMinimal.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/Character.h"
#include "AIFishCharacter.generated.h"

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

	/** Applied when the imported mesh bounds are very small (common for OBJ in meter units). */
	UPROPERTY(EditAnywhere, Category = "Fish", meta = (ClampMin = "0.01"))
	float TinyMeshAutoScale = 120.f;

	UPROPERTY(EditAnywhere, Category = "Fish", meta = (ClampMin = "0.0"))
	float TinyMeshMaxExtentCm = 8.f;

	/** Subtle whole-mesh pitch wobble to complement material WPO swim (static mesh only). */
	UPROPERTY(EditAnywhere, Category = "Fish|Swim")
	bool bEnableSwimWobble = true;

	UPROPERTY(EditAnywhere, Category = "Fish|Swim", meta = (ClampMin = "0.0"))
	float SwimWobblePitchAmplitudeDegrees = 1.5f;

	UPROPERTY(EditAnywhere, Category = "Fish|Swim", meta = (ClampMin = "0.0"))
	float SwimWobbleFrequencyHz = 0.45f;

	FRotator BaseTroutMeshRelativeRotation = FRotator::ZeroRotator;
	float SwimWobblePhase = 0.f;
	float SwimWobbleTime = 0.f;
};
