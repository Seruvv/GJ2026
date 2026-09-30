// Hub screen: building markers over the 3D scene, a details panel for the selected building (level, effects,
// next upgrade, cost, conversions), the living-hamster roster, profile resources, the "В ПОХОД" button with a
// preview of the selected hamster, and secondary buttons. Everything shown comes from the hub catalog Data Asset
// and the active profile.

#pragma once

#include "CoreMinimal.h"
#include "../Meta/CRMetaTypes.h"
#include "../UI/CRMenuHUDBase.h"
#include "CRHubHUD.generated.h"

class ACRHubGameMode;
class UCRHubBuildingDefinition;
class UCRProfileSubsystem;
class UTexture2D;

UCLASS()
class CARDSROGUELIKE_API ACRHubHUD : public ACRMenuHUDBase
{
	GENERATED_BODY()

protected:
	virtual void DrawScreen() override;
	virtual void OnButton(const FCRUIButton& Button) override;
	virtual bool HandleScroll(const FVector2D& ScreenPos, float Delta) override;

private:
	/** Screen layout (recomputed each frame): scene on the left, building details, roster at the far right. */
	struct FHubLayout
	{
		FBox2D Details = FBox2D(ForceInit);
		FBox2D Roster = FBox2D(ForceInit);
		float SceneCenterX = 0.f;
	};
	FHubLayout ComputeLayout() const;

	void DrawMarkers(const UCRProfileSubsystem& Profiles, ACRHubGameMode& GM);
	void DrawDetails(const UCRProfileSubsystem& Profiles, const UCRHubBuildingDefinition& Building, const FBox2D& Panel);
	void DrawRoster(const UCRProfileSubsystem& Profiles, const FBox2D& Panel);
	void DrawPortrait(const FCRHamsterPersistentState& Hamster, const FBox2D& Rect);
	void DrawTopBar(const UCRProfileSubsystem& Profiles);
	void DrawBottomBar(const UCRProfileSubsystem& Profiles, float SceneCenterX);
	/** "Серебро 25 · Дерево 5" with each lacking resource in red. Returns the X after the text. */
	float DrawCost(const FCRMetaResources& Cost, const FCRMetaResources& Have, float X, float Y, float Scale);
	/** The hamster's portrait texture, or nullptr (then a placeholder is drawn). Loaded once per hamster. */
	UTexture2D* GetAvatarTexture(const FCRHamsterPersistentState& Hamster);

	FName SelectedBuildingId = TEXT("Heart");
	FName HoveredBuildingId;

	/** First visible roster row, and the largest valid value (from the last drawn frame). */
	int32 RosterScroll = 0;
	int32 RosterMaxScroll = 0;
	FBox2D RosterRect = FBox2D(ForceInit);

	/** Portraits looked up so far (a null value means "no texture, draw the placeholder"). */
	UPROPERTY(Transient)
	TMap<FName, TObjectPtr<UTexture2D>> AvatarCache;
};
