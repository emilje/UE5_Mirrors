#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CVrMirror.generated.h"

class UCameraComponent;
class ATriggerBox;

UCLASS()

class UE5_MIRRORS_API ACVrMirror : public AActor
{
	GENERATED_BODY()

public:
	ACVrMirror();
	virtual void Tick(const float DeltaTime) override;
	void Init();

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TObjectPtr<USceneCaptureComponent2D> SceneCaptureLeftEye;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TObjectPtr<USceneCaptureComponent2D> SceneCaptureRightEye;

	FVector2D Resolution = FVector2D::ZeroVector;
	TObjectPtr<UCameraComponent> ActiveCamera;

protected:
	virtual void BeginPlay() override;
	virtual void PreInitializeComponents() override;
	virtual void PostInitializeComponents() override;

	UFUNCTION(BlueprintCallable)
	void SetupCaptureTriggers();

	// If not ticked, will greatly increase performance but reflection will be 2d and misaligned on both eyes. 
	UPROPERTY(EditAnywhere, meta=(DisplayName="Stereoscopic"))
	bool bIsStereoscopic = false;

	// Will cull objects that should not be seen in the reflection.
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
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<AActor*> DontCullActors;

	// Resolution capture multiplier. 1 for full resolution capture. More than 1 is oversampling - it can increase sharpness at a high cost.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin=0.1, ClampMax=2))
	float CaptureQuality = 1;

	// Captures won't trigger when this distance is exceeded.
	UPROPERTY(EditAnywhere)
	float CaptureMaxDistance = 5000;

	// Will decrease capture resolution as camera gets further from the mirror.
	UPROPERTY(EditAnywhere)
	bool bEnableDynamicCaptureResolution = false;

	// How often do we check and adjust the capture resolution in seconds.
	UPROPERTY(EditAnywhere, meta=(ClampMin=0))
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

	// Interpupillary distance in centimeters. 0 for automatic.
	UPROPERTY(EditAnywhere)
	float CustomIpdCm = 0;

	// Mirror will capture as long as we are within one of these boxes.
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<TObjectPtr<ATriggerBox>> CaptureTriggers;

	// Display number of active triggers for this mirror.
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bDisplayNumOfActiveTriggers = false;

	UPROPERTY()
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY()
	TObjectPtr<UTextureRenderTarget2D> RenderTargetLeftEye;

	UPROPERTY()
	TObjectPtr<UTextureRenderTarget2D> RenderTargetRightEye;

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
	void MirrorCulling(const FTransform& MirroredCameraTransform);
	bool ShouldSkipCapture() const;
	FTransform MirrorCamera(const FTransform& CameraTransform) const;
	static FVector2D GetHmdResolution();
	TArray<FTransform> CreateEyeOffsets(const FTransform& CameraTransform) const;
	float GetIpdCm() const;
	static FVector2D GetHmdFov();
	void FindActiveCamera();

	int32 HorizontalFov;
	float InitialCaptureQuality;
	float IpdHalfDistanceCm;
	FVector LastTraceCameraPosition = FVector::ZeroVector;
	bool bIsMobileMultiView = false;
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
