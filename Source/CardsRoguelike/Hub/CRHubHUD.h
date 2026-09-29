// Hub screen: building markers over the 3D scene, a details panel for the selected building (level, effects,
// next upgrade, cost, conversions), profile resources, the "В ПОХОД" button and secondary buttons.
// Everything shown comes from the hub catalog Data Asset and the active profile.

#pragma once

#include "CoreMinimal.h"
#include "../Meta/CRMetaTypes.h"
#include "../UI/CRMenuHUDBase.h"
#include "CRHubHUD.generated.h"

class ACRHubGameMode;
class UCRHubBuildingDefinition;
class UCRProfileSubsystem;

UCLASS()
class CARDSROGUELIKE_API ACRHubHUD : public ACRMenuHUDBase
{
	GENERATED_BODY()

protected:
	virtual void DrawScreen() override;
	virtual void OnButton(const FCRUIButton& Button) override;

private:
	void DrawMarkers(const UCRProfileSubsystem& Profiles, ACRHubGameMode& GM);
	void DrawDetails(const UCRProfileSubsystem& Profiles, const UCRHubBuildingDefinition& Building);
	void DrawTopBar(const UCRProfileSubsystem& Profiles);
	void DrawBottomBar(const UCRProfileSubsystem& Profiles);
	/** "Серебро 25 · Дерево 5" with each lacking resource in red. Returns the X after the text. */
	float DrawCost(const FCRMetaResources& Cost, const FCRMetaResources& Have, float X, float Y, float Scale);

	FName SelectedBuildingId = TEXT("Heart");
	FName HoveredBuildingId;
};
