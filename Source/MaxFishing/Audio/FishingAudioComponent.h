// Copyright MaxFishing Project

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "FishingAudioComponent.generated.h"

class USoundBase;
class UAudioComponent;

/**
 * Drives cast / reel / splash feedback. Assign UMetaSoundSource assets in editor and expose matching
 * float parameter "Tension" on reel MetaSounds for live line tension.
 */
UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class MAXFISHING_API UFishingAudioComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UFishingAudioComponent();

	/** Call when the player starts a cast (whoosh / rod bend). */
	UFUNCTION(BlueprintCallable, Category = "Audio|Fishing")
	void NotifyCast();

	/** While reeling; Tension01 should be 0..1 (drives MetaSound parameter "Tension" when present). */
	UFUNCTION(BlueprintCallable, Category = "Audio|Fishing")
	void NotifyReel(float Tension01);

	/** Surface hit at world location (assign a spatial MetaSound or wave). */
	UFUNCTION(BlueprintCallable, Category = "Audio|Fishing")
	void NotifySplash(const FVector& WorldLocation);

	/** Stop looping reel audio when the player releases reel input. */
	UFUNCTION(BlueprintCallable, Category = "Audio|Fishing")
	void NotifyReelStopped();

	UPROPERTY(EditAnywhere, Category = "Audio|MetaSound")
	TObjectPtr<USoundBase> CastSound;

	UPROPERTY(EditAnywhere, Category = "Audio|MetaSound")
	TObjectPtr<USoundBase> ReelSound;

	UPROPERTY(EditAnywhere, Category = "Audio|MetaSound")
	TObjectPtr<USoundBase> SplashSound;

protected:
	virtual void BeginPlay() override;

	void EnsureReelComponent();
	void ApplyReelTension(float Tension01);

	UPROPERTY(Transient)
	TObjectPtr<UAudioComponent> ReelLoopComponent;
};
