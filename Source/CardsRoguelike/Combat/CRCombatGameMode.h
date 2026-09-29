// Prototype combat GameMode: scene setup, turn state machine, mana and card resolution.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "CRTypes.h"
#include "CRCombatGameMode.generated.h"

class ACRArena;
class ACRBarrel;
class ACREnemy;
class ACRHamster;
class ACRPit;

UCLASS()
class CARDSROGUELIKE_API ACRCombatGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ACRCombatGameMode();

	virtual void StartPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void RestartPlayer(AController* NewPlayer) override;
	virtual void HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer) override;

	// Player commands (from the controller)
	void SelectCard(int32 Index);
	void CancelTargeting();
	void HandleClick(AActor* HitActor, const FVector& WorldLocation);
	void RequestEndTurn();
	void CycleBoundaryType(int32 EdgeIndex);

	/** True when dropping card Index on this hit would be accepted (used for drag highlights). */
	bool IsValidCardDrop(int32 Index, const AActor* HitActor, const FVector& WorldLocation, bool bHitWorld) const;

	/**
	 * Commits a card dragged from the hand. Returns false (nothing spent) when the drop is invalid.
	 * PUSH only locks its target here; the direction click commits it.
	 */
	bool TryDropCard(int32 Index, AActor* HitActor, const FVector& WorldLocation, bool bHitWorld);

	bool IsAwaitingPushDirection() const;

	/** End Turn is available for the whole player turn, even with 0 mana. */
	bool CanEndTurn() const { return TurnState == ECRTurnState::PlayerTurn; }
	bool CanAffordCard(int32 Index) const;

	void ToggleDebugView();
	bool IsDebugView() const { return bDebugView; }

	/** Short player-facing message shown under the turn panel for a few seconds. */
	/** Shows a short player-facing (Russian) message; LogMessage, if given, is the English log line. */
	void ShowMessage(const FString& Message, const FString& LogMessage = FString());
	FString GetActiveMessage() const;

	// Combat events
	void OnEnemyEliminated(ACREnemy* Enemy, ECREliminationReason Reason);
	void OnBarrelRemoved(ACRBarrel* Barrel);
	void HandleBoundaryContact(AActor* Actor, ECRBoundaryType Type);
	void ApplyRadialBlast(const FVector& Origin, float Radius, int32 Damage, float Impulse, bool bDamageHamster, const FString& Source);
	void LogEvent(const FString& Message);

	// Queries
	ACRArena* GetArena() const { return Arena; }
	ACRHamster* GetHamster() const { return Hamster; }
	ECRTurnState GetTurnState() const { return TurnState; }
	FString GetTurnStateName() const;
	int32 GetMana() const { return Mana; }
	int32 GetTurnNumber() const { return TurnNumber; }
	int32 GetSelectedCardIndex() const { return SelectedCard; }
	AActor* GetPendingTarget() const { return PendingTarget.Get(); }
	int32 GetEnemyCount() const { return Enemies.Num(); }
	const TArray<TObjectPtr<ACREnemy>>& GetEnemies() const { return Enemies; }
	const TArray<FString>& GetEventLog() const { return EventLog; }
	FString GetTargetingPrompt() const;
	bool IsPhysicsTarget(const AActor* Actor) const;

	/** True when this combat is a room of an active run (not the standalone sandbox test). */
	bool IsRunIntegrated() const { return bRunIntegrated; }

	/** Run-integrated only: pause on the VICTORY/DEFEAT banner before returning to the run map. */
	UPROPERTY(EditAnywhere, Category = "CR|Run")
	float RunReturnDelay = 2.f;

	// Rewards (run-integrated victories only)

	/** Commits reward offer Index (0-2). Ignored outside the Reward state or after a commit. */
	void ChooseReward(int32 OfferIndex);
	void SkipReward();

	const TArray<FName>& GetRewardOffers() const { return RewardOffers; }
	const FCRCardDef* FindCardDef(FName CardId) const;
	bool IsRewardCommitted() const { return bRewardCommitted; }
	FName GetChosenReward() const { return ChosenReward; }

	/** Victory banner time before the reward panel opens. */
	UPROPERTY(EditAnywhere, Category = "CR|Reward")
	float RewardDelay = 1.5f;

	UPROPERTY(EditAnywhere, Category = "CR|Reward")
	int32 RewardOfferCount = 3;

	/** Placeholder balance: resources granted once per cleared combat room. */
	UPROPERTY(EditAnywhere, Category = "CR|Reward")
	int32 RewardSilver = 5;

	UPROPERTY(EditAnywhere, Category = "CR|Reward")
	int32 RewardFood = 1;

	UPROPERTY(EditAnywhere, Category = "CR|Reward")
	int32 RewardWood = 1;

	UPROPERTY(EditAnywhere, Category = "CR|Cards")
	TArray<FCRCardDef> Cards;

	UPROPERTY(EditAnywhere, Category = "CR|Turn")
	int32 ManaPerTurn = 3;

	/** Card Blast damages the hamster when true (barrel explosions always can). */
	UPROPERTY(EditAnywhere, Category = "CR|Cards")
	bool bCardBlastHurtsHamster = false;

	UPROPERTY(EditAnywhere, Category = "CR|Resolve")
	float SettleSpeed = 20.f;

	UPROPERTY(EditAnywhere, Category = "CR|Resolve")
	float SettleHoldTime = 0.3f;

	UPROPERTY(EditAnywhere, Category = "CR|Resolve")
	float MinResolveTime = 0.3f;

	/** Watchdog in simulated seconds; normal resolution ends when bodies settle. */
	UPROPERTY(EditAnywhere, Category = "CR|Resolve")
	float ResolveTimeout = 5.f;

	UPROPERTY(EditAnywhere, Category = "CR|Turn")
	float EnemyActionGap = 0.35f;

	/** Watchdog per enemy action, in simulated seconds. */
	UPROPERTY(EditAnywhere, Category = "CR|Turn")
	float EnemyActionTimeout = 4.f;

	UPROPERTY(EditAnywhere, Category = "CR|Setup")
	FCREnemyStats MeleeStats;

	UPROPERTY(EditAnywhere, Category = "CR|Setup")
	FCREnemyStats RangedStats;

	/** Arena-relative spawn points. */
	UPROPERTY(EditAnywhere, Category = "CR|Setup")
	TArray<FCREnemySpawn> EnemySpawns;

	UPROPERTY(EditAnywhere, Category = "CR|Setup")
	TArray<FVector2D> BarrelLocations;

	UPROPERTY(EditAnywhere, Category = "CR|Setup")
	FVector2D PitLocation = FVector2D(-250.f, 750.f);

	UPROPERTY(EditAnywhere, Category = "CR|Setup")
	FVector2D PitSize = FVector2D(220.f, 220.f);

	/** Enemies below this Z are eliminated as fallen. */
	UPROPERTY(EditAnywhere, Category = "CR|Setup")
	float KillZ = -500.f;

