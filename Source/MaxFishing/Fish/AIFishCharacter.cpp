// Copyright MaxFishing Project

#include "AIFishCharacter.h"
#include "FishAIController.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture2D.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialParameters.h"
#include "RHIShaderPlatform.h"
#include "UObject/UObjectGlobals.h"
#include "WaterBodyLakeActor.h"
#include "WaterBodyComponent.h"
#include "WaterBodyTypes.h"

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

namespace MaxFishingFishWaterPrivate
{
	static const FName RuntimeWaterTag(TEXT("MaxFishingRuntimeWater"));
	static constexpr float WaveQueryDepthCm = 10000.f;
}

void AAIFishCharacter::ApplyTroutMeshSwimOrientation()
{
	if (!TroutMesh)
	{
		return;
	}
	// Logs showed FRotator(-90,0,90) canonicalizes to (-90,90,0) (gimbal lock) -> absUpDot=0 (fish on side).
	// Compose two non-degenerate rotations in quaternion space: Z-up nose (+mesh Z) -> capsule +X, then +Y -> +Z (dorsal up).
	const FQuat Qy = FRotator(0.f, -90.f, 0.f).Quaternion();
	const FQuat Qx = FRotator(-90.f, 0.f, 0.f).Quaternion();
	const FQuat Frame = (Qx * Qy).GetNormalized();
	TroutMesh->SetRelativeRotation(Frame * TroutMeshBaseRotation.Quaternion());
}

bool AAIFishCharacter::ResolveWaterBody()
{
	if (CachedWaterBody.IsValid())
	{
		return true;
	}
	UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}
	AWaterBodyLake* Lake = nullptr;
	for (TActorIterator<AWaterBodyLake> It(World); It; ++It)
	{
		if (It->ActorHasTag(MaxFishingFishWaterPrivate::RuntimeWaterTag))
		{
			Lake = *It;
			break;
		}
	}
	if (!Lake)
	{
		for (TActorIterator<AWaterBodyLake> It(World); It; ++It)
		{
			Lake = *It;
			break;
		}
	}
	if (!Lake)
	{
		return false;
	}
	CachedWaterBody = Lake->GetWaterBodyComponent();
	LakeCenterWorld = Lake->GetActorLocation();
	return CachedWaterBody.IsValid();
}

float AAIFishCharacter::QueryWaterSurfaceZAtXY(float X, float Y) const
{
	UWaterBodyComponent* WB = CachedWaterBody.Get();
	if (!WB)
	{
		return LakeCenterWorld.Z;
	}
	FWaveInfo WaveInfo;
	WaveInfo.Normal = FVector::UpVector;
	const FVector QueryPos(X, Y, LakeCenterWorld.Z);
	if (WB->HasWaves() && WB->GetWaveInfoAtPosition(QueryPos, MaxFishingFishWaterPrivate::WaveQueryDepthCm, true, WaveInfo))
	{
		return LakeCenterWorld.Z + WaveInfo.Height;
	}
	return LakeCenterWorld.Z;
}

void AAIFishCharacter::PickNewSwimTarget()
{
	const float MaxR = FMath::Max(50.f, LakeSwimRadiusCm - LakeBoundaryMarginCm - 400.f);
	const float T = FMath::FRandRange(0.f, UE_TWO_PI);
	const float D = FMath::Sqrt(FMath::FRand()) * MaxR;
	const FVector2D Offset(FMath::Cos(T) * D, FMath::Sin(T) * D);
	SwimTargetWorld = LakeCenterWorld + FVector(Offset.X, Offset.Y, 0.f);
	const float SurfaceZ = QueryWaterSurfaceZAtXY(SwimTargetWorld.X, SwimTargetWorld.Y);
	SwimTargetWorld.Z = SurfaceZ - SwimDepthBelowSurfaceCm;
}

