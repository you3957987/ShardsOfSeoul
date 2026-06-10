#include "HUD/ConversationLog.h"

#include "Components/Button.h"

void UConversationLog::NativeConstruct()
{
	Super::NativeConstruct();

	if ( QutiButton ) QutiButton->OnClicked.AddDynamic(this, &UConversationLog::OnPressedQuitButton);
}

void UConversationLog::OnPressedQuitButton()
{
	SetVisibility( ESlateVisibility::Hidden );

	// 2. 외부(컴포넌트)에 알림 발송!
	if (OnCloseClicked.IsBound())
	{
		OnCloseClicked.Broadcast();
	}
}
