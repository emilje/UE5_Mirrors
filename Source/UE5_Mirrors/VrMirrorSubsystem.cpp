#include "VrMirrorSubsystem.h"
#include "CVrMirror.h"
#include "Components/SceneCaptureComponent2D.h"

void UVrMirrorSubsystem::OnMirrorCreated(ACVrMirror* NewMirror)
{
	WorldMirrors.Add(NewMirror);
	NewMirror->SceneCaptureLeftEye->HiddenActors.Append(WorldMirrors);
	NewMirror->SceneCaptureRightEye->HiddenActors.Append(WorldMirrors);

	for (const auto Mirror : WorldMirrors)
	{
		if (Mirror)
		{
			Mirror->SceneCaptureLeftEye->HiddenActors.AddUnique(NewMirror);
			Mirror->SceneCaptureRightEye->HiddenActors.AddUnique(NewMirror);
		}
	}
}

void UVrMirrorSubsystem::OnMirrorDestroyed(ACVrMirror* DestroyedMirror)
{
	WorldMirrors.Remove(DestroyedMirror);
	for (const auto Mirror : WorldMirrors)
	{
		if (Mirror)
		{
			Mirror->SceneCaptureLeftEye->HiddenActors.Remove(Mirror);
			Mirror->SceneCaptureRightEye->HiddenActors.Remove(Mirror);
		}
	}
}

void UVrMirrorSubsystem::UpdateActiveCamera(UCameraComponent* NewActiveCamera) const
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
