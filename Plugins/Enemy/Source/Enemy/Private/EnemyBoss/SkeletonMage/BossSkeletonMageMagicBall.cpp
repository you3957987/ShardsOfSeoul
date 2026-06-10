#include "EnemyBoss/SkeletonMage/BossSkeletonMageMagicBall.h"

#include "EnemyLogManager.h"
#include "NiagaraFunctionLibrary.h"

ABossSkeletonMageMagicBall::ABossSkeletonMageMagicBall()
{
	bOnlyNiagaraEffect = true;
}

void ABossSkeletonMageMagicBall::CreateHitEffect()
{

	if (HitEffect && GetWorld() && EffectCreateLocation != FVector::ZeroVector)
	{
		// 시계 방향으로 90도 회전한 값으로 이펙트를 생성합니다.
		const FRotator EffectRotation(90.f, 0.f, 0.f);
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(GetWorld(), HitEffect, EffectCreateLocation, EffectRotation);
	}
	
	
	Super::Destroyed();
}


