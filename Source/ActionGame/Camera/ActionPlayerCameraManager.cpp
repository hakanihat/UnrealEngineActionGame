#include "Camera/ActionPlayerCameraManager.h"
#include "Engine/World.h"

namespace
{
	// Distinct noise seeds so each axis shakes independently.
	constexpr float SeedPitch = 11.3f;
	constexpr float SeedYaw = 47.9f;
	constexpr float SeedRoll = 93.1f;
	constexpr float SeedX = 131.7f;
	constexpr float SeedY = 173.5f;
	constexpr float SeedZ = 211.2f;
}

void AActionPlayerCameraManager::AddTrauma(float Amount)
{
	Trauma = FMath::Clamp(Trauma + Amount, 0.f, 1.f);
}

void AActionPlayerCameraManager::AddFOVKick(float Degrees)
{
	FOVKick = FMath::Clamp(FOVKick + Degrees, -MaxFOVKick, MaxFOVKick);
}

void AActionPlayerCameraManager::UpdateViewTarget(FTViewTarget& OutVT, float DeltaTime)
{
	Super::UpdateViewTarget(OutVT, DeltaTime);

	// Real time keeps shake speed stable during hitstop and slow motion.
	const float RealDelta = GetWorld() ? GetWorld()->DeltaRealTimeSeconds : DeltaTime;
	NoiseTime += RealDelta * ShakeFrequency;

	if (Trauma > 0.f)
	{
		const float Shake = Trauma * Trauma;

		const FRotator RotationOffset(
			MaxShakeRotation.Pitch * Shake * FMath::PerlinNoise1D(NoiseTime + SeedPitch),
			MaxShakeRotation.Yaw * Shake * FMath::PerlinNoise1D(NoiseTime + SeedYaw),
			MaxShakeRotation.Roll * Shake * FMath::PerlinNoise1D(NoiseTime + SeedRoll));

		const FVector LocalOffset(
			FMath::PerlinNoise1D(NoiseTime + SeedX),
			FMath::PerlinNoise1D(NoiseTime + SeedY),
			FMath::PerlinNoise1D(NoiseTime + SeedZ));

		OutVT.POV.Rotation += RotationOffset;
		OutVT.POV.Location += OutVT.POV.Rotation.RotateVector(LocalOffset * MaxShakeOffset * Shake);

		Trauma = FMath::Max(0.f, Trauma - TraumaDecayRate * RealDelta);
	}

	if (!FMath::IsNearlyZero(FOVKick, 0.01f))
	{
		OutVT.POV.FOV += FOVKick;
		FOVKick = FMath::FInterpTo(FOVKick, 0.f, RealDelta, FOVKickRecoverySpeed);
	}
}
