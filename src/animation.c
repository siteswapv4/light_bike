#include "xml.h"
#include "animation.h"

typedef struct ANI_Animation
{
	Uint64 time;
	SDL_FPoint position;
	SDL_FPoint scale;
	float rotation;
	float alpha;
	
	struct ANI_Animation* next;
}ANI_Animation;

typedef struct ANI_PlayingAnimation
{
	Uint64 start_time;
	ANI_Animation* animation;
	SDL_Texture* texture;
	SDL_FPoint position;
	void (*callback)(void* user_data);
	void* user_data;
	
	struct ANI_PlayingAnimation* next;
}ANI_PlayingAnimation;

static ANI_PlayingAnimation* ANI_animations_head = NULL;
static ANI_PlayingAnimation* ANI_animations_tail = NULL;

void ANI_DefaultAnimation(ANI_Animation* animation)
{
	animation->time = 0;
	animation->position.x = 0.0f;
	animation->position.y = 0.0f;
	animation->scale.x = 1.0f;
	animation->scale.y = 1.0f;
	animation->rotation = 0.0f;
	animation->alpha = 1.0f;
}

char* ANI_GetCharArrayFromXMLString(struct xml_string* source)
{
	char* res = SDL_calloc(xml_string_length(source) + 1, sizeof(char));
	xml_string_copy(source, (uint8_t*)res, xml_string_length(source));
	return res;
}

char* ANI_GetNodeName(struct xml_node* source)
{
	struct xml_string* str = xml_node_name(source);
	return ANI_GetCharArrayFromXMLString(str);
}

bool ANI_CheckNodeName(struct xml_node* source, const char* target)
{
	char* name = ANI_GetNodeName(source);
	bool res = !SDL_strcmp(name, target);
	SDL_free(name);
	return res;
}

char* ANI_GetNodeAttributeName(struct xml_node* source, size_t attribute)
{
	struct xml_string* str = xml_node_attribute_name(source, attribute);
	return ANI_GetCharArrayFromXMLString(str);
}

char* ANI_GetNodeAttributeContent(struct xml_node* source, size_t attribute)
{
	struct xml_string* str = xml_node_attribute_content(source, attribute);
	return ANI_GetCharArrayFromXMLString(str);
}

int ANI_GetNodeAttributeContentInt(struct xml_node* source, size_t attribute)
{
	char* str = ANI_GetNodeAttributeContent(source, attribute);
	int res = SDL_atoi(str);
	SDL_free(str);
	return res;
}

float ANI_GetNodeAttributeContentFloat(struct xml_node* source, size_t attribute)
{
	char* str = ANI_GetNodeAttributeContent(source, attribute);
	float res = SDL_atof(str);
	SDL_free(str);
	return res;
}

void ANI_DestroyAnimation(ANI_Animation* animation)
{
	if (!animation) { return; }
	ANI_Animation* iterator = animation->next;
	while (iterator)
	{
		ANI_Animation* temp = iterator->next;
		SDL_free(iterator);
		iterator = temp;
	}
	SDL_free(animation);
}

ANI_Animation* ANI_LoadAnimationFromConstMem(const void* buffer, size_t length)
{
	ANI_Animation* animation = SDL_calloc(1, sizeof(ANI_Animation));
	struct xml_document* document = xml_parse_document(buffer, length);
	if (!document)
	{
		SDL_Log("Failed to parse xml");
		goto error;
	}
	struct xml_node* root_node = xml_document_root(document);
	if (!ANI_CheckNodeName(root_node, "animation"))
	{
		SDL_Log("Root node is not 'animation'");
		goto error;
	}
	
	size_t num_states = xml_node_children(root_node);
	ANI_Animation* current_animation = animation;
	for (int i = 0; i < num_states; i++)
	{
		struct xml_node* state_node = xml_node_child(root_node, i);
		if (!ANI_CheckNodeName(state_node, "state"))
		{
			SDL_Log("Node is not 'state'");
			goto error;
		}
		
		ANI_DefaultAnimation(current_animation);
		size_t num_attributes = xml_node_attributes(state_node);
		for (int j = 0; j < num_attributes; j++)
		{
			char* attribute_name = ANI_GetNodeAttributeName(state_node, j);
			if (!SDL_strcmp(attribute_name, "time"))
			{
				current_animation->time = ANI_GetNodeAttributeContentInt(state_node, j);
			}
			else if (!SDL_strcmp(attribute_name, "x"))
			{
				current_animation->position.x = ANI_GetNodeAttributeContentFloat(state_node, j);
			}
			else if (!SDL_strcmp(attribute_name, "y"))
			{
				current_animation->position.y = ANI_GetNodeAttributeContentFloat(state_node, j);
			}
			else if (!SDL_strcmp(attribute_name, "scale-x"))
			{
				current_animation->scale.x = ANI_GetNodeAttributeContentFloat(state_node, j);
			}
			else if (!SDL_strcmp(attribute_name, "scale-y"))
			{
				current_animation->scale.y = ANI_GetNodeAttributeContentFloat(state_node, j);
			}
			else if (!SDL_strcmp(attribute_name, "alpha"))
			{
				current_animation->alpha = ANI_GetNodeAttributeContentFloat(state_node, j);
			}
			else if (!SDL_strcmp(attribute_name, "rotation"))
			{
				current_animation->rotation = ANI_GetNodeAttributeContentFloat(state_node, j);
			}
			SDL_free(attribute_name);
		}
		if (i != num_states - 1)
		{
			current_animation->next = SDL_calloc(1, sizeof(ANI_Animation));
			current_animation = current_animation->next;
		}
	}
	
	xml_document_free(document);

	return animation;
	
error:
	if (document) { xml_document_free(document); }
	if (animation) { ANI_DestroyAnimation(animation); }
	return NULL;
}

