#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "EnemyLogManager.h"
#include "Interface/ItemDropInterface.h"
#include "CSVLogType.h" 
#include "BaseEnemy.generated.h"

// 에디터 에너미 폴더 안의 ENUM 안에다가도 추가해줘야함!!!!!!!!!!!!!!!!!!!!!!!!
// 에디터 에너미 폴더 안의 ENUM 안에다가도 추가해줘야함!!!!!!!!!!!!!!!!!!!!!!!!
// 에디터 에너미 폴더 안의 ENUM 안에다가도 추가해줘야함!!!!!!!!!!!!!!!!!!!!!!!!
UENUM(BlueprintType, Meta=(DisplayName="Enemy Type")) // 적 타입을 정의하는 열거형
enum class EEnemyType : uint8
{
	EET_Melee UMETA(DisplayName = "Melee Enemy"), // 근접 공격 적
	EET_Ranged UMETA(DisplayName = "Ranged Enemy"), // 원거리 공격 적
	EET_Exploder UMETA(DisplayName = "Exploder Enemy"), // 폭발 적
	EET_Transpar UMETA(DisplayName = "Transpar Enemy"), // 투명 몹
	EET_Mimic UMETA(DisplayName = "Mimic Enemy"), // 위장 몹
	EET_Slime UMETA(DisplayName = "Slime Enemy"), // 슬라임 몹(분열)
	EET_Mage UMETA(DisplayName = "Mage Enemy"), // 마법사 몹 - 일단 안에 가능한 모든 공격 만들고 애님 노티파이로 결정
	EET_Guard UMETA(DisplayName = "Guard Enemy"), // 방패병 몹
	EET_Passive UMETA(DisplayName = "Passive Enemy"), // 패시브 몹 - 공격 안하는 몹 == 비선공 , 플레이어가 공격해야 반응하는 몹 등등
	EET_Burrow UMETA(DisplayName = "Burrow Enemy"), // 버로우 지렁이 몹
	EET_Revive UMETA(DisplayName = "Revive Enemy"), // 부활 몹
	// 스포너형은 따로 있음
	// 부활은 밀리랑 레인지드 기반

	EET_MAX UMETA(DisplayName = "Default") // 최대값, 추가적인 값을 위한 공간
};

UCLASS()
class ENEMY_API ABaseEnemy : public ACharacter, public IItemDropInterface
{
	GENERATED_BODY()

	friend class AEnemyAiController; // EnemyAiController에서 BaseEnemy의 protected 멤버에 접근할 수 있도록 합니다.

protected:
	virtual void BeginPlay() override;

	// 충돌 구 컴포넌트
	UPROPERTY(VisibleAnywhere)
	class USphereComponent* AttackRangeSphere; 
	// 인식 범위 확인용 컴포넌트
	UPROPERTY(VisibleAnywhere)
	class USphereComponent* DetectRangeSphere; 
	// 추적 범위 확인용 컴포넌트
	UPROPERTY(VisibleAnywhere)
	class USphereComponent* ChaseRangeSphere;;
	
public:
	ABaseEnemy();
	virtual void Tick(float DeltaTime) override;
	void PollInit(); // 틱에서 하는 초기화
	
	UFUNCTION(BlueprintPure)
	FORCEINLINE bool IsSpawnEnd() const { return !bUseSpawnMontage; }; // 스폰 몽타주 끝났는지 여부 반환
	FORCEINLINE EEnemyType GetEnemyType() const { return EnemyType; }; // 적 타입 반환
	
	// 데미지 처리 함수 재정의
	virtual float TakeDamage(float DamageAmount, struct FDamageEvent const& DamageEvent,
		class AController* EventInstigator, AActor* DamageCauser) override;

	class UBlackboardComponent* BlackboardComp; // 블랙보드 컴포넌트 포인터
	bool bBlackboardInitialized = false;
	
