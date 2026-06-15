#pragma once

#include "CoreMinimal.h"
#include "BaseEnemy.h"
#include "BaseTransparEnemy.generated.h"

// 머티리얼 슬롯 인덱스와 해당 슬롯의 일반/투명 상태 머티리얼을 묶는 구조체입니다.
USTRUCT(BlueprintType)
struct FMaterialSlotTransparencyData
{
	GENERATED_BODY()

	// 머티리얼을 교체할 머티리얼 슬롯의 인덱스입니다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "자체설정")
	int32 MaterialSlotIndex = 0;

	// 불투명 상태일 때 사용할 머티리얼입니다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "자체설정")
	TObjectPtr<UMaterialInterface> OpaqueMaterial;

	// 반투명 상태일 때 사용할 머티리얼입니다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "자체설정")
	TObjectPtr<UMaterialInterface> TransparentMaterial;
};

// 교체할 메시 컴포넌트와 머티리얼 슬롯별 데이터를 묶는 구조체입니다.
USTRUCT(BlueprintType)
struct FTransparencyMaterialData
{
	GENERATED_BODY()

	// 머티리얼을 교체할 메시 컴포넌트의 이름입니다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "자체설정")
	FName MeshComponentName;

	// 이 메시 컴포넌트의 머티리얼 슬롯별 투명도 데이터 배열입니다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "자체설정")
	TArray<FMaterialSlotTransparencyData> MaterialSlots;
};

UCLASS()
class ENEMY_API ABaseTransparEnemy : public ABaseEnemy
{
	GENERATED_BODY()

protected:
	virtual void BeginPlay() override;

	// 투명화 로직 체크용 플래그
	UPROPERTY(EditAnywhere, Category="자체설정")
	bool bCheckTransparencyLogic = false;
	void TestTransparencyLogic(); // 테스트용 투명화 로직 함수
	// 현재 투명 상태인지 여부
	UPROPERTY(VisibleAnywhere, Category="자체설정")
	bool bIsTransparent = false;
	// 투명도 변경 이펙트
	UPROPERTY(EditDefaultsOnly, Category = "자체설정")
	class UNiagaraSystem* TransparChangeEffect; 
	// 투명 변경 이펙트 생성 앞 뒤 위치 조정용 거리
	UPROPERTY(EditDefaultsOnly, Category="자체설정")
	float TransEffectForwardOffset = 0.0f;
	// 투명 변경 이펙트 생성 위 아래 위치 조정용 거리
	UPROPERTY(EditDefaultsOnly, Category="자체설정")
	float TransEffectUpOffset = 0.0f;

	
	// 근접 공격 데미지
	UPROPERTY(EditAnywhere, Category="자체설정")
	float MeleeAttackDamage = 20.f; //
	// 근접 공격 지점 컴포넌트
	UPROPERTY(VisibleAnywhere)
	class USceneComponent* MeleeAttackPoint;
	// 근접 공격 범위
	UPROPERTY(VisibleAnywhere)
	class USphereComponent* AttackRangePointSphere;
	// 공격 히트 체크 함수
	void CheckMeleeAttackHit(float DeltaTime); 

public:
	ABaseTransparEnemy();
	virtual void Tick(float DeltaTime) override;

	// 공격시 투명화 해제
	virtual UAnimMontage* Attack() override;
	virtual float TakeDamage(float DamageAmount, struct FDamageEvent const& DamageEvent,
		class AController* EventInstigator, AActor* DamageCauser) override;
	
	// 이미 공격에 히트된 액터들을 저장하는 배열
	UPROPERTY()
	TArray<TObjectPtr<AActor>> HittedActors;
	// 근접 공격 중인지 여부를 나타내는 변수
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "자체설정")
	bool bIsMeleeAttacking = false; 
	// 근접 공격 범위 활성화 함수 - 애님 노티파이에서 호출
	UFUNCTION(BlueprintCallable)
	void AttackCheck_Start() { HittedActors.Empty();bIsMeleeAttacking = true;} ;
	// 근접 공격 범위 비활성화 함수 - 애님 노티파이에서 호출
	UFUNCTION(BlueprintCallable)
	void AttackCheck_End() { HittedActors.Empty();bIsMeleeAttacking = false; };
	
	
	// 투명도 머티리얼 데이터 배열. 메인 캐릭터 메시이름은 무조건 ChracterMesh0   <- 숫자 0임
	UPROPERTY(EditDefaultsOnly, Category = "자체설정")
	TArray<FTransparencyMaterialData> TransparencyData;
	// 캐릭터의 투명도 상태를 설정하는 함수
	UFUNCTION(BlueprintCallable)
	void SetCharacterTransparency(bool bMakeTransparent);

#if WITH_EDITOR
	// 에디터에서 프로퍼티가 변경될 때 호출됩니다.
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
};
