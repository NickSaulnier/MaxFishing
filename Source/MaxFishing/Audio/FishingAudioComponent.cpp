// Copyright MaxFishing Project

#include "FishingAudioComponent.h"
#include "Components/AudioComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"
#include "AudioParameter.h"
#include "Templates/UnrealTemplate.h"

UFishingAudioComponent::UFishingAudioComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UFishingAudioComponent::BeginPlay()
{
	Super::BeginPlay();
	EnsureReelComponent();
}

void UFishingAudioComponent::EnsureReelComponent()
{
	if (ReelLoopComponent || !GetOwner())
	{
		return;
	}

	ReelLoopComponent = NewObject<UAudioComponent>(GetOwner(), TEXT("FishingReelLoopAudio"));
	ReelLoopComponent->bAutoActivate = false;
	ReelLoopComponent->bIsUISound = false;
	ReelLoopComponent->SetupAttachment(GetOwner()->GetRootComponent());
	ReelLoopComponent->RegisterComponent();
}

void UFishingAudioComponent::NotifyCast()
{
	if (CastSound && GetWorld())
	{
		UGameplayStatics::PlaySound2D(GetWorld(), CastSound);
	}
}

void UFishingAudioComponent::NotifyReel(float Tension01)
{
	EnsureReelComponent();
	if (!ReelLoopComponent || !ReelSound)
	{
		return;
	}

	ReelLoopComponent->SetSound(ReelSound);
	ApplyReelTension(Tension01);

	if (!ReelLoopComponent->IsPlaying())
	{
		ReelLoopComponent->Play();
	}
}

void UFishingAudioComponent::NotifySplash(const FVector& WorldLocation)
{
	if (SplashSound && GetWorld())
	{
		UGameplayStatics::PlaySoundAtLocation(GetWorld(), SplashSound, WorldLocation);
	}
}

void UFishingAudioComponent::NotifyReelStopped()
{
	if (ReelLoopComponent)
	{
		ReelLoopComponent->Stop();
	}
}

void UFishingAudioComponent::ApplyReelTension(float Tension01)
{
	if (!ReelLoopComponent)
	{
		return;
	}

	const float Clamped = FMath::Clamp(Tension01, 0.f, 1.f);
	TArray<FAudioParameter> Params;
	Params.Emplace(FAudioParameter(FName(TEXT("Tension")), Clamped));
	ReelLoopComponent->SetParameters(MoveTemp(Params));
}
