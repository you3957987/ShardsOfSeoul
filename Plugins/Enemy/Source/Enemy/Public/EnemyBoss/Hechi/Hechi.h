#pragma once

#include "CoreMinimal.h"
#include "EnemyBoss/BaseBossEnemy.h"
#include "Hechi.generated.h"

USTRUCT(BlueprintType)
struct FBossHechiManAttackStruct
{
	GENERATED_BODY()
	
	UPROPERTY(EditAnywhere, Category = "자체설정")
	float LaserAttackWeight = 0.0f;
	UPROPERTY(EditAnywhere, Category = "자체설정")
	float GravityAttackWeight = 0.0f;
	UPROPERTY(EditAnywhere, Category = "자체설정")
	float TeleportWeight = 0.0f;
	UPROPERTY(EditAnywhere, Category = "자체설정")
	float ThrowMagicBallWeight = 0.0f;
	
	UPROPERTY(EditAnywhere, Category = "자체설정")
	float LaserAttackDamage = 10.f;
	UPROPERTY(EditAnywhere, Category = "자체설정")
	float LaserAttackDelay = 1.5f;
	
	UPROPERTY(EditAnywhere, Category = "자체설정")
	float GravityAttackDamage = 15.f;
	UPROPERTY(EditAnywhere, Category = "자체설정")
	float GravityAttackDelay = 1.5f;
	
	UPROPERTY(EditAnywhere, Category = "자체설정")
	float TeleportDelay = 4.0f;
	// 텔레포트 최대 거리
	UPROPERTY(EditAnywhere, Category = "자체설정")
	float MaxTeleportDist = 800.f;
	// 텔레포트 최소 거리
	UPROPERTY(EditAnywhere, Category = "자체설정")
	float MinTeleportDist = 500.f;
	
	UPROPERTY(EditAnywhere, Category = "자체설정")
	float ThrowMagicBallDelay = 3.0f;
	
	UPROPERTY(EditAnywhere, Category = "자체설정")
	float ChangePostProcessDelay = 2.0f;
	
};

UCLASS()
class ENEMY_API AHechi : public ABaseBossEnemy
{
	GENERATED_BODY()
	
	// 3초마다 실행될 함수 (내용은 나중에 채우시면 됩니다)
	void MyThreeSecondRepeatingFunction();

	// 타이머를 관리하기 위한 핸들 (나중에 타이머를 멈출 때 필요합니다)
	FTimerHandle RepeatingTimerHandle;

public:
	AHechi();
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;
	virtual float TakeDamage(float DamageAmount, struct FDamageEvent const& DamageEvent, 
		class AController* EventInstigator, AActor* DamageCauser) override;
	virtual void Die();
	virtual void AfterDieMontageEnd() override;
	virtual void EndBattleLog() override;
	virtual void Destroy();
	
