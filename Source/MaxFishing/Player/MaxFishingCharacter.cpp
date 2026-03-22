// Copyright MaxFishing Project

#include "Player/MaxFishingCharacter.h"
#include "Fishing/FishingRodComponent.h"
#include "Audio/FishingAudioComponent.h"
#include "CableComponent.h"
#include "Components/SplineComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Camera/CameraComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "GameFramework/PlayerController.h"
#include "Engine/LocalPlayer.h"

AMaxFishingCharacter::AMaxFishingCharacter()
{
	PrimaryActorTick.bCanEverTick = true;

	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->bUsePawnControlRotation = true;
	CameraBoom->TargetArmLength = 320.f;

	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	FollowCamera->bUsePawnControlRotation = false;

	FishingLineCable = CreateDefaultSubobject<UCableComponent>(TEXT("FishingLineCable"));
	FishingLineCable->SetupAttachment(GetMesh());
	FishingLineCable->NumSegments = 10;
	FishingLineCable->CableWidth = 0.4f;
	FishingLineCable->bAttachStart = true;

	FishingLineSpline = CreateDefaultSubobject<USplineComponent>(TEXT("FishingLineSpline"));
	FishingLineSpline->SetupAttachment(GetMesh());
	FishingLineSpline->SetVisibility(false);

	FishingRod = CreateDefaultSubobject<UFishingRodComponent>(TEXT("FishingRod"));
	FishingAudio = CreateDefaultSubobject<UFishingAudioComponent>(TEXT("FishingAudio"));

	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = true;
	bUseControllerRotationRoll = false;
}

void AMaxFishingCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	UEnhancedInputComponent* EIC = Cast<UEnhancedInputComponent>(PlayerInputComponent);
	if (!EIC)
	{
		return;
	}

	if (!CastAction)
	{
		CastAction = NewObject<UInputAction>(this, TEXT("IA_FishingCast"));
		CastAction->ValueType = EInputActionValueType::Boolean;
	}

	if (!ReelAction)
	{
		ReelAction = NewObject<UInputAction>(this, TEXT("IA_FishingReel"));
		ReelAction->ValueType = EInputActionValueType::Boolean;
	}

	if (!FishingIMC)
	{
		FishingIMC = NewObject<UInputMappingContext>(this, TEXT("IMC_Fishing"));
		FishingIMC->MapKey(CastAction, EKeys::LeftMouseButton);
		FishingIMC->MapKey(ReelAction, EKeys::RightMouseButton);
	}

	EIC->BindAction(CastAction, ETriggerEvent::Started, this, &ThisClass::DoCast);
	EIC->BindAction(ReelAction, ETriggerEvent::Started, this, &ThisClass::DoReelStart);
	EIC->BindAction(ReelAction, ETriggerEvent::Completed, this, &ThisClass::DoReelStop);

	if (APlayerController* PC = Cast<APlayerController>(Controller))
	{
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer()))
		{
			if (FishingIMC)
			{
				Subsystem->AddMappingContext(FishingIMC, 0);
			}
		}
	}
}

void AMaxFishingCharacter::DoCast()
{
	if (!FishingRod)
	{
		return;
	}

	const FRotator YawOnly(0.f, GetControlRotation().Yaw, 0.f);
	const FVector Dir = YawOnly.Vector();
	FishingRod->CastToward(Dir);

	if (FishingAudio)
	{
		FishingAudio->NotifyCast();
		if (FishingRod->IsLineInWater())
		{
			FishingAudio->NotifySplash(FishingRod->GetLureWorldLocation());
		}
	}
}

void AMaxFishingCharacter::DoReelStart()
{
	if (FishingRod)
	{
		FishingRod->SetReelingIn(true);
	}
}

void AMaxFishingCharacter::DoReelStop()
{
	if (FishingRod)
	{
		FishingRod->SetReelingIn(false);
	}
	if (FishingAudio)
	{
		FishingAudio->NotifyReelStopped();
	}
}
