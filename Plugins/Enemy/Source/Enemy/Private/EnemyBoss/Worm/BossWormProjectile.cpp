#include "EnemyBoss/Worm/BossWormProjectile.h"

#include "EnemyLogManager.h"
#include "NiagaraFunctionLibrary.h"
#include "Components/SphereComponent.h"
#include "EnemyProjectile/DamageZoneProjectile.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Kismet/GameplayStatics.h"

ABossWormProjectile::ABossWormProjectile()
{
	bOnlyNiagaraEffect = false;
	
	// 중력 영향 받도록 설정
	ProjectileMovement->ProjectileGravityScale = 1.0f;
}

void ABossWormProjectile::CreateHitEffect()
{
	
	if (HitEffect && GetWorld() && EffectCreateLocation != FVector::ZeroVector)
	{
		// 시계 방향으로 90도 회전한 값으로 이펙트를 생성합니다.
		const FRotator EffectRotation(90.f, 0.f, 0.f);
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(GetWorld(), HitEffect, EffectCreateLocation, EffectRotation);
	}
	
	
	Super::Destroyed();
}

// 맵( 특히 바닥에 ) 오버랩 이벤트 설정을 트루로 바꿔줘야 함!!!!!!!!!!!!!
// 맵( 특히 바닥에 ) 오버랩 이벤트 설정을 트루로 바꿔줘야 함!!!!!!!!!!!!!
// 맵( 특히 바닥에 ) 오버랩 이벤트 설정을 트루로 바꿔줘야 함!!!!!!!!!!!!!
// 맵( 특히 바닥에 ) 오버랩 이벤트 설정을 트루로 바꿔줘야 함!!!!!!!!!!!!!
// 맵( 특히 바닥에 ) 오버랩 이벤트 설정을 트루로 바꿔줘야 함!!!!!!!!!!!!!
// 맵( 특히 바닥에 ) 오버랩 이벤트 설정을 트루로 바꿔줘야 함!!!!!!!!!!!!!
void ABossWormProjectile::OnOverlapBegin(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	// 자기 자신, 소유자, 다른 적 투사체, 또는 다른 적 캐릭터와 충돌한 경우 무시합니다.
	if (!OtherActor || OtherActor == this || OtherActor == GetOwner() 
	 || OtherActor->ActorHasTag(FName("EnemyProjectile")) 
	 || OtherActor->ActorHasTag(FName("Enemy"))
	 || OtherActor->ActorHasTag(FName("Pet")) ) // 이 줄을 추가하세요.
	{
		return;
	}
	
	EffectCreateLocation = SweepResult.ImpactPoint;
	
	// 충돌 대상 로그
	UE_LOG(LogTemp, Warning, TEXT("Projectile overlapped with: %s"), *OtherActor->GetName());
	// [추가] 충돌 지점에 디버그 스피어 그리기 (반지름 30, 빨간색, 1초간 유지)
	//if (GetWorld()) DrawDebugSphere(GetWorld(), SweepResult.ImpactPoint, 30.0f, 12, FColor::Red, false, 1.0f);
	
	// 충돌 대상이 플레이어이거나 월드(벽, 바닥 등)인지 확인합니다.
	const bool bIsPlayer = OtherActor->ActorHasTag(FName("Player"));
	
	// 플레이어랑 충돌 시 대미지 넣는거 넣기!!!
	if (bIsPlayer)
	{
		UE_LOG(LogTemp, Warning, TEXT("Projectile hit Player: %s"), *OtherActor->GetName());
		UGameplayStatics::ApplyDamage(
		   OtherActor,
		   Damage, // 헤더 파일에 선언된 대미지 변수
		   GetOwner() ? GetOwner()->GetInstigatorController() : nullptr,
		   this,
		   UDamageType::StaticClass()
		  );
		
		UEnemyLogManager::EnemyLog(EEnemyLogType::Worm, FString::Printf(TEXT("[웜] 화염구 적중 | 대미지[%.f]"), Damage));
		
		// 캐릭터에만 맞으면 충돌 이펙트 생성
		CreateHitEffect();
	}
	
	CreateDamageZoneOnGround();
	
	// 메시가 없고 나이아가라 이펙트만 있는 경우 액터를 즉시 파괴합니다.
	if (bOnlyNiagaraEffect)
	{
		Destroy();
		return;
	}
	// 트레일 이펙트가 충분히 나올떄 까지 파괴 대기

	// 추가적인 충돌 및 상호작용을 방지합니다.
	CollisionComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	ProjectileMovement->StopMovementImmediately();
	if (MeshComp)
	{
		MeshComp->SetVisibility(false);
	}

	// 트레일 이펙트가 사라질 시간을 주기 위해 2초 후에 액터를 파괴합니다.
	SetLifeSpan(2.0f);
	
	// 부모꺼 안씀
	//Super::OnOverlapBegin(OverlappedComponent, OtherActor, OtherComp, OtherBodyIndex, bFromSweep, SweepResult);
}

void ABossWormProjectile::CreateDamageZoneOnGround()
{
	//EffectCreateLocation 에서 바닥을 향해 라인 트레이스를 수행하여 충돌 지점을 찾습니다.
	FHitResult HitResult;
	FVector StartLocation = EffectCreateLocation + FVector(0.f, 0.f, 50.f);
	FVector EndLocation = StartLocation - FVector(0.f, 0.f, 1000.f); // 1000 유닛 아래로 라인 트레이스
	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(this); // 자신은 무시
	QueryParams.AddIgnoredActor(GetOwner()); // 소유자도 무시
	
	// 라인 트레이스 수행
	if (GetWorld()->LineTraceSingleByChannel(HitResult, StartLocation, EndLocation, ECC_Visibility, QueryParams))
	{
		// 충돌 지점에 파란색 디버그 스피어 그리기 (반지름 50, 12, 1초간 유지)
		//DrawDebugSphere(GetWorld(), HitResult.ImpactPoint, 50.0f, 12, FColor::Blue, false, 1.0f);
		
		if ( DamageZoneProjectileClass )
		{
			FActorSpawnParameters SpawnParams;
			SpawnParams.Owner = GetOwner();
			SpawnParams.Instigator = GetInstigator();
			
			// 충돌 지점에 장판 발사체 스폰
			GetWorld()->SpawnActor<ADamageZoneProjectile>(DamageZoneProjectileClass, 
				HitResult.ImpactPoint, FRotator::ZeroRotator, SpawnParams);
		}
	}
}
