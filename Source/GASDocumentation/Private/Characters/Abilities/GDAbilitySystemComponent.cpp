// Copyright 2019 Dan Kestranek.


#include "GDAbilitySystemComponent.h"

void UGDAbilitySystemComponent::ReceiveDamage(UGDAbilitySystemComponent * SourceASC, float UnmitigatedDamage, float MitigatedDamage)
{
	ReceivedDamage.Broadcast(SourceASC, UnmitigatedDamage, MitigatedDamage);
}

void UGDAbilitySystemComponent::AbilityLocalInputPressed(int32 InputID)
{
    UE_LOG(LogTemp, Log, TEXT("AbilityLocalInputPressed:%d"), InputID);
    Super::AbilityLocalInputPressed(InputID);
}
