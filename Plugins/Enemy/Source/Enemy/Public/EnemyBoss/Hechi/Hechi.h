#pragma once

#include "CoreMinimal.h"
#include "EnemyBoss/BaseBossEnemy.h"
#include "Hechi.generated.h"

USTRUCT(BlueprintType)
struct FBossHechiManAttackStruct
{
	GENERATED_BODY()
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "자체설정")
	float LaserAttackWeight = 0.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "자체설정")
	float GravityAttackWeight = 0.0f;
	
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "자체설정")
	float LaserAttackDamage = 10.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "자체설정")
	float LaserAttackDelay = 1.5f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "자체설정")
	float GravityAttackDamage = 15.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "자체설정")
	float GravityAttackDelay = 1.5f;
};


UCLASS()
class ENEMY_API AHechi : public ABaseBossEnemy
{
	GENERATED_BODY()

public:
	AHechi();
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;
	virtual float TakeDamage(float DamageAmount, struct FDamageEvent const& DamageEvent, 
		class AController* EventInstigator, AActor* DamageCauser) override;
	virtual void Die();
	
	// 공격 가중치 구조체
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "자체설정")
	FBossHechiManAttackStruct AttackStruct;
	
	// 헤치 전용 로그데이터
	UPROPERTY(BlueprintReadOnly)
	FHechiLogData HechiLogData;
	
	// 레이저가 나갈 씬 컴포넌트
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "자체설정")
	USceneComponent* LaserSpawnPoint;
	// 오른손 씬 컴포넌트
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "자체설정")
	USceneComponent* RightHandPoint;
	
	UPROPERTY(EditDefaultsOnly, Category = "자체설정")
	UAnimMontage* LaserAttackMontage;
	// 레이저 공격 시작 함수
	UAnimMontage* StartLaserAttack();
	// 애님 노티파이 - 레이저 발사 시작
	UFUNCTION(BlueprintCallable)
	void StartLaser();
	UPROPERTY()
	class UNiagaraComponent* CurrentLaserComp;
	// 레이저 나이아가라 이펙트
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "자체설정")
	class UNiagaraSystem* LaserEffect;
	FTimerHandle LaserTimerHandle;
	// ◀레이저를 끄는 기능을 담당할 함수 선언
	void StopLaser();
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "자체설정")
	TSubclassOf<class ABaseEnemyProjectile> LaserProjectileClass;
	UFUNCTION(BlueprintCallable)
	void StartShootLaserProjectile();
	UFUNCTION(BlueprintCallable)
	void StopShootLaserProjectole();
	// ◀ 연사 속도(발사 텀) 조절용 변수 추가 (기본값 0.2초)
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "자체설정", meta = (AllowPrivateAccess = "true"))
	float ProjectileFireRate = 0.2f; 
	// ◀ 반복 발사를 제어할 타이머 핸들 추가
	FTimerHandle ProjectileTimerHandle;
	// ◀ 실제로 1발씩 스폰하는 내부 함수 (기존 ShootLaserProjectile 내부에 있던 로직 분리용)
	void FireOneProjectile();
	
	UPROPERTY(EditDefaultsOnly, Category = "자체설정")
	UAnimMontage* GravityAttackMontage;
	// 레이저 공격 시작 함수
	UAnimMontage* PlayGravityAttack();
	UFUNCTION(BlueprintCallable)
	void StartGravityAttack();
	UFUNCTION(BlueprintCallable)
	void EndGravityAttack();
	UPROPERTY(EditDefaultsOnly, Category = "자체설정")
	class UNiagaraSystem* GravityGroundEffect;
	UPROPERTY(EditDefaultsOnly, Category = "자체설정")
	class UNiagaraSystem* GravityImpactEffect;
	void HandleGravityAttack(float DeltaTime);
	
	float DefaultGravityScale = 1.0f;
	float DefaultAirControl = 0.05f;
	float DefaultMaxWalkSpeed = 1.0f;
	
	bool bIsGravityAttackActive = false;
	FVector GravityAttackCenter;
	float GravityRadius = 800.f;
	float GravityHalfHeight = 700.f;
	float GravityDuration = 4.8f;
	float GravityTimer = 0.0f;
	
	
#if WITH_EDITOR
	// 에디터에서 프로퍼티가 변경될 때 호출됩니다.
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
};
