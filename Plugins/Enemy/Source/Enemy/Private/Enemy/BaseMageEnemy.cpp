#include "Enemy/BaseMageEnemy.h"

#include "EnemyProjectile/BaseDelayedBurstProjectile.h"
#include "EnemyProjectile/DamageZoneProjectile.h"

ABaseMageEnemy::ABaseMageEnemy()
{
	EnemyType = EEnemyType::EET_Mage; // 마법사 타입으로 설정
}


void ABaseMageEnemy::CastFogAttack()
{
	// TargetCharacter가 유효한지 확인 (부모 클래스 ABaseEnemy의 멤버 변수)
	if (TargetCharacter && DamageZoneProjectileClass)
	{
		// 타깃 위치에 장판 발사체 스폰
		FActorSpawnParameters SpawnParams;
		SpawnParams.Owner = this; // 발사체의 소유자를 현재 적으로 설정
		SpawnParams.Instigator = GetInstigator(); // 발사체의 인스티게이터를 현재 적의 인스티게이터로 설정
		
		GetWorld()->SpawnActor<ADamageZoneProjectile>(
			DamageZoneProjectileClass, 
			TargetCharacter->GetActorLocation(), 
			FRotator::ZeroRotator, 
			SpawnParams);
	}
}

void ABaseMageEnemy::CastExplosionAttack()
{
	if (TargetCharacter && ExplosionProjectileClass)
	{
		FActorSpawnParameters SpawnParams;
		SpawnParams.Owner = this;
		SpawnParams.Instigator = GetInstigator();
		
		// 타깃 캐릭터 위치에다가 발사체 스폰 
		
		GetWorld()->SpawnActor<ABaseDelayedBurstProjectile>(
			ExplosionProjectileClass, 
			TargetCharacter->GetActorLocation(), 
			FRotator::ZeroRotator, 
			SpawnParams
		);
	}

}

