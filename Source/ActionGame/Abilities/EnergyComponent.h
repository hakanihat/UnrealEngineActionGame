#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "EnergyComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnEnergyChanged, float, NewEnergy, float, MaxEnergy);

/**
 * Shared resource for telekinetic powers (throws, levitation, self-launch, slam).
 * Regenerates after a short delay; melee hits refund energy via AddEnergy, which rewards
 * mixing blade and powers instead of spamming one tool.
 */
UCLASS(ClassGroup = (Abilities), meta = (BlueprintSpawnableComponent))
class ACTIONGAME_API UEnergyComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UEnergyComponent();

	/** Spends Amount if available. Returns false (and spends nothing) otherwise. */
	UFUNCTION(BlueprintCallable, Category = "Energy")
	bool TryConsume(float Amount);

	/** Continuous drain (levitation). Returns false once energy runs out. */
	bool Drain(float Amount);

	UFUNCTION(BlueprintCallable, Category = "Energy")
	void AddEnergy(float Amount);

	UFUNCTION(BlueprintPure, Category = "Energy")
	bool HasEnergy(float Amount) const { return Energy >= Amount; }

	UFUNCTION(BlueprintPure, Category = "Energy")
	float GetEnergy() const { return Energy; }

	UFUNCTION(BlueprintPure, Category = "Energy")
	float GetEnergyPercent() const { return MaxEnergy > 0.f ? Energy / MaxEnergy : 0.f; }

	UPROPERTY(BlueprintAssignable, Category = "Energy")
	FOnEnergyChanged OnEnergyChanged;

protected:
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Energy", meta = (ClampMin = "1"))
	float MaxEnergy = 100.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Energy", meta = (ClampMin = "0"))
	float RegenRate = 22.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Energy", meta = (ClampMin = "0"))
	float RegenDelay = 0.8f;

private:
	void SetEnergy(float NewEnergy);

	float Energy = 0.f;
	float LastSpendTime = -1000.f;
};
