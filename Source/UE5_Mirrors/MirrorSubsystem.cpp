#include "MirrorSubsystem.h"
#include "CMirror.h"
#include "Components/SceneCaptureComponent2D.h"

void UMirrorSubsystem::OnMirrorCreated(ACMirror* NewMirror)
{
	WorldMirrors.Add(NewMirror);
	NewMirror->SceneCapture->HiddenActors.Append(WorldMirrors);

	for (const auto Mirror : WorldMirrors)
	{
		if (Mirror)
		{
			Mirror->SceneCapture->HiddenActors.AddUnique(NewMirror);
		}
	}
}

void UMirrorSubsystem::OnMirrorDestroyed(ACMirror* DestroyedMirror)
{
	WorldMirrors.Remove(DestroyedMirror);
	for (const auto Mirror : WorldMirrors)
	{
		if (Mirror)
		{
			Mirror->SceneCapture->HiddenActors.Remove(Mirror);
		}
	}
}

void UMirrorSubsystem::UpdateActiveCamera(UCameraComponent* NewActiveCamera) const
{
	for (const auto Mirror : WorldMirrors)
	{
		if (Mirror)
		{
			Mirror->ActiveCamera = NewActiveCamera;
			Mirror->Init();
		}
	}
	
	if (GEngine)
	{
		GEngine->ForceGarbageCollection();
	}
}
