#pragma once

#include "CoreMinimal.h"
#include "EnemyBoss/BaseBossEnemy.h"
#include "BossMagicSwordMan.generated.h"

USTRUCT(BlueprintType)
struct FBossMagicSwordManAttackStruct
{
	GENERATED_BODY()
	
	// 근점 공격 - 3개중 랜덤
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "자체설정")
	float CloseAttackWeight = 0.0f;
	// 대시 공격 - 3개중 랜덤
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "자체설정")
	float DashAttackWeight = 0.0f;
	// 근점 띄우기 공격
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "자체설정")
	float CloseJumpUpAttackWeight = 0.0f;
	// 대시 띄우기 공격
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "자체설정")
	float DashJumpUpAttackWeight = 10.0f;
	// 점프 공격
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "자체설정")	
	float JumpAttackWeight = 0.0f;
	// 가드 패턴
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "자체설정")
	float GuardWeight = 0.0f;
	// 궁극기 패턴
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "자체설정")
	float PowerAttackWeight = 0.0f;
	// 원거리 검기 공격 패턴
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "자체설정")
	float BladeWaveAttackWeight = 0.0f;
	// 백대시 패턴 
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "자체설정")
	float BackDashWeight = 0.0f;
	
	// 근접 공격 딜레이	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "자체설정")
	float CloseAttackDelay = 1.f;
	
	// 대시 공격 딜레이
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "자체설정")
	float DashAttackDelay = 1.f;
	
	// 근점 띄우기 공격 딜레이
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "자체설정")
	float CloseJumpUpAttackDelay = 2.5f;
	
	// 대시 띄우기 공격 딜레이
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "자체설정")
	float DashJumpUpAttackDelay = 2.5f;
	
	// 공중 공격 이후 딜레이	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "자체설정")
	float AirAttackDelay = 1.f;
	
	// 점프 공격 딜레이
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "자체설정")
	float JumpAttackDelay = 1.f;
	
	// 가드 후 딜레이 
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "자체설정")
	float GuardDelay = 1.5f;
	// 가드 유지 시간
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "자체설정")
	float GuardDuration = 4.f;
	// 반격 발동에 필요한 최대 대미지
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "자체설정")
	float MaxDamageToReaction = 30.f;
	
	// 궁극기 틱 대미지
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "자체설정")
	float PowerAttackTickDamage = 8.f;
	// 궁극기 딜레이
	UPROPERTY( EditAnywhere, BlueprintReadWrite, Category = "자체설정")
	float PowerAttackDelay = 2.5f;
	// 궁극기 마무리 대미지
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "자체설정")
	float PowerAttackFinishDamage = 15.f;
	
	// 백대시 딜레이
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "자체설정")
	float BackDashDelay = 0.1f;
	
	// 원거리 검기 공격 딜레이
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "자체설정")
	float BladeWaveAttackDelay = 1.0f;
	
	// 러쉬 스트라이크 대미지
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "자체설정")
	float RushStrikeAttackDamage = 15.f;
};

// 어택 타입 구분용 열거형
UENUM(BlueprintType)
enum class EMagicSwordManAttackType : uint8
{
	CloseAttack UMETA(DisplayName = "CloseAttack"),
	DashAttack UMETA(DisplayName = "DashAttack"),
	CloseJumpUpAttack UMETA(DisplayName = "CloseJumpUpAttack"),
	DashJumpUpAttack UMETA(DisplayName = "DashJumpUpAttack"),
	JumpAttack UMETA(DisplayName = "JumpAttack"),
	GuardCounterAttack UMETA(DisplayName = "GuardCounterAttack"),
	AirAttack UMETA(DisplayName = "AirAttack"),
};

UCLASS()
class ENEMY_API ABossMagicSwordMan : public ABaseBossEnemy
{
	GENERATED_BODY()
	UPROPERTY(VisibleAnywhere)
	class UCapsuleComponent* WeaponCollision = nullptr;
	
	float AttackDamage;
	
