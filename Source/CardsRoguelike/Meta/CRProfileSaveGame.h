// Profile persistence (UE SaveGame slots):
//  - "CR_ProfileIndex": list of profiles (metadata only) and the last selected profile id.
//  - "CR_Profile_<ProfileId>": one profile's progression (resources, building levels, flags, stats).
// Every save carries a version so later milestones can migrate old data instead of discarding it.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "CRMetaTypes.h"
#include "CRProfileSaveGame.generated.h"

/** Metadata shown in the profile list (kept in the index so the menu never loads every profile). */
USTRUCT(BlueprintType)
struct FCRProfileSummary
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Profile")
	FString ProfileId;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Profile")
	FString DisplayName;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Profile")
	FDateTime CreatedAt;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Profile")
	FDateTime LastPlayedAt;
};

UCLASS()
class CARDSROGUELIKE_API UCRProfileIndexSaveGame : public USaveGame
{
	GENERATED_BODY()

public:
	UPROPERTY()
	int32 SaveVersion = 1;

	UPROPERTY()
	TArray<FCRProfileSummary> Profiles;

	/** Profile highlighted in the menu on the next launch. */
	UPROPERTY()
	FString LastSelectedProfileId;

	/** Numbering for default names and ids ("Хранитель 3"). */
	UPROPERTY()
	int32 NextProfileNumber = 1;
};

/** Outcome of the last finished run, shown in the hub. */
USTRUCT(BlueprintType)
struct FCRRunEndSummary
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Profile")
	bool bValid = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Profile")
	ECRRunEndReason Reason = ECRRunEndReason::Completed;

	/** Resources the hamster carried when the run ended. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Profile")
	FCRMetaResources Carried;

	/** Resources delivered to the profile (after the keep percent). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Profile")
	FCRMetaResources Delivered;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Profile")
	int32 RunSeed = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Profile")
	int32 RoomsVisited = 0;

	/** Who went on the run (version 3). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Profile")
	FName HamsterId;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Profile")
	FString HamsterName;

	/** Failed runs: what killed the hamster. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Profile")
	ECRHamsterDeathCause DeathCause = ECRHamsterDeathCause::None;
};

USTRUCT(BlueprintType)
struct FCRProfileStats
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Profile")
	int32 RunsStarted = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Profile")
	int32 RunsCompleted = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Profile")
	int32 RunsFailed = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Profile")
	int32 RunsAbandoned = 0;
};

UCLASS()
class CARDSROGUELIKE_API UCRProfileSaveGame : public USaveGame
{
	GENERATED_BODY()

public:
	UPROPERTY(VisibleAnywhere, Category = "Profile")
	int32 SaveVersion = 1;

	UPROPERTY(VisibleAnywhere, Category = "Profile")
	FString ProfileId;

	UPROPERTY(VisibleAnywhere, Category = "Profile")
	FString DisplayName;

	UPROPERTY(VisibleAnywhere, Category = "Profile")
	FDateTime CreatedAt;

	UPROPERTY(VisibleAnywhere, Category = "Profile")
	FDateTime LastPlayedAt;

	UPROPERTY(VisibleAnywhere, Category = "Profile")
	FCRMetaResources Resources;

	/** Hub building levels by BuildingId (missing = the building's StartLevel). */
	UPROPERTY(VisibleAnywhere, Category = "Profile")
	TMap<FName, int32> BuildingLevels;

	/** Unlocked hub systems / flags (recomputed from building levels, plus any granted elsewhere). */
	UPROPERTY(VisibleAnywhere, Category = "Profile")
	TArray<FName> UnlockedFlags;

	/** Free-form counters for future systems (achievements, journal...). */
	UPROPERTY(VisibleAnywhere, Category = "Profile")
	TMap<FName, int32> Counters;

	UPROPERTY(VisibleAnywhere, Category = "Profile")
	FCRProfileStats Stats;

	UPROPERTY(VisibleAnywhere, Category = "Profile")
	FCRRunEndSummary LastRun;

	// Version 2: hamster roster.

	/** Set once the default roster was given to this profile (at creation, or when migrating a version 1 save). */
	UPROPERTY(VisibleAnywhere, Category = "Profile")
	bool bHamsterRosterInitialized = false;

	/** Every hamster the profile owns, living or dead. */
	UPROPERTY(VisibleAnywhere, Category = "Profile")
	TArray<FCRHamsterPersistentState> Hamsters;

	/** The living hamster that goes on the next run. */
	UPROPERTY(VisibleAnywhere, Category = "Profile")
	FName SelectedHamsterId;

	// Version 3: permanent death and recruitment.

	/** RunId of the last run end applied to this profile (a run's end is applied exactly once). */
	UPROPERTY(VisibleAnywhere, Category = "Profile")
	FString LastAppliedRunId;

	/**
	 * Recruitment candidates, created complete (stats, portrait, secret epitaph) and saved so they never
	 * reroll by reloading. Recruiting one moves it into Hamsters and creates a replacement candidate.
	 */
	UPROPERTY(VisibleAnywhere, Category = "Profile")
	TArray<FCRHamsterPersistentState> RecruitCandidates;

	/** Running number for recruit fallback names and generation seeds. */
	UPROPERTY(VisibleAnywhere, Category = "Profile")
	int32 NextRecruitNumber = 1;

	/** Dead hamsters the player has already seen in the graveyard (the hub marks newer deaths). */
	UPROPERTY(VisibleAnywhere, Category = "Profile")
	int32 GraveyardSeenCount = 0;
};