void AAIFishCharacter::TickWaterSwimming(float DeltaSeconds, UCharacterMovementComponent* Move)
{
	if (!Move || !bEnableWaterSwimming)
	{
		return;
	}
	if (!ResolveWaterBody())
	{
		return;
	}

	FVector Loc = GetActorLocation();
	const float MaxR = FMath::Max(50.f, LakeSwimRadiusCm - LakeBoundaryMarginCm);

	// Keep XY inside the circular lake footprint (matches spline lake used at runtime).
	{
		const FVector2D FromCenter(Loc.X - LakeCenterWorld.X, Loc.Y - LakeCenterWorld.Y);
		const float D = FromCenter.Size();
		if (D > MaxR)
		{
			const FVector2D Clamped = FromCenter.GetSafeNormal() * MaxR;
			Loc = FVector(LakeCenterWorld.X + Clamped.X, LakeCenterWorld.Y + Clamped.Y, Loc.Z);
			SetActorLocation(Loc, false, nullptr, ETeleportType::TeleportPhysics);
		}
	}

	const float SurfaceZHere = QueryWaterSurfaceZAtXY(Loc.X, Loc.Y);
	const float TargetZ = SurfaceZHere - SwimDepthBelowSurfaceCm;

	const FVector2D ToTarget2D(SwimTargetWorld.X - Loc.X, SwimTargetWorld.Y - Loc.Y);
	if (ToTarget2D.SizeSquared() < FMath::Square(SwimRetargetDistanceCm))
	{
		PickNewSwimTarget();
	}

	FVector ToTarget(SwimTargetWorld.X - Loc.X, SwimTargetWorld.Y - Loc.Y, 0.f);
	FVector Dir = ToTarget.GetSafeNormal();
	if (Dir.IsNearlyZero())
	{
		PickNewSwimTarget();
		ToTarget = FVector(SwimTargetWorld.X - Loc.X, SwimTargetWorld.Y - Loc.Y, 0.f);
		Dir = ToTarget.GetSafeNormal();
	}

	const float ZVel = FMath::Clamp((TargetZ - Loc.Z) * SwimZStabilize, -MaxVerticalSwimSpeedCm, MaxVerticalSwimSpeedCm);
	Move->Velocity = FVector(Dir.X * SwimSpeedCmPerSec, Dir.Y * SwimSpeedCmPerSec, ZVel);

	const float Yaw = FVector(Dir.X, Dir.Y, 0.f).Rotation().Yaw;
	SetActorRotation(FRotator(0.f, Yaw, 0.f));
}

AAIFishCharacter::AAIFishCharacter()
{
	PrimaryActorTick.bCanEverTick = true;
	// Before CharacterMovement so swim velocity applies the same frame.
	PrimaryActorTick.TickGroup = TG_PrePhysics;

	AIControllerClass = AFishAIController::StaticClass();
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;

	GetMesh()->SetVisibility(false);
	GetMesh()->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	TroutMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("TroutMesh"));
	TroutMesh->SetupAttachment(GetCapsuleComponent());
	TroutMesh->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
	// Keep mesh pivot near capsule center so we do not bury half the mesh under terrain when Z is shallow.
	TroutMesh->SetRelativeLocation(FVector::ZeroVector);
	ApplyTroutMeshSwimOrientation();
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

			ApplyTroutMeshSwimOrientation();
			BaseTroutMeshRelativeRotation = TroutMesh->GetRelativeRotation();
			SwimWobblePhase = FMath::FRandRange(0.f, UE_TWO_PI);
		}
	}

	if (bEnableWaterSwimming && ResolveWaterBody())
	{
		PickNewSwimTarget();
	}
}

void AAIFishCharacter::Tick(float DeltaSeconds)
{
	if (bEnableWaterSwimming)
	{
		if (UCharacterMovementComponent* Move = GetCharacterMovement())
		{
			TickWaterSwimming(DeltaSeconds, Move);
		}
	}

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
