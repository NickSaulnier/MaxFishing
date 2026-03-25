// Copyright MaxFishing Project

#include "AIFishCharacter.h"
#include "FishAIController.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture2D.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialParameters.h"
#include "RHIShaderPlatform.h"
#include "UObject/UObjectGlobals.h"

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

	/** OBJ imports often assign WorldGrid / default materials; those slots are non-null so we must replace them. */
	static bool IsLikelyPlaceholderOrMissingMaterial(const UMaterialInterface* Mat)
	{
		if (!Mat)
		{
			return true;
		}
		const FString P = Mat->GetPathName();
		return P.Contains(TEXT("WorldGridMaterial"))
			|| P.Contains(TEXT("LevelGridMaterial"))
			|| P.Contains(TEXT("WidgetGrid"))
			|| P.Contains(TEXT("WireframeMaterial"))
			|| P.Contains(TEXT("DefaultMaterial.DefaultMaterial"))
			|| P.Contains(TEXT("MI_DefaultGrid"));
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
			UMaterialInterface* Cur = Mesh->GetMaterial(i);
			if (!IsLikelyPlaceholderOrMissingMaterial(Cur))
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

	static void AssignMidToAllMeshSlots(UStaticMeshComponent* Mesh, UMaterialInstanceDynamic* MID)
	{
		if (!Mesh || !MID)
		{
			return;
		}
		const int32 NumSlots = FMath::Max(Mesh->GetNumMaterials(), 1);
		for (int32 i = 0; i < NumSlots; ++i)
		{
			Mesh->SetMaterial(i, MID);
		}
		Mesh->MarkRenderStateDirty();
	}

	/** Editor-created M_FishStaticSwim: FishDiffuse + WPO sine swim (preferred for static trout). */
	static bool ApplyProjectFishStaticSwimMaterial(UStaticMeshComponent* Mesh, UTexture2D* Diffuse)
	{
		if (!Mesh || !Diffuse)
		{
			return false;
		}
		UMaterialInterface* Parent = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Fish/RainbowTrout/M_FishStaticSwim.M_FishStaticSwim"));
		if (!Parent)
		{
			Parent = Cast<UMaterialInterface>(
				StaticLoadObject(UMaterialInterface::StaticClass(), nullptr, TEXT("/Game/Fish/RainbowTrout/M_FishStaticSwim.M_FishStaticSwim")));
		}
		if (!Parent)
		{
			return false;
		}
		if (UMaterial* Mat = Cast<UMaterial>(Parent))
		{
			if (Mat->IsCompilingOrHadCompileError(GMaxRHIShaderPlatform))
			{
				return false;
			}
		}
		UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(Parent, Mesh);
		if (!MID)
		{
			return false;
		}
		MID->SetTextureParameterValue(FName(TEXT("FishDiffuse")), Diffuse);
		MID->SetScalarParameterValue(FName(TEXT("SwimAmplitude")), 1.2f);
		MID->SetScalarParameterValue(FName(TEXT("SwimTimeScale")), 2.5f);
		MID->SetScalarParameterValue(FName(TEXT("SwimPhaseDensity")), 0.12f);
		AssignMidToAllMeshSlots(Mesh, MID);
		return true;
	}

	/** Editor-created M_FishRuntimeDiffuse (FishDiffuse -> BaseColor); no engine debug materials. */
	static bool ApplyProjectFishRuntimeDiffuseMaterial(UStaticMeshComponent* Mesh, UTexture2D* Diffuse)
	{
		if (!Mesh || !Diffuse)
		{
			return false;
		}
		UMaterialInterface* Parent = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Fish/RainbowTrout/M_FishRuntimeDiffuse.M_FishRuntimeDiffuse"));
		if (!Parent)
		{
			Parent = Cast<UMaterialInterface>(
				StaticLoadObject(UMaterialInterface::StaticClass(), nullptr, TEXT("/Game/Fish/RainbowTrout/M_FishRuntimeDiffuse.M_FishRuntimeDiffuse")));
		}
		if (!Parent)
		{
			return false;
		}
		UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(Parent, Mesh);
		if (!MID)
		{
			return false;
		}
		MID->SetTextureParameterValue(FName(TEXT("FishDiffuse")), Diffuse);
		AssignMidToAllMeshSlots(Mesh, MID);
		return true;
	}

	/** Last resort: engine materials (avoid DebugMeshMaterial — it is not a normal textured surface). */
	static bool ApplyEngineFallbackTexturedMaterial(UStaticMeshComponent* Mesh, UTexture2D* Diffuse)
	{
		if (!Mesh || !Diffuse)
		{
			return false;
		}
		static const TCHAR* const ParentCandidates[] = {
			TEXT("/Engine/EngineMaterials/DefaultTextMaterialOpaque.DefaultTextMaterialOpaque"),
			TEXT("/Engine/EngineDebugMaterials/TextureColorViewMode.TextureColorViewMode"),
			TEXT("/Engine/EngineMaterials/FlattenMaterial.FlattenMaterial"),
		};
		for (const TCHAR* Path : ParentCandidates)
		{
			UMaterialInterface* Parent = LoadObject<UMaterialInterface>(nullptr, Path);
			if (!Parent)
			{
				continue;
			}
			UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(Parent, Mesh);
			if (!MID)
			{
				continue;
			}
			TArray<FMaterialParameterInfo> ParentInfos;
			TArray<FGuid> ParentIds;
			Parent->GetAllTextureParameterInfo(ParentInfos, ParentIds);
			TArray<FMaterialParameterInfo> MidInfos;
			TArray<FGuid> MidIds;
			MID->GetAllTextureParameterInfo(MidInfos, MidIds);
			TArray<FMaterialParameterInfo> Infos = ParentInfos.Num() > 0 ? ParentInfos : MidInfos;
			if (Infos.Num() > 0)
			{
				for (const FMaterialParameterInfo& Info : Infos)
				{
					MID->SetTextureParameterValue(Info.Name, Diffuse);
				}
			}
			else
			{
				static const FName BlindTextureParamNames[] = {
					FName(TEXT("Texture")),
					FName(TEXT("DiffuseTexture")),
					FName(TEXT("BaseColorTexture")),
					FName(TEXT("Diffuse")),
					FName(TEXT("FontTexture")),
					FName(TEXT("SpriteTexture")),
					FName(TEXT("SlateTexture")),
				};
				for (FName ParamName : BlindTextureParamNames)
				{
					MID->SetTextureParameterValue(ParamName, Diffuse);
				}
			}
			AssignMidToAllMeshSlots(Mesh, MID);
			return true;
		}
		return false;
	}

	/** Binds `/Game/Fish/RainbowTrout/texture_diffuse` via an engine material MID (does not load M_RainbowTrout). */
	static bool ApplyRuntimeRainbowTroutMaterial(UStaticMeshComponent* Mesh)
	{
		if (!Mesh)
		{
			return false;
		}
		UTexture2D* Diffuse = LoadObject<UTexture2D>(nullptr, TEXT("/Game/Fish/RainbowTrout/texture_diffuse.texture_diffuse"));
		if (!Diffuse)
		{
			Diffuse = Cast<UTexture2D>(
				StaticLoadObject(UTexture2D::StaticClass(), nullptr, TEXT("/Game/Fish/RainbowTrout/texture_diffuse.texture_diffuse")));
		}
		if (!Diffuse)
		{
			return false;
		}

		if (ApplyProjectFishStaticSwimMaterial(Mesh, Diffuse))
		{
			return true;
		}
		if (ApplyProjectFishRuntimeDiffuseMaterial(Mesh, Diffuse))
		{
			return true;
		}
		return ApplyEngineFallbackTexturedMaterial(Mesh, Diffuse);
	}
}

