#pragma once

#include "CoreMinimal.h"
#include "EnemyBoss/BaseBossEnemy.h"
#include "Hechi.generated.h"

USTRUCT(BlueprintType)
struct FBossHechiManAttackStruct
{
	GENERATED_BODY()
	
	
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
	
	// 공격 가중치 구조체
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "자체설정")
	FBossHechiManAttackStruct AttackStruct;
	
	// 헤치 전용 로그데이터
	UPROPERTY(BlueprintReadOnly)
	FHechiLogData HechiLogData;
	
	
	
	
#if WITH_EDITOR
	// 에디터에서 프로퍼티가 변경될 때 호출됩니다.
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
};
