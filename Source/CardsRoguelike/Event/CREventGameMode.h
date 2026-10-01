// Event room GameMode: validates the run, loads the node's persistent event state and its authored
// Data Asset, spawns the illustration and drives choice -> (card sacrifice) -> result -> continue.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "CREventGameMode.generated.h"

class ACREventRoomActor;
class UCREventDefinition;
class UCRRunSubsystem;
struct FCREventNodeState;

enum class ECREventScreen : uint8
{
	/** Direct sandbox open or broken event data: nothing is interactive. */
	Invalid,
	Choices,
	/** A choice with a RemoveSelectedCard effect waits for the player to pick the card. */
	CardSelect,
	Result
};

UCLASS()
class CARDSROGUELIKE_API ACREventGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ACREventGameMode();

	virtual void StartPlay() override;
	virtual void RestartPlayer(AController* NewPlayer) override;

	// Player actions (each is ignored unless the matching screen is active)
	void ChooseOption(int32 ChoiceIndex);
	void SelectSacrifice(int32 DeckIndex);
	void CancelCardSelection();
	void Continue();

	// Queries for the HUD
	ECREventScreen GetScreen() const;
	const UCREventDefinition* GetEvent() const { return Event; }
	const FCREventNodeState* GetEventState() const;
	FString GetInvalidReason() const { return InvalidReason; }
	int32 GetPendingChoice() const { return PendingChoice; }
	/** Why a choice is disabled ("Need 2 Silver"), or empty when it can be taken. */
	FString GetChoiceBlockReason(int32 ChoiceIndex) const;
	const TArray<FName>& GetDeck() const;
	int32 GetHP() const;
	int32 GetMaxHP() const;
	int32 GetSilver() const;
	int32 GetFood() const;
	int32 GetWood() const;
	bool IsContinuing() const { return bContinuing; }
	/** True once the committed choice killed the hamster (the run failed). */
	bool IsHamsterDead() const;
	/** Hamster name of the current run. */
	FString GetHamsterName() const;
	FString GetActiveMessage() const;

private:
	UCRRunSubsystem* GetRun() const;
	void SetupPresentation();
	void ShowMessage(const FString& Message);

	UPROPERTY()
	TObjectPtr<ACREventRoomActor> Room;

	/** Loaded definition of the node's selected event (kept alive while the room is open). */
	UPROPERTY()
	TObjectPtr<UCREventDefinition> Event;

	FName EventNodeId;
	bool bValidEvent = false;
	FString InvalidReason;
	int32 PendingChoice = INDEX_NONE;
	bool bContinuing = false;
	FString Message;
	double MessageTime = -100.0;
};
