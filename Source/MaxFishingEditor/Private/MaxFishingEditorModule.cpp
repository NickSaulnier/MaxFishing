// Copyright MaxFishing Project

#include "MaxFishingWaterPlacement.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Editor.h"
#include "Engine/World.h"
#include "Modules/ModuleManager.h"
#include "TimerManager.h"
#include "UObject/SoftObjectPath.h"

namespace MaxFishingEditorWater
{
	/** Scan /Water so Content Browser shows plugin assets without a manual folder force-load; load core materials into memory. */
	static void PreloadWaterPluginContent()
	{
		if (!FModuleManager::Get().IsModuleLoaded("AssetRegistry"))
		{
			return;
		}

		FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
		IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();
		TArray<FString> Paths;
		Paths.Add(TEXT("/Water"));
		AssetRegistry.ScanPathsSynchronous(Paths, /*bForceRescan*/ false);

		static const TCHAR* ObjectPaths[] = {
			TEXT("/Water/Materials/WaterSurface/Water_Material_Lake.Water_Material_Lake"),
			TEXT("/Water/Materials/WaterSurface/LODs/Water_Material_Lake_LOD.Water_Material_Lake_LOD"),
			TEXT("/Water/Materials/PostProcessing/M_UnderWater_PostProcess_Volume.M_UnderWater_PostProcess_Volume"),
			TEXT("/Water/Materials/HLOD/HLODWater.HLODWater"),
			TEXT("/Water/Materials/MPC/MPC_Water.MPC_Water"),
			TEXT("/Water/Materials/WaterInfo/DrawWaterInfo.DrawWaterInfo"),
		};
		for (const TCHAR* Path : ObjectPaths)
		{
			FSoftObjectPath(Path).TryLoad();
		}
	}
}

class FMaxFishingEditorModule final : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		MaxFishingEditorWater::PreloadWaterPluginContent();

		OnMapOpenedHandle = FEditorDelegates::OnMapOpened.AddRaw(this, &FMaxFishingEditorModule::HandleMapOpened);
		TryPlaceWaterInEditorWorld();
	}

	virtual void ShutdownModule() override
	{
		FEditorDelegates::OnMapOpened.Remove(OnMapOpenedHandle);
		OnMapOpenedHandle = FDelegateHandle();
	}

private:
	void HandleMapOpened(const FString& /*Filename*/, bool /*bLoadAsTemplate*/)
	{
		TryPlaceWaterInEditorWorld();
	}

	void TryPlaceWaterInEditorWorld()
	{
		if (!GEditor)
		{
			return;
		}

		UWorld* World = GEditor->GetEditorWorldContext().World();
		if (!World || !World->IsEditorWorld() || World->IsPreviewWorld())
		{
			return;
		}

		// Next tick: landscape collision / WP cells are ready for GetHeightAtLocation; water zone composite updates after landscape.
		World->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateLambda(
			[WeakWorld = TWeakObjectPtr<UWorld>(World)]()
			{
				if (UWorld* W = WeakWorld.Get())
				{
					MaxFishingPlaceDefaultWater(W);
					if (GEditor)
					{
						GEditor->RedrawAllViewports();
					}
				}
			}));
	}

	FDelegateHandle OnMapOpenedHandle;
};

IMPLEMENT_MODULE(FMaxFishingEditorModule, MaxFishingEditor);
