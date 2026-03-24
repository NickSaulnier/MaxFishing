// Copyright MaxFishing Project

#include "MaxFishingWaterPlacement.h"
#include "WaterBodyActor.h"
#include "WaterBodyComponent.h"
#include "WaterBodyLakeActor.h"
#include "WaterBodyLakeComponent.h"
#include "WaterRuntimeSettings.h"
#include "WaterSplineComponent.h"
#include "WaterZoneActor.h"
#include "CollisionQueryParams.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerStart.h"
#include "LandscapeHeightfieldCollisionComponent.h"
#include "LandscapeProxy.h"
#include "Materials/MaterialInterface.h"
#include "WorldCollision.h"
#include "UObject/SoftObjectPath.h"

namespace MaxFishingWaterBootstrap
{
	static const FName RuntimeWaterTag(TEXT("MaxFishingRuntimeWater"));
}

namespace
{
	/**
	 * Small lift above sampled ground Z so the water surface is not coplanar / inside landscape collision
	 * (0 made the lake vanish; 300 looked obviously floating).
	 */
	static constexpr float WaterSurfaceAboveLandscapeCm = 12.f;
	/** Lake spline extent from actor origin (see MaxFishingConfigureLakeSplineAndBody). */
	static constexpr float DefaultLakeSplineRadiusCm = 15000.f;
	/** Horizontal gap between lake edge and PlayerStart so spawn is on land, not in water. */
	static constexpr float PlayerStartShoreStandoffCm = 1500.f;

	static void MaxFishingRebuildAllWaterZones(UWorld* World)
	{
		if (!World)
		{
			return;
		}
		for (TActorIterator<AWaterZone> ZIt(World); ZIt; ++ZIt)
		{
			ZIt->MarkForRebuild(EWaterZoneRebuildFlags::All, *ZIt);
			ZIt->Update();
		}
	}

	static TOptional<float> MaxFishingTryLandscapeZViaCollisionTrace(UWorld* World, float X, float Y)
	{
		if (!World)
		{
			return TOptional<float>();
		}

		FHitResult Hit;
		const FVector Start(X, Y, 200000.f);
		const FVector End(X, Y, -200000.f);
		FCollisionQueryParams Params(NAME_None, false);
		if (World->LineTraceSingleByChannel(Hit, Start, End, ECC_WorldStatic, Params))
		{
			if (Hit.Component.IsValid() && Hit.Component->IsA<ULandscapeHeightfieldCollisionComponent>())
			{
				return Hit.ImpactPoint.Z;
			}
		}
		return TOptional<float>();
	}

	static TOptional<float> MaxFishingTryLandscapeSurfaceZ(UWorld* World, float X, float Y)
	{
		if (!World)
		{
			return TOptional<float>();
		}

		// Prefer collision height: matches what geometry the water can end up "inside" when using heightfield-only Z.
		if (TOptional<float> TraceZ = MaxFishingTryLandscapeZViaCollisionTrace(World, X, Y))
		{
			return TraceZ;
		}

		const FVector Query(X, Y, 0.f);
		const bool bEditorLike = (World->WorldType == EWorldType::Editor) || (World->WorldType == EWorldType::EditorPreview);

		for (TActorIterator<ALandscapeProxy> LIt(World); LIt; ++LIt)
		{
			if (bEditorLike)
			{
				if (TOptional<float> Z = LIt->GetHeightAtLocation(Query, EHeightfieldSource::Editor))
				{
					return Z;
				}
			}
			if (TOptional<float> Z = LIt->GetHeightAtLocation(Query, EHeightfieldSource::Complex))
			{
				return Z;
			}
			if (TOptional<float> Z = LIt->GetHeightAtLocation(Query, EHeightfieldSource::Simple))
			{
				return Z;
			}
		}

		return TOptional<float>();
	}

	static void MaxFishingAlignLakeActorZAboveLandscape(UWorld* World, AWaterBodyLake* Lake)
	{
		if (!World || !Lake)
		{
			return;
		}

		FVector SampleXY = Lake->GetActorLocation();
		if (UWaterSplineComponent* Spline = Lake->GetWaterSpline())
		{
			SampleXY = Spline->Bounds.Origin;
		}

		const TOptional<float> TerrainZ = MaxFishingTryLandscapeSurfaceZ(World, SampleXY.X, SampleXY.Y);
		if (!TerrainZ.IsSet())
		{
			return;
		}

		const float NewZ = TerrainZ.GetValue() + WaterSurfaceAboveLandscapeCm;
		FVector Loc = Lake->GetActorLocation();
		Loc.Z = NewZ;
		Lake->SetActorLocation(Loc, false, nullptr, ETeleportType::TeleportPhysics);
	}

