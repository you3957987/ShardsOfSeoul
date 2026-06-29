#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ShootingComp.generated.h"

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class SHARDSOFSEOUL_API UShootingComp : public UActorComponent
{
	GENERATED_BODY()
public:
	UShootingComp();
	
protected:
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	
public:
	// 조준 및 발사 함수들
	UFUNCTION(BlueprintCallable, Category = "Combat|Shooting")
	void StartAiming();

	UFUNCTION(BlueprintCallable, Category = "Combat|Shooting")
	void StopAiming();
	
	UFUNCTION(BlueprintCallable, Category = "Combat|Shooting")
	void Fire();
	
	UFUNCTION(BlueprintPure, Category = "Combat|Shooting")
	FORCEINLINE bool IsAiming() const { return bIsAiming; }
	
private:
	bool GetMuzzleLocationAndRotation(FVector& OutLoc, FRotator& OutRot) const;

	// 캐싱 포인터들
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat|Shooting", meta = (AllowPrivateAccess = "true"))
	class ACharacter* OwnerCharacter;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat|Shooting", meta = (AllowPrivateAccess = "true"))
	class USpringArmComponent* SpringArmComp;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat|Shooting", meta = (AllowPrivateAccess = "true"))
	class UCameraComponent* CameraComp;

	// 조준 시 카메라 설정값들 (EditAnywhere로 에디터에서 미세 튜닝 가능하도록 설정)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Shooting", meta = (AllowPrivateAccess = "true"))
	float DefaultArmLength = 400.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Shooting", meta = (AllowPrivateAccess = "true"))
	float AimArmLength = 150.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Shooting", meta = (AllowPrivateAccess = "true"))
	FVector DefaultSocketOffset = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Shooting", meta = (AllowPrivateAccess = "true"))
	FVector AimSocketOffset = FVector(0.f, 60.f, 90.f); // 오른쪽 어깨 위 숄더뷰용 오프셋

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Shooting", meta = (AllowPrivateAccess = "true"))
	float InterpSpeed = 10.f; // 카메라 보간 속도

	// 사격 관련 설정
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Shooting", meta = (AllowPrivateAccess = "true"))
	float MaxFireDistance = 5000.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Shooting", meta = (AllowPrivateAccess = "true"))
	float BaseDamage = 20.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Shooting", meta = (AllowPrivateAccess = "true"))
	class UAnimMontage* FireMontage;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Shooting", meta = (AllowPrivateAccess = "true"))
	TSubclassOf<class UUserWidget> AimWidgetClass;

	UPROPERTY()
	class UUserWidget* AimWidgetInstance;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Shooting", meta = (AllowPrivateAccess = "true"))
	class UStaticMesh* PistolStaticMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat|Shooting", meta = (AllowPrivateAccess = "true"))
	class UStaticMeshComponent* PistolMeshComponent;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Shooting", meta = (AllowPrivateAccess = "true"))
	FName PistolSocketName = FName("PistolSocket");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Shooting", meta = (AllowPrivateAccess = "true"))
	float MovementSpreadMultiplier = 50.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Shooting|FX", meta = (AllowPrivateAccess = "true"))
	class UNiagaraSystem* TracerEffect;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Shooting|FX", meta = (AllowPrivateAccess = "true"))
	class UNiagaraSystem* ImpactEffect;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Shooting|Sound", meta = (AllowPrivateAccess = "true"))
	class USoundBase* FireSound;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Shooting|Sound", meta = (AllowPrivateAccess = "true"))
	class USoundBase* ImpactSound;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Shooting|Debug", meta = (AllowPrivateAccess = "true"))
	bool bDrawDebugLine = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Shooting|UI", meta = (AllowPrivateAccess = "true"))
	float DefaultWidgetOpacity = 0.3f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat|Shooting", meta = (AllowPrivateAccess = "true"))
	bool bIsAiming = false;

	UPROPERTY()
	TWeakObjectPtr<class AActor> LastColoringTargetActor = nullptr;
};
