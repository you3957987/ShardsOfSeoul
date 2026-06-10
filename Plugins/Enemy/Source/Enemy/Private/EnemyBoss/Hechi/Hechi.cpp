#include "EnemyBoss/Hechi/Hechi.h"

AHechi::AHechi()
{
}

void AHechi::BeginPlay()
{
	Super::BeginPlay();
	
	CommonBossLogData.BossID = BossLogId; // 로그 데이터에 보스 ID 기록
	
}

void AHechi::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

float AHechi::TakeDamage(float DamageAmount, struct FDamageEvent const& DamageEvent, class AController* EventInstigator,
	AActor* DamageCauser)
{
	return Super::TakeDamage(DamageAmount, DamageEvent, EventInstigator, DamageCauser);
}

void AHechi::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	
	// 변경된 프로퍼티의 이름을 가져옵니다.
	const FName PropertyName = (PropertyChangedEvent.Property != nullptr) ? PropertyChangedEvent.Property->GetFName() : NAME_None;
	
	// 디버그 모드에 따라 어택, 디텍트, 체이스 범위 구체의 가시성을 설정합니다.
	if (PropertyName == GET_MEMBER_NAME_CHECKED(AHechi, bDebugMode))
	{
		if ( bDebugMode == true )
		{

		}
		else
		{

		}
	}
}
