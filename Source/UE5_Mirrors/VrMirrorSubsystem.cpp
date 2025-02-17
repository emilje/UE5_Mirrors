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
			Mirror->SceneCaptureLeftEye->HiddenActors.Remove(DestroyedMirror);
			Mirror->SceneCaptureRightEye->HiddenActors.Remove(DestroyedMirror);
		}
	}
}

void UVrMirrorSubsystem::DestroyAllMirrors()
{
	TArray<ACVrMirror*> MirrorsForDestruction;
	for (const auto Mirror : WorldMirrors)
	{
		if (Mirror)
		{
			MirrorsForDestruction.Add(Mirror);
		}
	}

	for (const auto Mirror : MirrorsForDestruction)
	{
		if (Mirror)
		{
			Mirror->Destroy();
		}
	}

	WorldMirrors.Empty();
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