	static void MaxFishingEnsureLakeRootMobility(UWaterBodyComponent* Body)
	{
		if (!Cast<UWaterBodyLakeComponent>(Body))
		{
			return;
		}
		Body->SetMobility(EComponentMobility::Movable);
	}

	static void MaxFishingApplyDefaultLakeMaterialsIfNeeded(UWaterBodyComponent* Body)
	{
		if (!Cast<UWaterBodyLakeComponent>(Body))
		{
			return;
		}

		if (Body->GetWaterMaterial() && Body->GetWaterStaticMeshMaterial())
		{
			return;
		}

		static const FSoftObjectPath LakeWaterPath(TEXT("/Water/Materials/WaterSurface/Water_Material_Lake.Water_Material_Lake"));
		static const FSoftObjectPath LakeLodPath(TEXT("/Water/Materials/WaterSurface/LODs/Water_Material_Lake_LOD.Water_Material_Lake_LOD"));
		static const FSoftObjectPath UnderwaterPath(TEXT("/Water/Materials/PostProcessing/M_UnderWater_PostProcess_Volume.M_UnderWater_PostProcess_Volume"));
		static const FSoftObjectPath HlodPath(TEXT("/Water/Materials/HLOD/HLODWater.HLODWater"));

		if (!Body->GetWaterMaterial())
		{
			if (UMaterialInterface* M = Cast<UMaterialInterface>(LakeWaterPath.TryLoad()))
			{
				Body->SetWaterMaterial(M);
			}
		}
		if (!Body->GetWaterStaticMeshMaterial())
		{
			if (UMaterialInterface* M = Cast<UMaterialInterface>(LakeLodPath.TryLoad()))
			{
				Body->SetWaterStaticMeshMaterial(M);
			}
		}
		if (UMaterialInterface* Underwater = Cast<UMaterialInterface>(UnderwaterPath.TryLoad()))
		{
			Body->SetUnderwaterPostProcessMaterial(Underwater);
		}
		if (const UWaterRuntimeSettings* WaterSettings = GetDefault<UWaterRuntimeSettings>())
		{
			if (UMaterialInterface* Info = WaterSettings->GetDefaultWaterInfoMaterial())
			{
				Body->SetWaterInfoMaterial(Info);
			}
		}
		if (UMaterialInterface* Hlod = Cast<UMaterialInterface>(HlodPath.TryLoad()))
		{
			Body->SetHLODMaterial(Hlod);
		}

		Body->UpdateMaterialInstances();
	}

	static void MaxFishingRefreshExistingWater(UWorld* World)
	{
		if (!World)
		{
			return;
		}
		for (TActorIterator<AWaterBody> It(World); It; ++It)
		{
			if (AWaterBodyLake* LakeActor = Cast<AWaterBodyLake>(*It))
			{
				MaxFishingAlignLakeActorZAboveLandscape(World, LakeActor);
			}
			if (UWaterBodyComponent* Body = It->GetWaterBodyComponent())
			{
				MaxFishingEnsureLakeRootMobility(Body);
				MaxFishingApplyDefaultLakeMaterialsIfNeeded(Body);
				FOnWaterBodyChangedParams Params;
				Params.bShapeOrPositionChanged = true;
				Params.bUserTriggered = true;
				Body->UpdateAll(Params);
			}
		}
		MaxFishingRebuildAllWaterZones(World);
	}

	static FVector MaxFishingComputeDefaultWaterOrigin(UWorld* World)
	{
		FVector WaterOrigin = FVector(0.0, 0.0, 100.0);
		for (TActorIterator<APlayerStart> PSIt(World); PSIt; ++PSIt)
		{
			const FVector PS = PSIt->GetActorLocation();
			// Lake was centered on the player; default lake extends ~DefaultLakeSplineRadiusCm from center,
			// so spawn was underwater. Offset the lake west so PlayerStart sits on shore (+X = land).
			const float LakeWestOffsetX = DefaultLakeSplineRadiusCm + PlayerStartShoreStandoffCm;
			WaterOrigin.X = PS.X - LakeWestOffsetX;
			WaterOrigin.Y = PS.Y;
			WaterOrigin.Z = PS.Z - 20.0;
			break;
		}
		if (TOptional<float> TerrainZ = MaxFishingTryLandscapeSurfaceZ(World, WaterOrigin.X, WaterOrigin.Y))
		{
			WaterOrigin.Z = TerrainZ.GetValue() + WaterSurfaceAboveLandscapeCm;
		}
		return WaterOrigin;
	}