	// 깃 수정 확인용 
public:
	ABossMagicSwordMan();
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;
	virtual float TakeDamage(float DamageAmount, struct FDamageEvent const& DamageEvent, 
		class AController* EventInstigator, AActor* DamageCauser) override;
	virtual void EndBattleLog() override;
	
	UFUNCTION()
	void OnBeginOverlapWeaponCollisionSphere(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);
	
	UFUNCTION(BlueprintCallable)
	void AttackStart_WeaponCollision();
	UFUNCTION(BlueprintCallable)
	void AttackEnd_WeaponCollision();
	
	// 애님 노티파이에서 공격 대미지 설정
	UFUNCTION(BlueprintCallable)
	void SetAttackDamage(float DamageToApply);
	// 공격 가중치 구조체
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "자체설정")
	FBossMagicSwordManAttackStruct AttackStruct;
	
	UPROPERTY(BlueprintReadOnly)
	FBossMagicSwordManLogData BossMagicSwordManLogData;
	
	EMagicSwordManAttackType AttackType = EMagicSwordManAttackType::CloseAttack; // 기본값은 근접 공격으로 설정
	
	// 가드시 히트 몽타주
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "자체설정")
	UAnimMontage* GuardHitMontage;
	// 가드시 가드 풀리는 몽타주
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "자체설정")
	UAnimMontage* GuardBreakMontage;
	UPROPERTY(VisibleAnywhere,BlueprintReadOnly)
	bool bIsGuarding = false;
	float DamageWhileGuarding = 0.f;
	
	// 가드 반격 몽타주 배열
	UPROPERTY(EditDefaultsOnly, Category = "자체설정")
	TArray<UAnimMontage*> GuardReactionMontages;
	// 가드 반격 시작 함수
	UAnimMontage* StartGuardReactionAttack();
	
	// 근접 공격 몽타주 배열
	UPROPERTY(EditDefaultsOnly, Category = "자체설정")
	TArray<UAnimMontage*> CloseAttackMontages;
	// 근점 공격 시작 함수 - 랜덤 3개. 선택된 몽타주 반환
	UAnimMontage* StartCloseAttack();
	
	// 대시 공격 몽타주 배열
	UPROPERTY(EditDefaultsOnly, Category = "자체설정")
	TArray<UAnimMontage*> DashAttackMontages;
	// 대시 공격 시작 함수 - 랜덤 3개. 선택된 몽타주 반환
	UAnimMontage* StartDashAttack();
	
	// 근점 띄우기 공격 몽타주
	UPROPERTY(EditDefaultsOnly, Category = "자체설정")
	UAnimMontage* CloseJumpUpAttackMontage;
	// 근점 띄우기 공격 시작 함수
	UAnimMontage* StartCloseJumpUpAttack();
	
	// 대시 띄우기 공격 몽타주
	UPROPERTY(EditDefaultsOnly, Category = "자체설정")
	UAnimMontage* DashJumpUpAttackMontage;
	// 대시 띄우기 공격 시작 함수
	UAnimMontage* StartDashJumpUpAttack();
	
	// 공중 공격 몽타주 
	UPROPERTY(EditDefaultsOnly, Category = "자체설정")
	UAnimMontage* AirAttackMontage;
	bool bSuccessJumpUpAttack = false;
	// 띄우거 성공 여부에 따른 이후 행동 결정 함수
	UFUNCTION(BlueprintCallable)
	void JumpUpAttackCheck();
	// 공중 공격 시작 함수
	UAnimMontage* StartAirAttack();
	// 공중 공격 끝났을떄 처리 함수
	UFUNCTION(BlueprintCallable)
	void AirAttackEnd();
	float BossGravityScaleBeforeAirAttack; // 공중 공격 시작 전 보스의 중력 스케일 저장용 변수
	float TargetCharacterGravityScaleBeforeAirAttack; // 공중 공격 시작 전 타깃 캐릭터의 중력 스케일 저장용 변수
	
	// 점프 공격 몽타주 배열
	UPROPERTY(EditDefaultsOnly, Category = "자체설정")
	TArray<UAnimMontage*> JumpAttackMontages;
	// 점프 공격 함수
	UAnimMontage* StartJumpAttack();
	UFUNCTION(BlueprintCallable)
	void JumpStart();
	
	// 궁극기 몽타주
	UPROPERTY(EditDefaultsOnly, Category = "자체설정")
	UAnimMontage* PowerAttackMontage;
	// 궁극기 시전 함수
	UAnimMontage* StartPowerAttack();
	// 궁극기 피격 범위 스피어 콜리전 컴포넌트
	UPROPERTY(VisibleAnywhere)
	class USphereComponent* PowerAttackCollisionSphere;
	// 궁극기 피격 처리 함수
	UFUNCTION() 
	void OnBeginOverlapPowerAttackCollisionSphere(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);
	// 궁극기 공격 시작
	UFUNCTION(BlueprintCallable)
	void StartPowerAttackCollision();
	// 궁극기 공격 종료
	UFUNCTION(BlueprintCallable)
	void EndPowerAttackCollision();
	// 궁극기 대미지 로직 처리 함수
	void HandlePowerAttackDamage(AActor* OtherActor);
	// 주기적인 대미지 적용을 위한 타이머 핸들
    FTimerHandle PowerAttackTimerHandle;
	// 타이머에 의해 호출될 함수 (대상 액터를 인자로 받음)
	void OnPowerAttackTimerTick(AActor* TargetActor);
	// 궁극기 적중 여부
	bool bIsPowerAttackHit = false;
	// 궁극기 마무리 공격 처리 함수
	UFUNCTION(BlueprintCallable)
	void FinishPowerAttack();
	
	// 원거리 검기 공격 몽타주
	UPROPERTY(EditDefaultsOnly, Category = "자체설정")
	UAnimMontage* BladeWaveAttackMontage;
	// 원거리 검기 공격 시작 함수	
	UAnimMontage* StartBladeWaveAttack();
	// 애님 노티파이로 검기 날아가는 함수
	UFUNCTION(BlueprintCallable)
	void StartBladeWave();
	// 검기 발사체 클래스 
	UPROPERTY(EditDefaultsOnly, Category = "자체설정")
	TSubclassOf<class ABaseEnemyProjectile> BladeWaveProjectileClass;
	
	// 뒤로 백대시 하는 몽타주
	UPROPERTY(EditDefaultsOnly, Category = "자체설정")
	UAnimMontage* DashBackMontage;
	// 백대시 시작 함수
	UAnimMontage* StartDashBack();
	
	// 특수 패턴 수행할 체력 비율 0 ~ 1
	UPROPERTY(EditDefaultsOnly, Category = "자체설정")
	float SecondPhaseHealthThreshold = 0.5f;
	// 특정 체력 이하로 내려갈 시 실행할 특수 패턴
	void StartSecondPhase();
	bool bIsSecondPhaseStarted = false;
	// 특수 공격 몽타주
	UPROPERTY(EditDefaultsOnly, Category = "자체설정")
	class UAnimMontage* SecondPhaseMontage;
	UAnimMontage* PlaySecondPhaseMontage();
	
	UPROPERTY(EditAnywhere, Category = "자체설정")
	TObjectPtr<class ATargetPoint> HoverLocationPoint;
	
	UFUNCTION(BlueprintCallable)
	void StartHover();
	UFUNCTION(BlueprintCallable)
	void SetGravityCharacter();
	UFUNCTION(BlueprintCallable)
	void RushToPlayer();
	
	float DefaultGravityScaleBeforeHover;
	
	// RushAttackCollisionSphere 비긴 오버랩 함수
	UFUNCTION(BlueprintCallable)
	void RushStrike();

#if WITH_EDITOR
	// 에디터에서 프로퍼티가 변경될 때 호출됩니다.
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
};