private:
	void SpawnCombatScene();
	void ApplyRunState();
	void OnCombatResolved(bool bVictory);
	void ReturnToRunMap();
	void EnterRewardState();
	void GrantCombatResources();
	void CommitReward(FName CardId);
	void StartPlayerTurn();
	void PlayCard(int32 Index, AActor* Target, const FVector& Point);
	void BeginResolving();
	/** Both take simulated physics time (see CRProto::GetSimulatedDeltaSeconds). */
	bool UpdateSettle(float SimDeltaSeconds);
	void TickEnemyTurn(float SimDeltaSeconds);
	void EnforceArenaBounds();
	void TeleportToInner(AActor* Actor);
	void RefreshIntents();
	bool CheckCombatEnd();
	void ClearTargeting();
	UPrimitiveComponent* GetPhysicsBody(AActor* Actor) const;
	FString DescribeActor(const AActor* Actor) const;

	UPROPERTY()
	TObjectPtr<ACRArena> Arena;

	UPROPERTY()
	TObjectPtr<ACRHamster> Hamster;

	UPROPERTY()
	TObjectPtr<ACRPit> Pit;

	UPROPERTY()
	TArray<TObjectPtr<ACREnemy>> Enemies;

	UPROPERTY()
	TArray<TObjectPtr<ACRBarrel>> Barrels;

	ECRTurnState TurnState = ECRTurnState::PlayerTurn;
	int32 Mana = 0;
	int32 TurnNumber = 0;
	int32 SelectedCard = INDEX_NONE;
	TWeakObjectPtr<AActor> PendingTarget;

	float ResolveElapsed = 0.f;
	float SettledTime = 0.f;

	TArray<TWeakObjectPtr<ACREnemy>> EnemyQueue;
	int32 EnemyIndex = 0;
	bool bEnemyActing = false;
	bool bEnemySettling = false;
	float EnemyPhaseTimer = 0.f;

	TArray<FString> EventLog;

	bool bDebugView = false;
	bool bRunIntegrated = false;
	FTimerHandle ReturnToRunTimer;
	FTimerHandle RewardTimer;

	/** Every prototype card definition, kept before the hand is rebuilt from the run deck. */
	UPROPERTY()
	TArray<FCRCardDef> CardCatalog;

	TArray<FName> RewardOffers;
	FName ChosenReward;
	bool bResourcesGranted = false;
	bool bRewardCommitted = false;
	FString FlashMessage;
	double FlashMessageTime = -100.0;
};