namespace CRMeta
{
	inline const TCHAR* ProfileIndexSlot() { return TEXT("CR_ProfileIndex"); }
	inline FString ProfileSlot(const FString& ProfileId) { return FString(TEXT("CR_Profile_")) + ProfileId; }
	constexpr int32 SaveUserIndex = 0;
	/** Profile save version written by this build (3: permanent death, graveyard, recruitment). */
	constexpr int32 CurrentProfileSaveVersion = 3;

	/** Per-hamster counters in FCRHamsterPersistentState::Stats. */
	inline FName HamsterStatRunsStarted() { return TEXT("RunsStarted"); }
	inline FName HamsterStatRunsCompleted() { return TEXT("RunsCompleted"); }

	FCRHamsterPersistentState* FindHamster(UCRProfileSaveGame& Profile, FName HamsterId);
	const FCRHamsterPersistentState* FindHamster(const UCRProfileSaveGame& Profile, FName HamsterId);
	/** The selected hamster if it is alive, otherwise nullptr. */
	const FCRHamsterPersistentState* FindSelectedHamster(const UCRProfileSaveGame& Profile);

	/** Living hamsters in roster order (the only ones that can be selected). */
	TArray<const FCRHamsterPersistentState*> GetLivingHamsters(const UCRProfileSaveGame& Profile);
	int32 CountLivingHamsters(const UCRProfileSaveGame& Profile);
	/** Dead hamsters, most recent death first. */
	TArray<const FCRHamsterPersistentState*> GetGraveyard(const UCRProfileSaveGame& Profile);
	/** The epitaph, only for a dead hamster (empty while alive): the one way UI reads epitaphs. */
	FString GetRevealedEpitaph(const FCRHamsterPersistentState& Hamster);
	/** Dead hamsters the player has not seen in the graveyard yet. */
	int32 CountUnseenGraves(const UCRProfileSaveGame& Profile);

	/**
	 * Marks the hamster dead with its death record and moves the selection to a living hamster (or clears it).
	 * Returns false if the hamster is unknown or already dead (a death is recorded only once).
	 */
	bool KillHamster(UCRProfileSaveGame& Profile, FName HamsterId, const FCRHamsterDeathRecord& Record);

	/** Keep percent of a run end reason from the catalog (built-in defaults without a catalog). */
	int32 GetKeepPercent(const UCRHubCatalog* Catalog, ECRRunEndReason Reason);

	/**
	 * Applies a finished run to its profile exactly once (keyed by the run's RunId): stats, delivered resources,
	 * LastRun, per-hamster counters, and for a failed run the hamster's permanent death with 0 delivered.
	 * Returns false (changing nothing) if this run was already applied. The caller saves.
	 */
	bool ApplyRunEnd(UCRProfileSaveGame& Profile, const UCRHubCatalog* Catalog, const FCRRunState& EndedRun, const FDateTime& Now);

	/** Counts the run start on the hamster that goes (per-hamster lifetime counter). */
	void RecordHamsterRunStart(UCRProfileSaveGame& Profile, FName HamsterId);

	// Recruitment

	/** Whether the profile may recruit now (fewer living hamsters than the target). */
	bool CanRecruit(const UCRProfileSaveGame& Profile, const UCRRecruitmentDefinition* Recruitment);

	/**
	 * Creates one new candidate: unique generated id, a name not used by any hamster or candidate of the profile
	 * (fallback pattern once the pool is exhausted), a stat template, a tint and its epitaph (assigned now).
	 */
	FCRHamsterPersistentState MakeRecruitCandidate(UCRProfileSaveGame& Profile, const UCRRecruitmentDefinition& Recruitment, FRandomStream& Stream);

	/** Tops the candidate list up to CandidateCount. Returns true if candidates were added. */
	bool RefillRecruitCandidates(UCRProfileSaveGame& Profile, const UCRRecruitmentDefinition* Recruitment);

	/**
	 * Moves a candidate into the roster as a new living hamster, replaces it with a new candidate and selects it
	 * if no living hamster was selected. Fails (changing nothing) if recruiting is not allowed or the id is unknown.
	 */
	bool RecruitCandidate(UCRProfileSaveGame& Profile, const UCRRecruitmentDefinition* Recruitment, FName CandidateId);

	/** Gives the profile the catalog's default roster if it has none yet. Returns true if hamsters were added. */
	bool InitializeHamsterRoster(UCRProfileSaveGame& Profile, const UCRHubCatalog* Catalog);

	/** Makes SelectedHamsterId point at a living hamster (the first one if it is invalid). Returns true if it changed. */
	bool EnsureValidHamsterSelection(UCRProfileSaveGame& Profile);

	/**
	 * Brings an older save up to date without touching its existing data (resources, buildings, stats, roster...):
	 * version 1 saves get the default roster exactly once; version 2 saves get recruitment candidates (every
	 * existing hamster stays as it was). Returns true if anything changed (the caller saves).
	 */
	bool MigrateProfile(UCRProfileSaveGame& Profile, const UCRHubCatalog* Catalog);
}