	// 로그 데이터에 사용할 EnemyID
	UPROPERTY(EditAnywhere, Category="자체설정")
	FString EnemyLogID;
	// 이번 전투 세션의 데이터를 실시간으로 누적할 바구니
	UPROPERTY(BlueprintReadOnly)
	FEnemyLogData EnemyLogData;
	// 적 타입
	UPROPERTY(EditAnywhere, Category="자체설정")
	EEnemyType EnemyType = EEnemyType::EET_Melee;
	FString GetEnemyTypeAsString() const;
	// 아이템 드롭 테이블에 사용하는 에너미 ID
	UPROPERTY(EditAnywhere, Category="자체설정")
	FName ItemDropTableEnemyID;
	
	virtual FName GetItemDropTableEnemyID_Implementation() const override { return ItemDropTableEnemyID; }
	
	// 타겟으로 삼을 캐릭터를 저장할 변수
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "자체설정")
	TObjectPtr<ACharacter> TargetCharacter;
	// 캐릭터 초기화 여부를 나타내는 변수
	bool bTargetInitalize = false;
	// 체력 바 위젯 컴포넌트
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "자체설정")
	TObjectPtr<class UWidgetComponent> HealthBarWidget;

	// 락온용 위젯 컴포넌트
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "자체설정")
	TObjectPtr<class UWidgetComponent> LockOnWidget;
	// Tick에서 체력 바 위젯을 내 캐릭터 쪽으로 돌아보게 하는 함수
	void UpdateHealthBarWidget(float DeltaTime);
	
	// 디버그 모드
	UPROPERTY(EditAnywhere, Category="자체설정")
	bool bDebugMode = false;
	// 죽음 로직 체크. 3초 뒤에 체력을 0으로 만든 후 Die() 호출
	UPROPERTY(EditAnywhere, Category="자체설정")
	bool bCheckDeadLogic = false;
	void TestDeadLogic(); // 테스트용 죽음 로직 함수
	// 최대 체력
	UPROPERTY(EditAnywhere, Category="자체설정")
	float MaxHealth = 50.f;
	// 현재 체력
	UPROPERTY(EditAnywhere, Category="자체설정")
	float Health = 50.f;
	// 이동 속도 = 여기서 값 바꾸면 자동으로 캐릭터 무브먼트 컴포넌트의 MaxWalkSpeed도 바뀜
	UPROPERTY(EditAnywhere, Category="자체설정")
	float MoveSpeed = 300.f;
	// 이거 수정하면 자동으로 AttackRangeSphere의 반지름도 수정되게 만듬
	UPROPERTY(EditAnywhere, Category="자체설정")
	float AttackRange = 100.f;
	// AI가 플레이어를 인식하는 범위
	UPROPERTY(EditAnywhere, Category="자체설정")
	float DetectRange = 800.f; 
	// AI가 플레이어를 추적하는 범위
	UPROPERTY(EditAnywhere, Category="자체설정")
	float ChaseRange = 1500.f;
	// 공격 애니메이션 몽타주 배열
	UPROPERTY(EditDefaultsOnly, Category="자체설정")
	TArray<TObjectPtr<class UAnimMontage>> AttackMontages;
	virtual UAnimMontage* Attack(); // 공격 함수. 
	// 공격 딜레이
	UPROPERTY(EditAnywhere, Category="자체설정")
	float AttackDelay = 2.0f;
	// 패트롤 딜레이
	UPROPERTY(EditAnywhere, Category="자체설정")
	float PatrolDelay = 3.f;
	
	
	// 죽음 애니메이션 몽타주
	UPROPERTY(EditDefaultsOnly, Category = "자체설정")
	class UAnimMontage* DeathMontage;
	// 스폰시 재생할 몽타주
	UPROPERTY(EditDefaultsOnly, Category = "자체설정")
	class UAnimMontage* SpawnMontage;
	
	// 죽을떄 실행할 사운드
	UPROPERTY(EditDefaultsOnly, Category="자체설정")
	class USoundBase* DeadSound;
	// 죽을 떄 생성할 이펙트 == 케스케이드
	UPROPERTY(EditDefaultsOnly, Category="자체설정")
	class UParticleSystem* DeadEffectCascade;
	// 죽음 이펙트 생성 앞 뒤 위치 조정용 거리 +가 앞, -가 뒤
	UPROPERTY(EditDefaultsOnly, Category="자체설정")
	float DeadEffectForwardOffset = 0.0f;
	// 죽음 이펙트 생성 위 아래 위치 조정용 거리 +가 위, -가 아래
	UPROPERTY(EditDefaultsOnly, Category="자체설정")
	float DeadEffectUpOffset = 0.0f;
	// 죽음 이펙트 크기 배율
	UPROPERTY(EditDefaultsOnly, Category="자체설정")
	float DeadEffectScale = 1.0f;
	// 죽음 처리 함수
	virtual void Die();
	// 죽음 몽타주 끝난 후 호출되는 함수 - 애님 노티파이에서 호출
	UFUNCTION(BlueprintCallable)
	virtual void AfterDieMontageEnd();
	
	// 죽음 후 일정 시간 뒤에 이펙트 생성 및 액터 제거를 위한 타이머 핸들
	FTimerHandle DeathTimerHandle;
	virtual void SpawnDeadEffectAndDestroy();
	
	// 스폰 효과 사용 여부. 기본은 트루이지만 스폰 몽타주 안넣으면 false랑 동일
	UPROPERTY(EditDefaultsOnly, Category = "자체설정")
	bool bUseSpawnMontage = false;
	// 스폰 몽타주로 스폰중에는 대미지 안 받도록 하게 하는 변수
	bool bIsSpawning = false;
	// 스폰 몽타주 끝난 후 AI 컨트롤러 빙의 함수 - 애님 노티파이에서 호출
	UFUNCTION( BlueprintCallable )
	void SpawnAndPossessAIController();
	// 스폰 몽타주 초반에 메시를 보이게 하는 함수 - 애님 노티파이에서 호출
	UFUNCTION(BlueprintCallable)
	void ShowCharacterMesh();
	
	// 무조건 플레이어 추적 모드
	UPROPERTY(EditAnywhere, Category="자체설정")
	bool bAlwaysChase = false;
	
	// ATargetPoint 액터를 패트롤 지점으로 사용하기 위한 새 프로퍼티
	UPROPERTY(EditAnywhere, Category = "자체설정", meta=(DisplayName="Patrol Points"))
	TArray<TObjectPtr<class ATargetPoint>> PatrolPoints;
	
	bool bFocusPlayerAfterAttack = true; // 공격 후 플레이어 주시 여부
	UFUNCTION(BlueprintCallable)
	void StartFocusPlayerAfterAttack(); // 공격 후 플레이어 주시 시작 함수
	
	UFUNCTION(BlueprintCallable)
	void SetAttackDelayToBehaviorTree( float Delay ); // AttackDelay 값을 블랙보드에 설정하는 함수
	
	// 피격용 Overlay 머티리얼 == 피격시 머터리얼 덧씌우기
	UPROPERTY(EditDefaultsOnly, Category = "자체설정")
	class UMaterialInterface* HitOverlayMaterial;
	FTimerHandle HitFlashTimerHandle;

	// 히트시 오버레이 덧씌울 함수
	void SetHitOverlay();
	// 오버레이를 다시 제거할 함수
	void ClearHitOverlay();
	
	// 로그 관리 함수
	float BattleStartTime = 0.f;
	bool bIsInBattle = false;
	void StartBattleLog();
	void EndBattleLog();
	EEnemyLogType GetLogTypeFromEnemyType() const;
	UFUNCTION()
	void PlayerDeadLog();

	
#if WITH_EDITOR
	// 에디터에서 프로퍼티가 변경될 때 호출됩니다.
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
};
