#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "BossHealthBarWidget.generated.h"


UCLASS()
class ENEMY_API UBossHealthBarWidget : public UUserWidget
{
	GENERATED_BODY()
	
public:
	// 프로그레스바 바인드
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	class UProgressBar* HealthProgressBar;
	
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	class UTextBlock* BossNameText;
	
	UPROPERTY(Transient, meta = (BindWidgetAnim))
	UWidgetAnimation* FadeInAnim;
	UPROPERTY(Transient, meta = (BindWidgetAnim))
	UWidgetAnimation* FadeOutAnim;
};