ANI_Animation* ANI_LoadAnimationFromFile(const char* path)
{
	size_t length;
	Uint8* buffer = SDL_LoadFile(path, &length);
	if (!buffer)
	{
		SDL_Log("%s", SDL_GetError());
		return NULL;
	}
	ANI_Animation* res = ANI_LoadAnimationFromConstMem(buffer, length);
	SDL_free(buffer);
	return res;
}

bool ANI_PlayAnimationWithCallback(ANI_Animation* animation, SDL_Texture* texture, SDL_FPoint position, Uint64 time, void (*callback)(void* user_data), void* user_data)
{
	ANI_PlayingAnimation* playing_animation = SDL_calloc(1, sizeof(ANI_PlayingAnimation));
	playing_animation->animation = animation;
	playing_animation->texture = texture;
	playing_animation->start_time = time;
	playing_animation->callback = callback;
	playing_animation->user_data = user_data;
	playing_animation->position = position;
	
	if (ANI_animations_tail)
	{
		ANI_animations_tail->next = playing_animation;
		ANI_animations_tail = playing_animation;
	}
	else
	{
		ANI_animations_head = ANI_animations_tail = playing_animation;
	}
	
	return true;
}

bool ANI_PlayAnimation(ANI_Animation* animation, SDL_Texture* texture, SDL_FPoint position, Uint64 time)
{
	return ANI_PlayAnimationWithCallback(animation, texture, position, time, NULL, NULL);
}

bool ANI_RenderAnimation(SDL_Renderer* renderer, ANI_PlayingAnimation* animation, Uint64 time)
{
	while ((animation->animation->next) && (animation->animation->next->time < (time - animation->start_time)))
	{
		animation->animation = animation->animation->next;
	}
	if (!animation->animation->next) { return false; }
	
	ANI_Animation* current = animation->animation;
	ANI_Animation* next = animation->animation->next;
	
	float coeff = ((time - animation->start_time) - current->time) / (float)(next->time - current->time);
	
	SDL_FPoint position = {current->position.x + (next->position.x - current->position.x) * coeff + animation->position.x, current->position.y + (next->position.y - current->position.y) * coeff + animation->position.y};
	SDL_FPoint scale = {current->scale.x + (next->scale.x - current->scale.x) * coeff, current->scale.y + (next->scale.y - current->scale.y) * coeff};
	float alpha = current->alpha + (next->alpha - current->alpha) * coeff;
	float rotation = current->rotation + (next->rotation - current->rotation) * coeff;
	SDL_FRect rect = {position.x - animation->texture->w * scale.x / 2, position.y - animation->texture->h * scale.y / 2, animation->texture->w * scale.x, animation->texture->h * scale.y};
	
	SDL_SetTextureAlphaMod(animation->texture, alpha * 255);
	SDL_RenderTextureRotated(renderer, animation->texture, NULL, &rect, rotation, NULL, SDL_FLIP_NONE);

	return true;
}

bool ANI_RenderAnimations(SDL_Renderer* renderer, Uint64 time)
{
	ANI_PlayingAnimation* prev = NULL;
	ANI_PlayingAnimation* iterator = ANI_animations_head;
	while (iterator)
	{
		if (!ANI_RenderAnimation(renderer, iterator, time))
		{
			if (iterator->callback) { iterator->callback(iterator->user_data); }
			ANI_PlayingAnimation* next = iterator->next;
			if (prev == NULL)
			{
				ANI_animations_head = next;
			}
			else
			{
				prev->next = next;
			}

			if (next == NULL) 
			{
				ANI_animations_tail = prev;
			}
			
			SDL_free(iterator);
			iterator = next;
		}
		else
		{
			iterator = iterator->next;
		}
	}
	
	return true;
}

void ANI_ClearAnimations()
{
	ANI_PlayingAnimation* iterator = ANI_animations_head;
	while (iterator)
	{
		ANI_PlayingAnimation* next = iterator->next;
		SDL_free(iterator);
		iterator = next;
	}
	
	ANI_animations_head = NULL;
	ANI_animations_tail = NULL;
}
