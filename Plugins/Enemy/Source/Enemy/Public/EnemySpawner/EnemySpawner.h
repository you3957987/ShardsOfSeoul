#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CSVLogType.h" 
#include "EnemySpawner.generated.h"

UCLASS()
class ENEMY_API AEnemySpawner : public AActor
{
	GENERATED_BODY()
	
protected:
	virtual void BeginPlay() override;
	// 데미지 처리 함수 재정의
	virtual float TakeDamage(float DamageAmount, struct FDamageEvent const& DamageEvent,
		class AController* EventInstigator, AActor* DamageCauser) override;
	
	void PollInit(); // 틱에서 하는 초기화
	
	// 로그 데이터에 사용할 EnemyID
	UPROPERTY(EditAnywhere, Category="자체설정")
	FString EnemyLogID;
	// 이번 전투 세션의 데이터를 실시간으로 누적할 바구니
	UPROPERTY(BlueprintReadOnly)
	FEnemyLogData EnemyLogData;
	
	UPROPERTY(EditAnywhere, Category = "자체설정")
	bool bDebugMode = false;

	// 캐릭터 초기화 여부를 나타내는 변수
	bool bTargetInitalize = false;
	// 타겟으로 삼을 캐릭터를 저장할 변수
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "자체설정")
	TObjectPtr<ACharacter> TargetCharacter;
	
	// 루트 컴포넌트가 될 캡슐 충돌 컴포넌트
	UPROPERTY(VisibleAnywhere)
	class UCapsuleComponent* RootCollisionSphere;
	
	// 메시 컴포넌트
	UPROPERTY(VisibleAnywhere)
	class UStaticMeshComponent* SpawnerMesh;
	// 플레이어 감지 범위 컴포넌트
	UPROPERTY(VisibleAnywhere)
	class USphereComponent* PlayerDetectSphere;

	// 비긴 오버랩 함수
	UFUNCTION()
	void OnBeginOverlapPlayerDetectSphere(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	// 플레이어 감지 종료 (스폰 중지) 
	UFUNCTION()
	void OnEndOverlapPlayerDetectSphere(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);
	
	// Tick에서 체력 바 위젯을 내 캐릭터 쪽으로 돌아보게 하는 함수
	void UpdateHealthBarWidget(float DeltaTime);
	
	// 플레이어가 보이는지 여부 확인 함수
	bool CanSeePlayer() const;
	// 플레이어 감시 하는 딜레이
	float CurrentSpawnCooldown = 0.0f;
	
	bool bActivateSpawner = false;
	
	// 몬스터가 스폰될 위치 씬 컴포넌트
	UPROPERTY(VisibleAnywhere)
	class USceneComponent* SpawnLocation;

	UPROPERTY(VisibleAnywhere, Category="자체설정")
	bool bTargetInRange = false; // 타겟이 감지 범위 안에 있는지 여부
	// 최대 체력
	UPROPERTY(EditAnywhere, Category="자체설정")
	float MaxHealth = 100.f;
	// 현재 체력
	UPROPERTY(EditAnywhere, Category="자체설정")
	float Health = 100.f;
	// 스폰할 적 클래스 배열
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "자체설정")
	TArray<TSubclassOf<class ABaseEnemy>> SpawningEnemyClasses;
	
	// 죽을 떄 생성할 이펙트 == 케스케이드
	UPROPERTY(EditDefaultsOnly, Category="자체설정")
	class UParticleSystem* DeathEffectCascade;
	// 아이템 드롭 함수
	void DropItemsAfterDead();
	// 죽고 나서 떨어질 아이템 배열
	UPROPERTY(EditAnywhere, Category="자체설정")
	TArray<TSubclassOf<AActor>> DropItems;
	
	// 스폰 딜레이
	UPROPERTY(EditAnywhere, Category = "자체설정")
	float SpawnDelay = 7.0f;

	// 스폰 타이머 관리용 핸들 
	FTimerHandle SpawnTimerHandle;
	
	void SpawnEnemy(); // 적 스폰 함수

	void Die();
	
	int32 SpawnEnemyId = 0; // 스폰한 적의 고유 ID를 위한 카운터
	
	// 스폰시 재생할 2D 사운드
	UPROPERTY(EditDefaultsOnly, Category="자체설정")
	class USoundBase* SpawnSound;
	// 죽을떄 재생할 2d 사운드
	UPROPERTY(EditDefaultsOnly, Category="자체설정")
	class USoundBase* DeadSound;
	
	// 피격용 Overlay 머티리얼 == 피격시 머터리얼 덧씌우기
	UPROPERTY(EditDefaultsOnly, Category = "자체설정")
	class UMaterialInterface* HitOverlayMaterial;
	FTimerHandle HitFlashTimerHandle;

	// 히트시 오버레이 덧씌울 함수
	void SetHitOverlay();
	// 오버레이를 다시 제거할 함수
	void ClearHitOverlay();
	
public:	
	AEnemySpawner();
	virtual void Tick(float DeltaTime) override;
	
	// 체력 바 위젯 컴포넌트
	UPROPERTY( EditDefaultsOnly, BlueprintReadOnly, Category = "자체설정")
	TObjectPtr<class UWidgetComponent> HealthBarWidget;

#if WITH_EDITOR
	// 에디터에서 프로퍼티가 변경될 때 호출됩니다.
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
};
