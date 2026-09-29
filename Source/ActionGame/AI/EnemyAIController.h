#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "EnemyAIController.generated.h"

class AEnemyCharacter;

UENUM(BlueprintType)
enum class EEnemyAIState : uint8
{
	Idle,
	/** Circling the player at the preferred range, waiting for a token / cooldown. */
	Engage,
	/** Closing distance and performing the chosen attack. */
	Attack,
	/** Short breather after an attack: the player's window to punish. */
	Recover,
	Stunned,
	Dead
};

/**
 * Small, explicit state machine instead of a Behavior Tree: easy to read, debug and tune in
 * code, and it works without any AI assets. Key design rules it enforces:
 *  - Attack tokens: only a few enemies attack at once; the rest circle (readable crowds).
 *  - Recovery after every attack: guaranteed punish windows.
 *  - Commit: enemies track the player during wind-up but lock direction once the swing starts,
 *    so a well-timed sidestep always works.
 */
UCLASS()
class ACTIONGAME_API AEnemyAIController : public AAIController
{
	GENERATED_BODY()

public:
	AEnemyAIController();

	virtual void Tick(float DeltaTime) override;

	/** Makes the enemy fight immediately regardless of distance (used by arena spawns). */
	UFUNCTION(BlueprintCallable, Category = "AI")
	void ForceAggro();

	UFUNCTION(BlueprintPure, Category = "AI")
	EEnemyAIState GetAIState() const { return State; }

protected:
	virtual void OnUnPossess() override;

	/** How often the enemy picks a new circling position. */
	UPROPERTY(EditAnywhere, Category = "AI", meta = (ClampMin = "0.1"))
	float RepositionInterval = 2.f;

	/** How often the enemy considers starting an attack. Randomized +-50% to desync groups. */
	UPROPERTY(EditAnywhere, Category = "AI", meta = (ClampMin = "0.1"))
	float DecisionInterval = 0.6f;

	/** Give up closing in on the player after this long (then circle again). */
	UPROPERTY(EditAnywhere, Category = "AI", meta = (ClampMin = "0.5"))
	float MaxApproachTime = 4.f;

private:
	void EnterState(EEnemyAIState NewState);
	void TickEngage(float DeltaTime, AActor* Target, float Distance);
	void TickAttack(float DeltaTime, AActor* Target, float Distance);
	void TickRecover(float DeltaTime);

	void MoveToward(const FVector& Destination, float AcceptanceRadius);
	void MoveToTarget(AActor* Target, float AcceptanceRadius);
	void TickDirectMove();
	void PickStrafeLocation(const AActor* Target);
	void SetSpeed(float Speed);
	void ReleaseToken();

	AEnemyCharacter* GetEnemy() const;

	EEnemyAIState State = EEnemyAIState::Idle;
	bool bAggro = false;
	bool bEngagedOnce = false;
	bool bAttackStarted = false;
	bool bAttackCommitted = false;
	int32 PendingAttack = INDEX_NONE;
	int32 StrafeSide = 1;

	float StateTime = 0.f;
	float RepositionTimer = 0.f;
	float DecisionTimer = 0.f;
	float RecoverTimer = 0.f;

	/** Used when pathfinding fails (e.g. no navmesh): walk straight at the goal. */
	bool bDirectMove = false;
	FVector DirectMoveGoal = FVector::ZeroVector;
	float DirectMoveAcceptance = 0.f;
};
