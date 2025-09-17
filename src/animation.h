#pragma once
#include <SDL3/SDL.h>

typedef struct ANI_Animation ANI_Animation;

ANI_Animation* ANI_LoadAnimationFromFile(const char* path);

ANI_Animation* ANI_LoadAnimationFromConstMem(const void* buffer, size_t length);

void ANI_DestroyAnimation(ANI_Animation* animation);

bool ANI_PlayAnimation(ANI_Animation* animation, SDL_Texture* texture, SDL_FPoint position, Uint64 time);

bool ANI_PlayAnimationWithCallback(ANI_Animation* animation, SDL_Texture* texture, SDL_FPoint position, Uint64 time, void (*callback)(void* user_data), void* user_data);

bool ANI_RenderAnimations(SDL_Renderer* renderer, Uint64 time);

void ANI_ClearAnimations();
