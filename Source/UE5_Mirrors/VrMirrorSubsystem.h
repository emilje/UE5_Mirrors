#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "VrMirrorSubsystem.generated.h"

class UCameraComponent;
class ACVrMirror;

UCLASS()
class UE5_MIRRORS_API UVrMirrorSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	void OnMirrorCreated(ACVrMirror* NewMirror);
	void OnMirrorDestroyed(ACVrMirror* DestroyedMirror);

protected:
	UFUNCTION(BlueprintCallable)
	void UpdateActiveCamera(UCameraComponent* NewActiveCamera) const;

	UFUNCTION(BlueprintCallable)
	void DestroyAllMirrors();

private:
	UPROPERTY()
	TArray<ACVrMirror*> WorldMirrors;
};
