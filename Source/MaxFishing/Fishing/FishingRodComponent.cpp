// Copyright MaxFishing Project

#include "FishingRodComponent.h"
#include "MaxFishingCharacter.h"
#include "FishingAudioComponent.h"
#include "CableComponent.h"
#include "Components/SplineComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"

UFishingRodComponent::UFishingRodComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	SetIsReplicatedByDefault(false);
}

void UFishingRodComponent::BeginPlay()
{
	Super::BeginPlay();
	OwnerCharacter = Cast<AMaxFishingCharacter>(GetOwner());
	LureWorldLocation = GetRodTipWorldLocation();
	LastLureForSplashTest = LureWorldLocation;
}

void UFishingRodComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (bReelingIn && OwnerCharacter)
	{
		const FVector Tip = GetRodTipWorldLocation();
		const FVector ToTip = Tip - LureWorldLocation;
		const float Dist = ToTip.Size();
		if (Dist > 5.f)
		{
			LureWorldLocation += ToTip.GetSafeNormal() * FMath::Min(Dist, 600.f * DeltaTime);
		}
	}

	const FVector TipNow = GetRodTipWorldLocation();
	const float Span = (LureWorldLocation - TipNow).Size();
	LineTension01 = FMath::Clamp(Span / FMath::Max(MaxCastDistance * 0.25f, 10.f), 0.f, 1.f);

	if (bReelingIn && OwnerCharacter && OwnerCharacter->FishingAudio)
	{
		OwnerCharacter->FishingAudio->NotifyReel(LineTension01);
	}

	UpdateLineVisual();
	SpawnSplashIfNeeded();
}

void UFishingRodComponent::CastToward(const FVector& WorldDirection)
{
	const FVector Dir = WorldDirection.GetSafeNormal();
	if (Dir.IsNearlyZero())
	{
		return;
	}

	const FVector Start = GetRodTipWorldLocation();
	FHitResult Hit;
	const FVector End = Start + Dir * MaxCastDistance;

	FCollisionQueryParams Params(NAME_None, false, GetOwner());
	bLureInWater = false;
	if (GetWorld()->LineTraceSingleByChannel(Hit, Start, End, LureTraceChannel, Params))
	{
		LureWorldLocation = Hit.ImpactPoint;
		if (Hit.GetActor() && Hit.GetActor()->ActorHasTag(FName(TEXT("Water"))))
		{
			bLureInWater = true;
		}
	}
	else
	{
		LureWorldLocation = End;
	}

	bReelingIn = false;
	bHadSplashEvent = true;
}

void UFishingRodComponent::SetReelingIn(bool bInReeling)
{
	bReelingIn = bInReeling;
}

FVector UFishingRodComponent::GetRodTipWorldLocation() const
{
	if (RodTipSource)
	{
		return RodTipSource->GetComponentLocation();
	}

	if (const APawn* Pawn = Cast<APawn>(GetOwner()))
	{
		FVector Eyes;
		FRotator Rot;
		Pawn->GetActorEyesViewPoint(Eyes, Rot);
		return Eyes + Rot.Vector() * 50.f;
	}

	return GetOwner() ? GetOwner()->GetActorLocation() : FVector::ZeroVector;
}

void UFishingRodComponent::UpdateLineVisual()
{
	if (!OwnerCharacter)
	{
		return;
	}

	switch (LineVisualMode)
	{
	case EFishingLineVisualMode::Cable:
		if (OwnerCharacter->FishingLineCable)
		{
			OwnerCharacter->FishingLineCable->SetVisibility(true);
		}
		if (OwnerCharacter->FishingLineSpline)
		{
			OwnerCharacter->FishingLineSpline->SetVisibility(false);
		}
		UpdateCableLine();
		break;
	case EFishingLineVisualMode::SplineTrace:
		if (OwnerCharacter->FishingLineCable)
		{
			OwnerCharacter->FishingLineCable->SetVisibility(false);
		}
		if (OwnerCharacter->FishingLineSpline)
		{
			OwnerCharacter->FishingLineSpline->SetVisibility(true);
		}
		UpdateSplineLine();
		break;
	default:
		break;
	}
}

void UFishingRodComponent::UpdateCableLine() const
{
	UCableComponent* Cable = OwnerCharacter ? OwnerCharacter->FishingLineCable : nullptr;
	if (!Cable)
	{
		return;
	}

	const FVector Tip = GetRodTipWorldLocation();
	Cable->SetWorldLocation(Tip);
	Cable->SetAttachEndTo(nullptr, NAME_None);
	Cable->EndLocation = Cable->GetComponentTransform().InverseTransformPosition(LureWorldLocation);
}

void UFishingRodComponent::UpdateSplineLine() const
{
	USplineComponent* Spline = OwnerCharacter ? OwnerCharacter->FishingLineSpline : nullptr;
	if (!Spline)
	{
		return;
	}

	const FVector Tip = GetRodTipWorldLocation();
	const FVector Mid = (Tip + LureWorldLocation) * 0.5f - FVector(0.f, 0.f, SplineSag);

	Spline->ClearSplinePoints(false);
	Spline->AddSplinePoint(Tip, ESplineCoordinateSpace::World, false);
	Spline->AddSplinePoint(Mid, ESplineCoordinateSpace::World, false);
	Spline->AddSplinePoint(LureWorldLocation, ESplineCoordinateSpace::World, false);
	Spline->UpdateSpline();
}

void UFishingRodComponent::SpawnSplashIfNeeded()
{
	if (!bHadSplashEvent)
	{
		return;
	}

	bHadSplashEvent = false;

	UNiagaraSystem* Splash = SplashSystem.IsValid() ? SplashSystem.Get() : SplashSystem.LoadSynchronous();
	if (Splash)
	{
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(GetWorld(), Splash, LureWorldLocation, FRotator::ZeroRotator, FVector(1.f), true, true);
	}
}
