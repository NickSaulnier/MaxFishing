// Copyright MaxFishing Project

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "MaxFishingCharacter.generated.h"

class UCableComponent;
class USplineComponent;
class UFishingRodComponent;
class UFishingAudioComponent;
class UInputAction;
class UInputMappingContext;
class USpringArmComponent;
class UCameraComponent;

UCLASS()
class MAXFISHING_API AMaxFishingCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	AMaxFishingCharacter();

	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Fishing")
	TObjectPtr<UCableComponent> FishingLineCable;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Fishing")
	TObjectPtr<USplineComponent> FishingLineSpline;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Fishing")
	TObjectPtr<UFishingRodComponent> FishingRod;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Fishing")
	TObjectPtr<UFishingAudioComponent> FishingAudio;

protected:
	void DoCast();
	void DoReelStart();
	void DoReelStop();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
	TObjectPtr<USpringArmComponent> CameraBoom;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
	TObjectPtr<UCameraComponent> FollowCamera;

	UPROPERTY(Transient)
	TObjectPtr<UInputMappingContext> FishingIMC;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> CastAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> ReelAction;
};
