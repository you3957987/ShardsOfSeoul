#include "EnemyBoss/Hechi/HechiMagicBallProjectile.h"

#include "NiagaraFunctionLibrary.h"

AHechiMagicBallProjectile::AHechiMagicBallProjectile()
{
	bOnlyNiagaraEffect = true;
}

void AHechiMagicBallProjectile::CreateHitEffect()
{
	UE_LOG(LogTemp, Log, TEXT("HechiMagicBallProjectile: CreateHitEffect called"));
	if (HitEffect && GetWorld() && EffectCreateLocation != FVector::ZeroVector)
	{
		// 시계 방향으로 90도 회전한 값으로 이펙트를 생성합니다.
		const FRotator EffectRotation(0.f, 90.f, 180.f);
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(GetWorld(), HitEffect, EffectCreateLocation, EffectRotation);
	}
	
	
	Super::Destroyed();
}