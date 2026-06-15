#include "Enemy/BaseRangedEnemy.h"
#include "EnemyProjectile/BaseEnemyProjectile.h"
#include "Kismet/KismetMathLibrary.h"

ABaseRangedEnemy::ABaseRangedEnemy()
{
	RangedAttackPoint = CreateDefaultSubobject<USceneComponent>(TEXT("RangedAttackPoint"));
	RangedAttackPoint->SetupAttachment(RootComponent); // 루트 컴포넌트

	EnemyType = EEnemyType::EET_Ranged;
}

// 발사체 발사 함수
void ABaseRangedEnemy::ShootProjectile()
{
	//UE_LOG(LogTemp, Warning, TEXT("BaseEnemyShootProjectileCall") );

	if (ProjectileClass && RangedAttackPoint && TargetCharacter)
	{
		const FVector SpawnLocation = RangedAttackPoint->GetComponentLocation();
		// RangedAttackPoint에서 TargetCharacter의 위치를 바라보는 회전값을 계산합니다.
		const FRotator SpawnRotation = UKismetMathLibrary::FindLookAtRotation(SpawnLocation,
			TargetCharacter->GetActorLocation());

		FActorSpawnParameters SpawnParams;
		SpawnParams.Owner = this;
		SpawnParams.Instigator = this;
		
		UWorld* World = GetWorld();
		if (World)
		{
			// 계산된 위치와 회전값으로 발사체를 스폰하고, 스폰된 액터의 포인터를 가져옵니다.
			ABaseEnemyProjectile* SpawnedProjectile =
				World->SpawnActor<ABaseEnemyProjectile>(ProjectileClass, SpawnLocation, SpawnRotation, SpawnParams);
			
			// 엄
		}
	}
}
