// Owns profiles and out-of-run progression. Lives on the GameInstance next to the run subsystem:
// menu and hub screens call it, and it listens for run ends to deliver run resources to the profile.
// Every change to a profile is saved immediately.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "CRProfileSaveGame.h"
#include "CRProfileSubsystem.generated.h"

class UCRRunSubsystem;

DECLARE_MULTICAST_DELEGATE(FCROnProfileChanged);

UCLASS()
class CARDSROGUELIKE_API UCRProfileSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// Profiles

	/** Profiles in creation order. */
	const TArray<FCRProfileSummary>& GetProfiles() const;
	/** Profile highlighted by default (the last one played), or empty. */
	FString GetLastSelectedProfileId() const;
	/** Suggested name for the next new profile ("Хранитель 2"). */
	FString MakeDefaultProfileName() const;

	/** Creates, saves and activates a new profile. Returns its id (empty on failure). */
	FString CreateProfile(const FString& DisplayName);
	/** Loads the profile and makes it active. */
	bool SelectProfile(const FString& ProfileId);
	/** Removes the profile's save and its index entry. Deactivates it if active. */
	bool DeleteProfile(const FString& ProfileId);

	bool HasActiveProfile() const { return ActiveProfile != nullptr; }
	const UCRProfileSaveGame* GetActiveProfile() const { return ActiveProfile; }
	/** Loads the last selected profile if none is active (e.g. the hub opened directly in the editor). */
	bool EnsureActiveProfile();

	// Hub progression (active profile)

	/** Hub definitions (Data Asset), or nullptr if the catalog asset is missing. */
	const UCRHubCatalog* GetCatalog() const;
	int32 GetBuildingLevel(const UCRHubBuildingDefinition& Building) const;
	ECRUpgradeStatus GetUpgradeStatus(const UCRHubBuildingDefinition& Building, const FCRHubBuildingLevel** OutNextLevel = nullptr) const;
	/** Pays and raises the building one level. OutMessage is player-facing feedback. */
	bool TryUpgrade(FName BuildingId, FString& OutMessage);
	/** Performs one of the building's current conversions. OutMessage is player-facing feedback. */
	bool TryConvert(FName BuildingId, FName ConversionId, FString& OutMessage);
	/** Bonuses the next run gets from the active profile's buildings. */
	FCRRunStartBonuses GetRunStartBonuses() const;

	// Hamsters (active profile)

	/** The living hamster that goes on the next run, or nullptr. */
	const FCRHamsterPersistentState* GetSelectedHamster() const;
	/** Selects a living hamster and saves. */
	bool SelectHamster(FName HamsterId);
	/** What the next run starts with: the selected hamster's stats plus hub bonuses (for the preview and the run). */
	bool GetNextRunStartConfig(FCRRunStartConfig& OutConfig) const;
	int32 GetLivingHamsterCount() const;

	// Graveyard (active profile)

	/** Dead hamsters not yet seen in the graveyard (the hub marks them as new). */
	int32 GetUnseenGraveCount() const;
	/** The player opened the graveyard: every current death counts as seen. */
	void MarkGraveyardSeen();

	// Recruitment (active profile)

	const UCRRecruitmentDefinition* GetRecruitment() const;
	/** True while the living roster is below the recruitment target. */
	bool CanRecruit() const;
	/** Hires a saved candidate (free in this prototype) and saves. OutMessage is player-facing feedback. */
	bool RecruitCandidate(FName CandidateId, FString& OutMessage);

	// Runs

	/**
	 * Starts a fresh run for the active profile with a living hamster (falls back to another living one if the
	 * selection is invalid). Refuses, starting nothing, when no living hamster exists. The caller opens the run map.
	 */
	bool StartRunFromHub();

	/** Broadcast after any change to the active profile (and when the active profile changes). */
	FCROnProfileChanged OnProfileChanged;

#if !UE_BUILD_SHIPPING
	/**
	 * Development (CR.Dev.KillAllLiving): kills every living hamster of the active profile, for empty-roster tests on
	 * a disposable profile. ConfirmDisplayName must equal the active profile's name. Returns the number killed.
	 */
	int32 DevKillAllLiving(const FString& ConfirmDisplayName);
#endif

private:
	void LoadIndex();
	bool SaveIndex();
	bool SaveActiveProfile();
	FCRProfileSummary* FindSummary(const FString& ProfileId);
	void RefreshUnlockedFlags();
	void HandleRunEnded(const FCRRunState& EndedRun);

	UPROPERTY()
	TObjectPtr<UCRProfileIndexSaveGame> Index;

	UPROPERTY()
	TObjectPtr<UCRProfileSaveGame> ActiveProfile;

	UPROPERTY()
	mutable TObjectPtr<UCRHubCatalog> CachedCatalog;

	FDelegateHandle RunEndedHandle;
};
