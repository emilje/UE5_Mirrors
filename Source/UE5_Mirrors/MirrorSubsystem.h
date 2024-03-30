#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "MirrorSubsystem.generated.h"

class UCameraComponent;
class ACMirror;

UCLASS()
class UE5_MIRRORS_API UMirrorSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	void OnMirrorCreated(ACMirror* NewMirror);
	void OnMirrorDestroyed(ACMirror* DestroyedMirror);

protected:
	UFUNCTION(BlueprintCallable)
	void UpdateActiveCamera(UCameraComponent* NewActiveCamera) const;

private:
	UPROPERTY()
	TArray<ACMirror*> WorldMirrors;
};
