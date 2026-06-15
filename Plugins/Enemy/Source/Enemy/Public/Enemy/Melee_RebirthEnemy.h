#pragma once

#include "CoreMinimal.h"
#include "Enemy/BaseMeleeEnemy.h"
#include "Melee_RebirthEnemy.generated.h"


UCLASS()
class ENEMY_API AMelee_RebirthEnemy : public ABaseMeleeEnemy
{
	GENERATED_BODY()
	
protected:
	bool bIsActiveSoul = false; // 소울이 활성화 되었는지 여부
	bool bReviveFlag = true; // 부활 플래그
	
	// 소울 생성 위치 포인트 컴포넌트
	UPROPERTY(VisibleAnywhere)
	class USceneComponent* SoulPoint; 
	// 소울 원형 콜리전 컴포넌트
	UPROPERTY(VisibleAnywhere)
	class USphereComponent* SoulCollisionSphere;
	// 소울 나이아가라 이펙트 컴포넌트
	UPROPERTY(VisibleAnywhere)
	class UNiagaraComponent* SoulEffectNiagara;

	// 타이머를 제어할 핸들
	FTimerHandle ReviveTimerHandle;
	// 부활 횟수
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "자체설정")
	int32 ReviveCount = 1;
	// 소울 생성 후 부활까지 걸리는 시간 (초 단위)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "자체설정")
	float ReviveDelayTime = 5.0f;
	
	// 소울이 가진 채력(타수 기준)
	UPROPERTY(EditAnywhere, Category="자체설정")
	int32 SoulHpCount = 3; 

	// 부활 애니메이션 몽타주
	UPROPERTY(EditDefaultsOnly, Category = "자체설정")
	class UAnimMontage* ReviveMontage;
	// 부활 몽타주 끝난 후 호출되는 함수 
	UFUNCTION(BlueprintCallable)
	void AfterReviveMontageEnd(); 

	
public:
	AMelee_RebirthEnemy();
	virtual float TakeDamage(float DamageAmount, struct FDamageEvent const& DamageEvent,
		class AController* EventInstigator, AActor* DamageCauser) override;

	// 죽음 몽타주 끝난 후 호출되는 함수 - 애님 노티파이에서 호출 -> 부모꺼 안쓰고 수정
	//UFUNCTION(BlueprintCallable) // 오버라이드시에는 UFUNCTION 필요 없음
	virtual void AfterDieMontageEnd() override; 
	virtual void Die() override; // 죽음 처리 함수도 오버라이드해서 소울 생성 로직 추가
	
	// 부활 처리 함수
	void Revive();

	// 애니메이션 노티파이에서 호출할 함수: 소울 생성
	UFUNCTION(BlueprintCallable)
	void SpawnSoul();
	
#if WITH_EDITOR
	// 에디터에서 프로퍼티가 변경될 때 호출됩니다.
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
};
