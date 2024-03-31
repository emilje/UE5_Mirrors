#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CMirror.generated.h"

class ATriggerBox;
class UCameraComponent;

UCLASS()

class UE5_MIRRORS_API ACMirror : public AActor
{
	GENERATED_BODY()

public:
	ACMirror();
	virtual void Tick(const float DeltaTime) override;
	void Init();

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TObjectPtr<USceneCaptureComponent2D> SceneCapture;

	FVector2D Resolution = FVector2D::ZeroVector;
	TObjectPtr<UCameraComponent> ActiveCamera;

protected:
	virtual void BeginPlay() override;

	// Will cull objects that should not be seen in the reflection
	UPROPERTY(EditAnywhere, meta=(DisplayName="Culling"))
	bool bCullingEnabled = false;

	// Visualize culling planes. Only for debugging.
	UPROPERTY(EditAnywhere, meta=(EditCondition=bCullingEnabled), DisplayName="Display culling planes")
	bool bShowCullingPlanes = false;

	// Trace channel for mirror culling. Select your custom trace channel.
	UPROPERTY(EditAnywhere)
	TEnumAsByte<ECollisionChannel> MirrorCullingTraceChannel;

	// How far will the mirror scan for objects that will be rendered.
	UPROPERTY(EditAnywhere)
	float MirrorCullingTraceDistance = 10000;

	// Increasing this will increase the mirror's fov used for culling. If you find objects disappearing while they should still be visible in the reflection, try slightly increasing this number.
	UPROPERTY(EditAnywhere, meta=(ClampMin=1, ClampMax=2))
	float MirrorCullingBufferMultiplier = 1;

	// Use this for far away actors which might not be hit by the trace. Skybox is a good example.
	UPROPERTY(EditAnywhere)
	TArray<AActor*> DontCullActors;

	// Resolution capture multiplier. 1 for full resolution capture.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(ClampMin=0.1, ClampMax=1))
	float CaptureQuality = 1;

	// Captures won't trigger when this distance is exceeded. Think of having a mirror inside a room. Setting this to the room's size will make it so that captures are not triggerred when outside of the room.
	UPROPERTY(EditAnywhere)
	float CaptureMaxDistance = 5000;

	// Will decrease capture resolution as camera gets further from the mirror.
	UPROPERTY(EditAnywhere)
	bool bEnableDynamicCaptureResolution = false;

	// How often do we check and adjust the capture resolution in seconds.
	UPROPERTY(EditAnywhere, meta=(EditCondition=bEnableDynamicCaptureResolution, ClampMin=0))
	float DynamicCaptureCheckInterval = 1;

	// Capture resolution will reach this quality at DynamicCaptureRangeEnd.
	UPROPERTY(EditAnywhere, meta=(EditCondition=bEnableDynamicCaptureResolution, ClampMin=0.1))
	float LowestDynamicCaptureQuality = 0.5;

	// Capture resolution will be at maximum quality within this distance.
	UPROPERTY(EditAnywhere, meta=(EditCondition=bEnableDynamicCaptureResolution))
	float DynamicCaptureRangeStart = 500;

	// Capture resolution will be at lowest quality at and beyond this distance.
	UPROPERTY(EditAnywhere, meta=(EditCondition=bEnableDynamicCaptureResolution))
	float DynamicCaptureRangeEnd = 2500;
	// Capture resolution will be at lowest quality at and beyond this distance.
	UPROPERTY(EditAnywhere, meta=(EditCondition=bEnableDynamicCaptureResolution))
	bool bDisplayDynamicCaptureQuality = false;

	// Mirror will capture as long as we are within one of these boxes.
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<TObjectPtr<ATriggerBox>> CaptureTriggers;

	// Display number of active triggers for this mirror.
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bDisplayNumOfActiveTriggers = false;
	
	UPROPERTY()
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY()
	TObjectPtr<UTextureRenderTarget2D> RenderTarget;

	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> MaterialInstanceDynamic;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TObjectPtr<UStaticMeshComponent> MirrorMesh;

	UPROPERTY(EditAnywhere)
	TObjectPtr<UMaterial> MirrorMaterial;

private:
	virtual void Destroyed() override;
	void OnViewportResize(FViewport* Viewport, uint32);
	void CaptureScene();
	void CheckDynamicResolution();
	void MirrorCulling(FVector& MirroredCameraLocation);
	bool ShouldSkipCapture() const;
	FTransform MirrorCamera(const FTransform& CameraTransform) const;
	FVector2D CalcRenderTargetResolution() const;
	void FindActiveCamera();
	void SetupCaptureTriggers();

	float InitialCaptureQuality;
	int32 HorizontalFov;
	FVector LastTraceCameraPosition = FVector::ZeroVector;
	bool bIsUsingCaptureTriggers = false;
	int32 NumActiveCaptureTriggers = 0;

	UFUNCTION()
	void OnCaptureTriggerBeginOverlap(AActor* OverlappedActor, AActor* OtherActor);
	UFUNCTION()
	void OnCaptureTriggerEndOverlap(AActor* OverlappedActor, AActor* OtherActor);

	UPROPERTY()
	TObjectPtr<APlayerController> PlayerController;

	// Editor only
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
};
