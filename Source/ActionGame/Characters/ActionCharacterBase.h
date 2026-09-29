#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "GameplayTagAssetInterface.h"
#include "GameplayTagContainer.h"
#include "Combat/CombatantInterface.h"
#include "Combat/CombatTypes.h"
#include "ActionCharacterBase.generated.h"

class UHealthComponent;
class UHitReactionComponent;
class UMeleeComponent;
class UPhysicalAnimationComponent;
class UStaticMeshComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnDamageDealt, const FCombatHit&, Hit, const FCombatDamageResult&, Result);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnStateTagChanged, FGameplayTag, Tag, bool, bAdded);

/**
 * Shared base for the player, enemies and bosses. Owns what every combatant needs:
 * health/poise, hit reactions, melee, a team, and a counted set of state tags.
 *
 * State tags are reference-counted so two systems can hold the same state
 * (e.g. dodge i-frames and post-hit mercy both granting State.Invulnerable)
 * without one accidentally clearing the other.
 */
UCLASS(Abstract)
class ACTIONGAME_API AActionCharacterBase : public ACharacter, public ICombatant, public IGameplayTagAssetInterface
{
	GENERATED_BODY()

public:
	AActionCharacterBase(const FObjectInitializer& ObjectInitializer);

	// ICombatant
	virtual ECombatTeam GetCombatTeam() const override { return Team; }
	virtual FVector GetTargetPoint() const override;
	virtual void NotifyDamageDealt(const FCombatHit& Hit, const FCombatDamageResult& Result) override;

	// IGameplayTagAssetInterface
	virtual void GetOwnedGameplayTags(FGameplayTagContainer& TagContainer) const override { TagContainer = ActiveStateTags; }

	UFUNCTION(BlueprintCallable, Category = "State")
	void AddStateTag(FGameplayTag Tag);

	UFUNCTION(BlueprintCallable, Category = "State")
	void RemoveStateTag(FGameplayTag Tag);

	UFUNCTION(BlueprintPure, Category = "State")
	bool HasStateTag(FGameplayTag Tag) const { return ActiveStateTags.HasTagExact(Tag); }

	UFUNCTION(BlueprintPure, Category = "State")
	bool IsAlive() const;

	/** False while dead, stunned, staggered or knocked down. Gate for every voluntary action. */
	UFUNCTION(BlueprintPure, Category = "State")
	bool CanAct() const;

	/** Receives gameplay events fired by UAnimNotify_CombatEvent (telegraphs, projectile spawns...). */
	virtual void HandleCombatEvent(FGameplayTag EventTag) {}

	UHealthComponent* GetHealthComponent() const { return HealthComponent; }
	UHitReactionComponent* GetHitReactionComponent() const { return HitReactionComponent; }
	UMeleeComponent* GetMeleeComponent() const { return MeleeComponent; }

	UPROPERTY(BlueprintAssignable, Category = "Combat")
	FOnDamageDealt OnDamageDealt;

	UPROPERTY(BlueprintAssignable, Category = "State")
	FOnStateTagChanged OnStateTagChanged;

protected:
	virtual void BeginPlay() override;

	UFUNCTION()
	virtual void HandleDeath(AActor* DeadActor, const FCombatHit& KillingHit);

	/** Called whenever a hit interrupts this character. Override to cancel abilities. */
	UFUNCTION()
	virtual void HandleInterrupted(EHitReaction Reaction, bool bStaggered);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UHealthComponent> HealthComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UHitReactionComponent> HitReactionComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UMeleeComponent> MeleeComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UPhysicalAnimationComponent> PhysicalAnimationComponent;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat")
	ECombatTeam Team = ECombatTeam::Enemy;

	/** Colour of the placeholder body shown when no skeletal mesh is assigned. */
	UPROPERTY(EditAnywhere, Category = "Placeholder")
	FLinearColor PlaceholderColor = FLinearColor(0.8f, 0.1f, 0.1f);

	/** Bone used as the aim / lock-on point. Falls back to the capsule center if missing. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat")
	FName TargetPointBone = TEXT("spine_03");

private:
	/** Shows a simple capsule body when no character mesh is set, so the game is playable with zero art. */
	void CreatePlaceholderBodyIfNeeded();

	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> PlaceholderBody;

	TMap<FGameplayTag, int32> StateTagCounts;

	UPROPERTY(VisibleInstanceOnly, Category = "State")
	FGameplayTagContainer ActiveStateTags;
};
