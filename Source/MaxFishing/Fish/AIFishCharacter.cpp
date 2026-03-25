// Copyright MaxFishing Project

#include "AIFishCharacter.h"
#include "FishAIController.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/Material.h"
#include "Materials/MaterialInterface.h"

namespace MaxFishingTroutMeshPrivate
{
	static UStaticMesh* TryLoadTroutStaticMesh()
	{
		// OBJ import from Content/Fish/RainbowTrout/base.obj creates asset "base" -> /Game/Fish/RainbowTrout/base.base
		static const TCHAR* const MeshPaths[] = {
			TEXT("/Game/Fish/RainbowTrout/base.base"),
			TEXT("/Game/Fish/RainbowTrout/Base.Base"),
			TEXT("/Game/Fish/RainbowTrout/SM_base.SM_base"),
			TEXT("/Game/Fish/RainbowTrout/SM_RainbowTrout.SM_RainbowTrout"),
		};
		for (const TCHAR* Path : MeshPaths)
		{
			if (UStaticMesh* M = LoadObject<UStaticMesh>(nullptr, Path))
			{
				return M;
			}
		}
		return nullptr;
	}

	static UMaterialInterface* TryLoadTroutMaterial()
	{
		static const TCHAR* const MaterialPaths[] = {
			TEXT("/Game/Fish/RainbowTrout/MI_RainbowTrout.MI_RainbowTrout"),
			TEXT("/Game/Fish/RainbowTrout/M_Trout.M_Trout"),
			TEXT("/Game/Fish/RainbowTrout/MI_Trout.MI_Trout"),
		};
		for (const TCHAR* Path : MaterialPaths)
		{
			if (UMaterialInterface* Mat = LoadObject<UMaterialInterface>(nullptr, Path))
			{
				return Mat;
			}
		}
		return nullptr;
	}

	static UMaterialInterface* GetBrightFallbackMaterial()
	{
		if (UMaterialInterface* Basic = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial")))
		{
			return Basic;
		}
		if (UMaterial* Def = UMaterial::GetDefaultMaterial(MD_Surface))
		{
			return Def;
		}
		return nullptr;
	}

	static void EnsureMaterialsOnMesh(UStaticMeshComponent* Mesh)
	{
		if (!Mesh)
		{
			return;
		}
		UMaterialInterface* TroutMat = TryLoadTroutMaterial();
		UMaterialInterface* Fallback = GetBrightFallbackMaterial();
		const int32 NumSlots = FMath::Max(Mesh->GetNumMaterials(), 1);
		for (int32 i = 0; i < NumSlots; ++i)
		{
			if (Mesh->GetMaterial(i) != nullptr)
			{
				continue;
			}
			UMaterialInterface* Use = (i == 0 && TroutMat) ? TroutMat : Fallback;
			if (Use)
			{
				Mesh->SetMaterial(i, Use);
			}
		}
		Mesh->MarkRenderStateDirty();
	}
}

AAIFishCharacter::AAIFishCharacter()
{
	PrimaryActorTick.bCanEverTick = false;

	AIControllerClass = AFishAIController::StaticClass();
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;

	GetMesh()->SetVisibility(false);
	GetMesh()->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	TroutMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("TroutMesh"));
	TroutMesh->SetupAttachment(GetCapsuleComponent());
	TroutMesh->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
	// Keep mesh pivot near capsule center so we do not bury half the mesh under terrain when Z is shallow.
	TroutMesh->SetRelativeLocation(FVector::ZeroVector);
	TroutMesh->SetRelativeRotation(FRotator(0.f, 90.f, 0.f));
	TroutMesh->SetVisibility(true);
	TroutMesh->SetHiddenInGame(false);
	TroutMesh->SetRenderInMainPass(true);

	// Avoid walking/penetration correction snapping fish onto terrain above the lake.
	if (UCharacterMovementComponent* Move = GetCharacterMovement())
	{
		Move->GravityScale = 0.f;
		Move->bOrientRotationToMovement = false;
		Move->DefaultLandMovementMode = MOVE_Flying;
		Move->DefaultWaterMovementMode = MOVE_Flying;
	}
	GetCapsuleComponent()->SetCapsuleHalfHeight(40.f);
	GetCapsuleComponent()->SetCapsuleRadius(28.f);
}

void AAIFishCharacter::BeginPlay()
{
	if (UCharacterMovementComponent* Move = GetCharacterMovement())
	{
		Move->SetMovementMode(MOVE_Flying);
		Move->Velocity = FVector::ZeroVector;
	}
	Super::BeginPlay();

	if (TroutMesh)
	{
		if (!TroutMesh->GetStaticMesh())
		{
			if (UStaticMesh* TroutSM = MaxFishingTroutMeshPrivate::TryLoadTroutStaticMesh())
			{
				TroutMesh->SetStaticMesh(TroutSM);
			}
		}
		if (!TroutMesh->GetStaticMesh())
		{
			if (UStaticMesh* Sphere = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere")))
			{
				TroutMesh->SetStaticMesh(Sphere);
				static constexpr float EngineSphereFallbackUniformScale = 0.6f;
				TroutMesh->SetRelativeScale3D(FVector(EngineSphereFallbackUniformScale));
			}
		}

		if (TroutMesh->GetStaticMesh())
		{
			const FVector Extent = TroutMesh->GetStaticMesh()->GetBounds().BoxExtent;
			const float MaxExtent = FMath::Max3(Extent.X, Extent.Y, Extent.Z);
			if (MaxExtent > 0.f && MaxExtent < TinyMeshMaxExtentCm)
			{
				TroutMesh->SetRelativeScale3D(FVector(TinyMeshAutoScale));
			}

			MaxFishingTroutMeshPrivate::EnsureMaterialsOnMesh(TroutMesh);
		}
	}
}
