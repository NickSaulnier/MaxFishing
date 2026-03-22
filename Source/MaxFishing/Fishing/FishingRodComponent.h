// Copyright MaxFishing Project

#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"
#include "Components/ActorComponent.h"
#include "Components/SceneComponent.h"
#include "FishingRodComponent.generated.h"

class UCableComponent;
class USplineComponent;
class UNiagaraSystem;
class AMaxFishingCharacter;

UENUM(BlueprintType)
enum class EFishingLineVisualMode : uint8
{
	Cable UMETA(DisplayName = "Cable (Verlet)"),
	SplineTrace UMETA(DisplayName = "Spline + line trace")
};

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class MAXFISHING_API UFishingRodComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UFishingRodComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/** Cast toward world direction; uses line trace to place lure up to MaxCastDistance. */
	UFUNCTION(BlueprintCallable, Category = "Fishing")
	void CastToward(const FVector& WorldDirection);

	UFUNCTION(BlueprintCallable, Category = "Fishing")
	void SetReelingIn(bool bInReeling);

	UFUNCTION(BlueprintCallable, Category = "Fishing")
	bool IsLineInWater() const { return bLureInWater; }

	UFUNCTION(BlueprintPure, Category = "Fishing")
	FVector GetLureWorldLocation() const { return LureWorldLocation; }

	/** 0 = slack, 1 = max tension (for audio / UI). */
	UFUNCTION(BlueprintPure, Category = "Fishing")
	float GetLineTension01() const { return LineTension01; }

protected:
	virtual void BeginPlay() override;

	void UpdateLineVisual();
	void UpdateCableLine() const;
	void UpdateSplineLine() const;
	FVector GetRodTipWorldLocation() const;
	void SpawnSplashIfNeeded();

	UPROPERTY(EditAnywhere, Category = "Fishing|Line")
	EFishingLineVisualMode LineVisualMode = EFishingLineVisualMode::Cable;

	UPROPERTY(EditAnywhere, Category = "Fishing|Line", meta = (ClampMin = "100.0"))
	float MaxCastDistance = 2500.f;

	UPROPERTY(EditAnywhere, Category = "Fishing|Line")
	TObjectPtr<USceneComponent> RodTipSource;

	UPROPERTY(EditAnywhere, Category = "Fishing|VFX")
	TSoftObjectPtr<UNiagaraSystem> SplashSystem;

	UPROPERTY(EditAnywhere, Category = "Fishing|Line")
	float SplineSag = 80.f;

	UPROPERTY(EditAnywhere, Category = "Fishing|Line")
	TEnumAsByte<ECollisionChannel> LureTraceChannel = ECC_Visibility;

	UPROPERTY(Transient)
	TObjectPtr<AMaxFishingCharacter> OwnerCharacter;

	FVector LureWorldLocation = FVector::ZeroVector;
	bool bLureInWater = false;
	bool bReelingIn = false;
	float LineTension01 = 0.f;
	bool bHadSplashEvent = false;

	FVector LastLureForSplashTest = FVector::ZeroVector;
};
