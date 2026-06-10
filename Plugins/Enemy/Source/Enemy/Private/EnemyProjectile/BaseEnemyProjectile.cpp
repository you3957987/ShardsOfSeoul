#include "EnemyProjectile/BaseEnemyProjectile.h"

#include "EnemyLogManager.h"
#include "NiagaraComponent.h"
#include "Components/SphereComponent.h"
#include "Enemy/BaseRangedEnemy.h"
#include "EnemyBoss/MagicSwordMan/BossMagicSwordMan.h"
#include "EnemyBoss/SkeletonMage/BossSkeletonMage.h"
#include "GameFramework/Character.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Kismet/GameplayStatics.h"

ABaseEnemyProjectile::ABaseEnemyProjectile()
{
	PrimaryActorTick.bCanEverTick = true;

	// 충돌 컴포넌트 생성 및 루트 컴포넌트로 설정
	CollisionComp = CreateDefaultSubobject<USphereComponent>(TEXT("CollisionComp"));
	RootComponent = CollisionComp;
	
	// 오버랩 이벤트 생성 활성화
	CollisionComp->SetGenerateOverlapEvents(true);
	// 콜리전 프리셋: Custom, 활성화: Query and Physics
	CollisionComp->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	// 오브젝트 타입: WorldDynamic
	CollisionComp->SetCollisionObjectType(ECollisionChannel::ECC_WorldDynamic);

	// 모든 채널에 대한 기본 반응을 '무시(Ignore)'로 설정
	CollisionComp->SetCollisionResponseToAllChannels(ECollisionResponse::ECR_Ignore);
	
	// 특정 채널에 대한 반응을 '블록(Block)'으로 설정
	CollisionComp->SetCollisionResponseToChannel(ECollisionChannel::ECC_Visibility, ECollisionResponse::ECR_Overlap);
	CollisionComp->SetCollisionResponseToChannel(ECollisionChannel::ECC_Pawn, ECollisionResponse::ECR_Overlap);
	CollisionComp->SetCollisionResponseToChannel(ECollisionChannel::ECC_WorldDynamic, ECollisionResponse::ECR_Overlap);
	CollisionComp->SetCollisionResponseToChannel(ECollisionChannel::ECC_WorldStatic, ECollisionResponse::ECR_Overlap);
	// 스케탈 메시 설정은 에디터에서 하기

	// 외형을 표시할 스태틱 메시 컴포넌트 생성
	MeshComp = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComp"));
	MeshComp->SetupAttachment(RootComponent); 

	// 메시 컴포넌트는 콜리전 없음
	MeshComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	// 나이아가라 이펙트 컴포넌트 생성
	NiagaraEffectComp = CreateDefaultSubobject<UNiagaraComponent>(TEXT("NiagaraEffectComp"));
	NiagaraEffectComp->SetupAttachment(RootComponent); 

	TrailEffectComp = CreateDefaultSubobject<UNiagaraComponent>(TEXT("TrailEffectComp"));
	TrailEffectComp->SetupAttachment(RootComponent);
	
	// 투사체 이동 컴포넌트 생성
	ProjectileMovement = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("ProjectileMovement"));
	ProjectileMovement->UpdatedComponent = CollisionComp; // 이동을 적용할 컴포넌트 설정
	ProjectileMovement->bRotationFollowsVelocity = true; // 발사체가 날아가는 방향으로 회전
	ProjectileMovement->ProjectileGravityScale = 0.0f; // 중력 영향을 받지 않도록 설정

	ProjectileMovement->InitialSpeed = ProjectileSpeed; // 초기 속도 설정
	ProjectileMovement->MaxSpeed = ProjectileSpeed; // 최대 속도 설정

	// 액터 태그 추가
	Tags.Add(FName("EnemyProjectile"));
}

void ABaseEnemyProjectile::BeginPlay()
{
	Super::BeginPlay();

	if (CollisionComp)
	{
		CollisionComp->OnComponentBeginOverlap.AddDynamic(this, &ABaseEnemyProjectile::OnOverlapBegin);
	}
}

void ABaseEnemyProjectile::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

void ABaseEnemyProjectile::CreateHitEffect()
{
	// 기본 구현은 아무 것도 하지 않습니다.
}

