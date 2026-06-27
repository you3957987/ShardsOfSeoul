#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GrappleComp.generated.h"

UCLASS()
class SHARDSOFSEOUL_API UGrappleComp : public UActorComponent
{
	GENERATED_BODY()
public:
	UGrappleComp();
	
	UFUNCTION(BlueprintCallable, Category = "Movement|Grapple")
	void Grapple();

	UFUNCTION(BlueprintCallable, Category = "Movement|Grapple")
	void AddGrappleTarget(class AActor* TargetActor);

	UFUNCTION(BlueprintCallable, Category = "Movement|Grapple")
	void RemoveGrappleTarget(class AActor* TargetActor);

	UFUNCTION(BlueprintPure, Category = "Movement|Grapple")
	FORCEINLINE bool IsGrappling() const { return bIsGrappling; }

	UFUNCTION(BlueprintPure, Category = "Movement|Grapple")
	bool IsGrapplingOrPreparing() const;
	
protected:
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	// 최적의 그래플 대상(몬스터 등)을 검색하는 헬퍼 함수
	class AActor* FindBestGrappleTarget() const;

	// 애니메이션 재생 후 실제로 그래플 이동을 개시하는 헬퍼 함수
	void StartGrappleMove();
	
private:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Movement|Grapple", meta = (AllowPrivateAccess = "true"))
	class ACharacter* Owner;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Movement|Grapple", meta = (AllowPrivateAccess = "true"))
	class APlayerCameraManager* CameraManager;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Grapple", meta = (AllowPrivateAccess = "true"))
	float MaxGrappleDistance = 2000.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Grapple", meta = (AllowPrivateAccess = "true"))
	float LaunchSpeed = 1500.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Grapple", meta = (AllowPrivateAccess = "true"))
	FName GrappleableTag = FName("Grappleable");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Grapple", meta = (AllowPrivateAccess = "true"))
	float AutoTargetMaxAngle = 20.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Grapple", meta = (AllowPrivateAccess = "true"))
	class UAnimMontage* GrappleMontage;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Grapple", meta = (AllowPrivateAccess = "true"))
	float GrappleDelay = 0.3f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Movement|Grapple", meta = (AllowPrivateAccess = "true"))
	class AActor* CurrentTargetActor = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Movement|Grapple", meta = (AllowPrivateAccess = "true"))
	TArray<class AActor*> GrappleTargets;

	FTimerHandle GrappleDelayTimerHandle;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Movement|Grapple", meta = (AllowPrivateAccess = "true"))
	bool bIsGrappling = false;

	bool bShouldHoldInAir = false;
	FVector GrappleTargetLocation;
	float GrappleActiveTime = 0.f;
};
