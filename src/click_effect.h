#pragma once
#include "common.h"

// Initialize click effect system and window
void InitClickEffect(HINSTANCE hInstance);

// Clean up click effect resources
void ShutdownClickEffect();

// Enable or disable click effect overlay and low-level mouse hook
void EnableClickEffect(bool enable);

// Check if click effect is currently enabled
bool IsClickEffectEnabled();