void ABaseEnemyProjectile::OnOverlapBegin(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
 UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	// 자기 자신, 소유자, 다른 적 투사체, 또는 다른 적 캐릭터와 충돌한 경우 무시합니다.
	// +@ 사운드 박스 같은 거에는 Ignore 추가해 줘야 함!!!!!!!!!!!!!!!!!!!
	if (!OtherActor || OtherActor == this || OtherActor == GetOwner() 
	 || OtherActor->ActorHasTag(FName("EnemyProjectile")) 
	 || OtherActor->ActorHasTag(FName("Enemy"))
	 || OtherActor->ActorHasTag(FName("Pet")) 
	 || OtherActor->ActorHasTag(FName("Ignore"))) // 이 줄을 추가하세요.
	{
		return;
	}

	// 충돌 대상이 플레이어이거나 월드(벽, 바닥 등)인지 확인합니다.
	const bool bIsPlayer = OtherActor->ActorHasTag(FName("Player"));
	const bool bIsWorldObject = OtherComp &&
	 (OtherComp->GetCollisionObjectType() == ECC_WorldStatic || OtherComp->GetCollisionObjectType() == ECC_WorldDynamic);

	// 플레이어나 월드 오브젝트가 아니면 충돌을 처리하지 않습니다.
	if (!bIsPlayer && !bIsWorldObject) return;

	// 충돌 대상 뭔지 로그 찍기
	UE_LOG(LogTemp, Warning, TEXT("Projectile overlapped with: %s"), *OtherActor->GetName());
	
	// 플레이어랑 충돌 시 대미지 넣는거 넣기!!!
	if (bIsPlayer)
	{
		// 1. 발사체의 소유자(Owner)를 가져와서 캐릭터인지 확인
		if (ACharacter* OwnerCharacter = Cast<ACharacter>(GetOwner()))
		{
			if (OwnerCharacter->GetMesh())
			{
				// 1. 스켈레탈 메쉬 에셋 이름 가져오기
				FString OwnerMeshName = OwnerCharacter->GetMesh()->GetSkeletalMeshAsset() ? 
					OwnerCharacter->GetMesh()->GetSkeletalMeshAsset()->GetName() : TEXT("NoMeshAsset");
				
				if (OwnerMeshName.Contains(TEXT("MagicSwordMan")))
				{
					ABossMagicSwordMan* BossOwner = Cast<ABossMagicSwordMan>(GetOwner());
					
					if ( BossOwner )
					{
						BossOwner->BossMagicSwordManLogData.BladeWaveAttackDamage += Damage; // 로그 데이터에 입힌 대미지 누적
						BossOwner->CommonBossLogData.TotalDamageDealt += Damage; // 공통 로그 데이터에도 누적
					}
					
					UEnemyLogManager::EnemyLog(EEnemyLogType::MagicSwordMan, FString::Printf(TEXT("[소드맨] 검기가 [%.f] 대미지 줌"), Damage));
				}
				else if ( OwnerMeshName.Contains(TEXT("Skeleton_Mage")) )
				{
					UEnemyLogManager::EnemyLog(EEnemyLogType::SkeletonMage, FString::Printf(TEXT("[스켈레톤 메이지] 마법구 충돌 [%.f] 대미지 줌"), Damage ));
					
					ABossSkeletonMage* BossOwner = Cast<ABossSkeletonMage>(GetOwner());
					
					if ( BossOwner )
					{
						BossOwner->BossSkeletonMageLogData.FireBallDamage += Damage; // 로그 데이터에 입힌 대미지 누적
						BossOwner->CommonBossLogData.TotalDamageDealt += Damage; // 공통 로그 데이터에도 누적
					}
					
				}
				else
				{
					
					ABaseEnemy* EnemyOwner = Cast<ABaseEnemy>(GetOwner());
					
					if ( EnemyOwner ) EnemyOwner->EnemyLogData.TotalDamageDealt += Damage; // 로그 데이터에 입힌 대미지 누적
					
					ABaseRangedEnemy* RangedCharacter = Cast<ABaseRangedEnemy>(GetOwner());
					
					UEnemyLogManager::EnemyLog( RangedCharacter->GetEnemyType() == EEnemyType::EET_Ranged ? EEnemyLogType::Ranged : EEnemyLogType::Revive,
						FString::Printf(TEXT("적 [%s] 발사체가 [%.f] 대미지 줌"),
						*OwnerMeshName, Damage));
				}
			}
		}
		
		UE_LOG(LogTemp, Warning, TEXT("Projectile hit Player: %s"), *OtherActor->GetName());
		UGameplayStatics::ApplyDamage(
		   OtherActor,
		   Damage, // 헤더 파일에 선언된 대미지 변수
		   GetOwner() ? GetOwner()->GetInstigatorController() : nullptr,
		   this,
		   UDamageType::StaticClass()
		  );
	}
	
	// SweepResult에서 충돌 위치를 가져옵니다.
	EffectCreateLocation = SweepResult.ImpactPoint;
	CreateHitEffect();

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
}

#if WITH_EDITOR
void ABaseEnemyProjectile::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	// 변경된 프로퍼티의 이름을 가져옵니다.
	const FName PropertyName = (PropertyChangedEvent.Property != nullptr) ? PropertyChangedEvent.Property->GetFName() : NAME_None;
	
	// AttackRange 프로퍼티가 변경되었는지 확인합니다.
	if (PropertyName == GET_MEMBER_NAME_CHECKED(ABaseEnemyProjectile, ProjectileSpeed))
	{
		if (ProjectileMovement)
		{
			// AttackRangeSphere의 반지름을 AttackRange 값으로 설정합니다.
			ProjectileMovement->InitialSpeed = ProjectileSpeed;
			ProjectileMovement->MaxSpeed = ProjectileSpeed;
		}
	}
}
#endif