	// 공격 가중치 구조체
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "자체설정")
	FBossHechiManAttackStruct AttackStruct;
	
	// 헤치 전용 로그데이터
	UPROPERTY(BlueprintReadOnly)
	FHechiLogData HechiLogData;
	
	// 레이저가 나갈 씬 컴포넌트
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	USceneComponent* LaserSpawnPoint;
	// 오른손 씬 컴포넌트
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	USceneComponent* RightHandPoint;
	
	// 포스트 프로세스 볼륨을 찾는 헬퍼 함수
	void InitializePostProcessVolume();
	
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
	
	// 블루프린트 HandleTeleport에서 사용할 변수
	UPROPERTY(BlueprintReadWrite, Category = "자체설정")
	bool HandleTeleportFlag = false;
	UPROPERTY(EditDefaultsOnly, Category = "자체설정")
	UAnimMontage* TeleportMontage;
	// 텔레포트 시작 함수
	UAnimMontage* PlayTeleportMontage(const FVector& Destination);
	// 텔레포트 목적지 위치 저장용 변수
	FVector TeleportDestination;
	// 애님 노티파이에서 호출할 함수
	UFUNCTION( BlueprintCallable )
	void TeleportMoveToNextPoint();
	UFUNCTION(BlueprintCallable)
	void StartDisappear();
	void EndDisappear();
	// 타이머를 관리할 핸들 변수
	FTimerHandle TeleportTimerHandle;
	UPROPERTY(EditDefaultsOnly, Category = "자체설정")
	float DisappearDuration = 4.0f; 
	UFUNCTION( BlueprintCallable )
	void SetMeshHidden();
	UFUNCTION( BlueprintCallable )
	void SetMeshVissible();
	UFUNCTION( BlueprintCallable )
	void SpawnBlackhole();
	UPROPERTY(EditDefaultsOnly, Category = "자체설정")
	TSubclassOf<class ABlackholeProjectile> BlackholeProjectileClass;
	
	UPROPERTY(EditDefaultsOnly, Category = "자체설정")
	UAnimMontage* ThrowMagicBallMontage;
	UAnimMontage* PlayThrowMagicBallMontage();
	UPROPERTY(EditDefaultsOnly, Category = "자체설정")
	TSubclassOf<class ABaseEnemyProjectile> MagicBallProjectileClass;
	UFUNCTION(BlueprintCallable)
	void ShootMagickBall();
	
	// 특수 패턴 수행할 체력 비율 0 ~ 1
	UPROPERTY(EditDefaultsOnly, Category = "자체설정")
	float ChangeMapHealthThreshold = 0.5f;
	UPROPERTY(EditDefaultsOnly, Category = "자체설정")
	float LoopChangePostProcess = 8.f;
	void StartChangeMapPattern();
	bool bIsChangeMap = false;
	UPROPERTY(EditDefaultsOnly, Category = "자체설정")
	UAnimMontage* ChangeMapMontage;
	UAnimMontage* PlayChangeMapMontage();
	// 캐릭터 빨아들이는 거 플래그
	UPROPERTY(BlueprintReadWrite, Category = "자체설정")
	bool HandleCharacterTeleportFlag = false;
	
	UFUNCTION(BlueprintCallable)
	void StartDisappearCharacter();
	void EndDisappearCharacter();
	FTimerHandle CharacterTeleportTimerHandle;
	UPROPERTY(EditDefaultsOnly, Category = "자체설정")
	class UParticleSystem* CharacterTeleportInEffect;
	UFUNCTION(BlueprintCallable)
	void PlayCharacterTeleportInEffect();
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "자체설정")
	TObjectPtr<class ATargetPoint> HechiTeleportPoint;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "자체설정")
	TObjectPtr<class ATargetPoint> CharacterTeleportPoint;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "자체설정")
	TObjectPtr<class ATargetPoint> CharacterReturnPoint;
	// 블루프린트 이벤트 그래프에 쓸 커스텀 이벤트
	UFUNCTION(BlueprintCallable, BlueprintImplementableEvent)
	void ChangeMap();
	// 블루프린트 이벤트 그래프에 쓸 커스텀 이벤트
	UFUNCTION(BlueprintCallable, BlueprintImplementableEvent)
	void ReturnToMap();
	UFUNCTION(BlueprintCallable)
	void DisablePlayerInput();
	UFUNCTION(BlueprintCallable)
	void EnablePlayerInput();
	UPROPERTY(EditDefaultsOnly, Category = "자체설정")
	class UNiagaraSystem* CharacterTeleportReadyEffect;
	UFUNCTION(BlueprintCallable)
	void PlayCharacterTeleportReadyEffect();
	
	UPROPERTY()
	class APostProcessVolume* RandomChangeLevelPostProcessVolume;
	UPROPERTY()
	class APostProcessVolume* FirstChangeLevelPostProcessVolume;
	// 에디터에서 포스트 프로세스 머터리얼(또는 인스턴스)들을 등록할 배열
	UPROPERTY(EditDefaultsOnly, Category = "자체설정")
	TArray<class UMaterialInterface*> RamdomPostProcessMaterialArray;
	// 처음에 초기화 할 머터리얼 인스턴스 배열
	UPROPERTY(EditDefaultsOnly, Category = "자체설정")
	TArray<class UMaterialInterface*> FirstPostProcessMaterialArray;
	void ChangePostProcessMaterialByIndex(int32 Index);
	UPROPERTY(EditDefaultsOnly, Category = "자체설정")
	UAnimMontage* ChangePostProcessMontage;
	UAnimMontage* PlayChangePostProcessMontage();
	UFUNCTION(BlueprintCallable)
	void RandomChangePostProcess();
	UFUNCTION(BlueprintCallable)
	void FirstChangePostProcess();
	int32 CurrentPostProcessIndex = 0;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "자체설정")
	TSubclassOf<UCameraShakeBase> CameraShakeClass;
	// 카메라를 흔드는 기능만 담당할 함수
	void ShakeCamera();
	FTimerHandle PostProcessLoopHandle;
	UFUNCTION(BlueprintCallable)
	void StartLoopPostProcessChange();
	
#if WITH_EDITOR
	// 에디터에서 프로퍼티가 변경될 때 호출됩니다.
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
};