AAIFishCharacter::AAIFishCharacter()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PostUpdateWork;

	AIControllerClass = AFishAIController::StaticClass();
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;

	GetMesh()->SetVisibility(false);
	GetMesh()->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	TroutMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("TroutMesh"));
	TroutMesh->SetupAttachment(GetCapsuleComponent());
	TroutMesh->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
	// Keep mesh pivot near capsule center so we do not bury half the mesh under terrain when Z is shallow.
	TroutMesh->SetRelativeLocation(FVector::ZeroVector);
	// OBJ import is often Z-up; pitch -90 lays the fish length along the water plane (was vertical in PIE).
	TroutMesh->SetRelativeRotation(FRotator(-90.f, 90.f, 0.f));
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

			const bool bApplied = MaxFishingTroutMeshPrivate::ApplyRuntimeRainbowTroutMaterial(TroutMesh);
			if (!bApplied)
			{
				MaxFishingTroutMeshPrivate::EnsureMaterialsOnMesh(TroutMesh);
			}

			BaseTroutMeshRelativeRotation = TroutMesh->GetRelativeRotation();
			SwimWobblePhase = FMath::FRandRange(0.f, UE_TWO_PI);
		}
	}
}

void AAIFishCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (!bEnableSwimWobble || !TroutMesh || !TroutMesh->GetStaticMesh())
	{
		return;
	}

	SwimWobbleTime += DeltaSeconds;
	const float Pitch =
		SwimWobblePitchAmplitudeDegrees * FMath::Sin(UE_TWO_PI * SwimWobbleFrequencyHz * SwimWobbleTime + SwimWobblePhase);
	TroutMesh->SetRelativeRotation(BaseTroutMeshRelativeRotation + FRotator(Pitch, 0.f, 0.f));
}
