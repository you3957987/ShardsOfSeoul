#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "EnemyHealthBarWidget.generated.h"


UCLASS()
class ENEMY_API UEnemyHealthBarWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// 프로그레스바 바인드
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	class UProgressBar* HealthProgressBar;
};