	static AWaterZone* MaxFishingSpawnWaterZone(UWorld* World, const FVector& WaterOrigin)
	{
		FActorSpawnParameters SpawnParams;
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

		AWaterZone* WaterZone = World->SpawnActor<AWaterZone>(AWaterZone::StaticClass(), WaterOrigin, FRotator::ZeroRotator, SpawnParams);
		if (WaterZone)
		{
			WaterZone->Tags.Add(MaxFishingWaterBootstrap::RuntimeWaterTag);
			WaterZone->SetActorLocation(WaterOrigin);
			WaterZone->SetZoneExtent(FVector2D(200000.0, 200000.0));
			WaterZone->MarkForRebuild(EWaterZoneRebuildFlags::All, WaterZone);
			WaterZone->Update();
		}
		return WaterZone;
	}

	static void MaxFishingConfigureLakeSplineAndBody(AWaterBodyLake* Lake)
	{
		if (!Lake)
		{
			return;
		}

		Lake->Tags.Add(MaxFishingWaterBootstrap::RuntimeWaterTag);

		if (UWaterSplineComponent* Spline = Lake->GetWaterSpline())
		{
			const float Radius = DefaultLakeSplineRadiusCm;
			Spline->ClearSplinePoints(false);
			Spline->AddSplinePoint(FVector(Radius, 0.0f, 0.0f), ESplineCoordinateSpace::Local, false);
			Spline->AddSplinePoint(FVector(0.0f, Radius, 0.0f), ESplineCoordinateSpace::Local, false);
			Spline->AddSplinePoint(FVector(-Radius, 0.0f, 0.0f), ESplineCoordinateSpace::Local, false);
			Spline->AddSplinePoint(FVector(0.0f, -Radius, 0.0f), ESplineCoordinateSpace::Local, false);
			Spline->SetClosedLoop(true, false);
			Spline->UpdateSpline();
			Spline->K2_SynchronizeAndBroadcastDataChange();
		}

		if (UWaterBodyComponent* Body = Lake->GetWaterBodyComponent())
		{
			MaxFishingEnsureLakeRootMobility(Body);
			MaxFishingApplyDefaultLakeMaterialsIfNeeded(Body);
			FOnWaterBodyChangedParams Params;
			Params.bShapeOrPositionChanged = true;
			Params.bUserTriggered = true;
			Body->UpdateAll(Params);
		}
	}

	static AWaterBodyLake* MaxFishingSpawnWaterLake(UWorld* World, const FVector& WaterOrigin)
	{
		FActorSpawnParameters SpawnParams;
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

		AWaterBodyLake* Lake = World->SpawnActor<AWaterBodyLake>(AWaterBodyLake::StaticClass(), WaterOrigin, FRotator::ZeroRotator, SpawnParams);
		MaxFishingConfigureLakeSplineAndBody(Lake);
		return Lake;
	}
}

void MaxFishingPlaceDefaultWater(UWorld* World)
{
	if (!World)
	{
		return;
	}

	bool bHasZone = false;
	for (TActorIterator<AWaterZone> ZoneIt(World); ZoneIt; ++ZoneIt)
	{
		bHasZone = true;
		break;
	}

	bool bHasLake = false;
	for (TActorIterator<AWaterBodyLake> LakeIt(World); LakeIt; ++LakeIt)
	{
		bHasLake = true;
		break;
	}

	if (bHasZone && bHasLake)
	{
		MaxFishingRefreshExistingWater(World);
		return;
	}

	const FVector WaterOrigin = MaxFishingComputeDefaultWaterOrigin(World);

	if (!bHasZone)
	{
		MaxFishingSpawnWaterZone(World, WaterOrigin);
	}

	if (!bHasLake)
	{
		MaxFishingSpawnWaterLake(World, WaterOrigin);
	}

	if (!bHasZone && bHasLake)
	{
		MaxFishingRefreshExistingWater(World);
		return;
	}

	MaxFishingRebuildAllWaterZones(World);
}
