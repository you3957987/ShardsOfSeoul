#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "EnemyLogManager.generated.h"

// 로그의 종류를 정의하는 열거형 (파일 이름으로 매핑됨)
UENUM(BlueprintType)
enum class EEnemyLogType : uint8
{
	// 적 로그 =  대미지 얼마 주었는지 | 대미지 얼마 받았는지 + 사망 여부 | 전투 시작 시간, 전투 종료 시간 및 전투 시간 | 공격으로 사망 여부
	Melee,
	Ranged,
	Exploder,
	Transpar,
	Mimic,
	Slime,
	Mage,
	Guard,// 가드 여부 및 가드중 받은 대미지/임계치
	Passive,
	Burrow, // 언버로우 공격 적중 여부
	Revive, // 부활 여부
	Spawner, // 몬스터 스폰 했는지 | 몬스터 뭐 스폰했는지
	
	// 보스 로그 = 패턴 근/중/원 거리 및 패턴 이름 대미지 얼마 받았는지 | 패턴 적중 여부 | 전투 시작 ~ 끝 시간 | 공격으로 사망 여부
	SkeletonMage, // 파이어볼 적중 여부 | 뼈 장판 공격 여부 | 실드 캐스트 적중 여부 | 텔레포트 거리 | 소환시 적 이름 및 거리
	BlackKnight, // 일반, 가드반격, 차지 어택 적중 여부 | 차지 어택시 웨이브 번개 적중 여부 | 돌진 적중 여부 | 번개 소환 적중 여부 | 가드 여부 및 가드중 받은 대미지/임계치
	Worm, // 기본, 런지, 석션 공격 적중 여부 | 언버로우 적중 여부 | 화염구 충돌 및 장판 공격 | 화염 방사 적중 여부 | 빨아들이기 성공시 시간 
	MagicSwordMan // 기본, 띄우기, 공중, 점프 공격 적중 여부 | 검기 발사체 적중 여부 | 가드 여부 및 가드중 받은 대미지/임계치 | 궁극기 적중 여부 및 대미지
};

UCLASS()
class ENEMY_API UEnemyLogManager : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

	static FString GetLogFileName(EEnemyLogType InEnemyLogType);
	
public:

	static void EnemyLog(EEnemyLogType InEnemyLogType, FString Content);
	
};