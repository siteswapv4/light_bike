#define SDL_MAIN_USE_CALLBACKS
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include "animation.h"

#define TRON_LOGICAL_WIDTH 1920
#define TRON_LOGICAL_HEIGHT 1080
#define TRON_TITLE_SCALE 10.0f
#define TRON_PLAYER_CHOICE_SCALE 5.0f
#define TRON_MAX_BIKES 4
#define TRON_BIKE_WIDTH 50
#define TRON_BIKE_HEIGHT 70
#define TRON_BIKE_SPEED 4.0f
#define TRON_TRAIL_SIZE 10.0f
#define TRON_TURN_COOLDOWN 50

typedef enum TRON_Direction
{
    TRON_NORTH,
    TRON_EAST,
    TRON_SOUTH,
    TRON_WEST
}TRON_Direction;

typedef struct TRON_Bike
{
    SDL_FPoint position;
    TRON_Direction direction;
    float speed;
    SDL_Color color;
    SDL_FPoint* trail_points;
    int num_trail_points;
    SDL_Color trail_color;
    Sint64 last_turn;
    bool dead;
}TRON_Bike;

typedef struct TRON_AppState
{
    SDL_Window* window;
    SDL_Renderer* renderer;
    SDL_Scancode keys[TRON_MAX_BIKES][4];
    TRON_Bike* bikes;
    int num_bikes;
    bool game_started;
    bool game_ended;
    bool hide_menu;
    int player_choice;
}TRON_AppState;

static const char* TRON_DEATH_TEXT_XML =
"<animation>"
"<state time='0' scale-x='0' scale-y='0'/>"
"<state time='500' scale-x='10' scale-y='10' rotation='720'/>"
"<state time='1500' scale-x='10' scale-y='10' rotation='720'/>"
"</animation>";

static const char* TRON_START_XML = 
"<animation>"
"<state time='0' scale-x='0' scale-y='0'/>"
"<state time='150' scale-x='11' scale-y='12'/>"
"<state time='200' scale-x='9' scale-y='8'/>"
"<state time='250' scale-x='10' scale-y='10'/>"
"<state time='1000' scale-x='10' scale-y='10'/>"
"<state />"
"</animation>";

static const SDL_Scancode TRON_DEFAULT_KEYS[TRON_MAX_BIKES][4] =
{
    { SDL_SCANCODE_W, SDL_SCANCODE_D, SDL_SCANCODE_S, SDL_SCANCODE_A },
    { SDL_SCANCODE_UP, SDL_SCANCODE_RIGHT, SDL_SCANCODE_DOWN, SDL_SCANCODE_LEFT },
    { SDL_SCANCODE_I, SDL_SCANCODE_L, SDL_SCANCODE_K, SDL_SCANCODE_J },
    { SDL_SCANCODE_T, SDL_SCANCODE_H, SDL_SCANCODE_G, SDL_SCANCODE_F }
};

static SDL_Texture* TRON_title_text = NULL;
static SDL_Texture* TRON_draw_text = NULL;
static SDL_Texture* TRON_start_text = NULL;
static SDL_Texture* TRON_player_texts[TRON_MAX_BIKES] = {0};
static SDL_Texture* TRON_death_texts[TRON_MAX_BIKES] = {0};
static SDL_Texture* TRON_win_texts[TRON_MAX_BIKES] = {0};

static ANI_Animation* TRON_death_text_animation = NULL;
static ANI_Animation* TRON_start_animation = NULL;

SDL_Texture* TRON_CreateText(SDL_Renderer* renderer, const char* text)
{
    size_t length = SDL_strlen(text);
    SDL_Texture* texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_TARGET, SDL_DEBUG_TEXT_FONT_CHARACTER_SIZE * length, SDL_DEBUG_TEXT_FONT_CHARACTER_SIZE);
    SDL_SetTextureScaleMode(texture, SDL_SCALEMODE_NEAREST);
    SDL_SetRenderTarget(renderer, texture);
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    SDL_RenderDebugText(renderer, 0.0f, 0.0f, text);
    SDL_SetRenderTarget(renderer, NULL);

    return texture;
}

void TRON_CenterRect(SDL_FRect* rect)
{
    rect->x -= rect->w / 2.0f;
    rect->y -= rect->h / 2.0f;
}

SDL_FPoint TRON_GetLogicalCenter()
{
    return (SDL_FPoint){TRON_LOGICAL_WIDTH / 2.0f, TRON_LOGICAL_HEIGHT / 2.0f};
}

SDL_FRect TRON_GetBikeRect(TRON_Bike* bike)
{
    if ((bike->direction == TRON_NORTH) || (bike->direction == TRON_SOUTH))
    {
        return (SDL_FRect){bike->position.x - TRON_BIKE_WIDTH / 2, bike->position.y - TRON_BIKE_HEIGHT / 2, TRON_BIKE_WIDTH, TRON_BIKE_HEIGHT};
    }
    else
    {
        return (SDL_FRect){bike->position.x - TRON_BIKE_HEIGHT / 2, bike->position.y - TRON_BIKE_WIDTH / 2, TRON_BIKE_HEIGHT, TRON_BIKE_WIDTH};
    }
}

TRON_Bike* TRON_CreateBikes(SDL_Renderer* renderer)
{
    SDL_FPoint positions[TRON_MAX_BIKES] = {{100.0f, 100.0f}, {TRON_LOGICAL_WIDTH - 100.0f, 100.0f}, {100.0f, TRON_LOGICAL_HEIGHT - 100.0f}, {TRON_LOGICAL_WIDTH - 100.0f, TRON_LOGICAL_HEIGHT - 100.0f}};
    TRON_Direction directions[TRON_MAX_BIKES] = {TRON_SOUTH, TRON_SOUTH, TRON_NORTH, TRON_NORTH};
    SDL_Color colors[TRON_MAX_BIKES] = {{255, 0, 0, 255}, {0, 255, 0, 255}, {0, 0, 255, 255}, {255, 255, 0, 255}};
    TRON_Bike* bikes = SDL_calloc(TRON_MAX_BIKES, sizeof(TRON_Bike));

    for (int i = 0; i < TRON_MAX_BIKES; i++)
    {
        bikes[i].position = positions[i];
        bikes[i].direction = directions[i];
        bikes[i].speed = TRON_BIKE_SPEED;
        bikes[i].num_trail_points = 1;
        bikes[i].trail_points = SDL_malloc(2 * sizeof(SDL_FPoint));
        bikes[i].trail_points[0] = positions[i];
        bikes[i].num_trail_points = 2;
        bikes[i].trail_points[1] = positions[i];
        bikes[i].trail_color = colors[i];
        bikes[i].color = colors[i];
    }

    return bikes;
}

void TRON_DestroyBikes(TRON_Bike* bikes)
{
    for (int i = 0; i < TRON_MAX_BIKES; i++)
    {
        SDL_free(bikes[i].trail_points);
    }
    SDL_free(bikes);
}

void TRON_MoveBike(TRON_Bike* bike, float speed)
{
    switch (bike->direction)
    {
        case TRON_NORTH:
            bike->position.y -= speed;
            break;
        case TRON_SOUTH:
            bike->position.y += speed;
            break;
        case TRON_WEST:
            bike->position.x -= speed;
            break;
        case TRON_EAST:
            bike->position.x += speed;
            break;
    }

    bike->trail_points[bike->num_trail_points - 1] = bike->position;
}

void TRON_MoveBikes(TRON_Bike* bikes, int num_bikes)
{
    for (int i = 0; i < num_bikes; i++)
    {
        if (!bikes[i].dead)
        {
            TRON_MoveBike(&bikes[i], bikes[i].speed);
        }
    }
}

bool TRON_TurnBike(TRON_Bike* bike, TRON_Direction direction)
{
    if (!bike->dead)
    {
        if ((direction != bike->direction) && (direction != (bike->direction + 2) % 4))
        {
            bike->num_trail_points++;
            bike->trail_points = SDL_realloc(bike->trail_points, bike->num_trail_points * sizeof(SDL_FPoint));
            bike->trail_points[bike->num_trail_points - 1] = bike->position;
            bike->direction = direction;
            TRON_MoveBike(bike, direction % 2 == 0 ? TRON_BIKE_HEIGHT / 4.0f : TRON_BIKE_WIDTH / 4.0f);
            return true;
        }
    }

    return false;
}

void TRON_RenderBikeTrail(SDL_Renderer* renderer, TRON_Bike* bike)
{
    SDL_SetRenderDrawColor(renderer, bike->trail_color.r, bike->trail_color.g, bike->trail_color.b, bike->trail_color.a);
    SDL_SetRenderScale(renderer, TRON_TRAIL_SIZE, TRON_TRAIL_SIZE);
    for (int i = 1; i < bike->num_trail_points; i++)
    {
        SDL_RenderLine(renderer, (bike->trail_points[i - 1].x - TRON_TRAIL_SIZE / 2) / TRON_TRAIL_SIZE,
                                 (bike->trail_points[i - 1].y - TRON_TRAIL_SIZE / 2) / TRON_TRAIL_SIZE,
                                 (bike->trail_points[i - 0].x - TRON_TRAIL_SIZE / 2) / TRON_TRAIL_SIZE,
                                 (bike->trail_points[i - 0].y - TRON_TRAIL_SIZE / 2) / TRON_TRAIL_SIZE);
    }
    SDL_SetRenderScale(renderer, 1.0f, 1.0f);
}

void TRON_RenderBike(SDL_Renderer* renderer, TRON_Bike* bike)
{
    SDL_SetRenderDrawColor(renderer, bike->color.r, bike->color.g, bike->color.b, bike->color.a);
    SDL_FRect rect = TRON_GetBikeRect(bike);
    SDL_RenderFillRect(renderer, &rect);
}

void TRON_RenderBikes(SDL_Renderer* renderer, TRON_Bike* bikes, int num_bikes)
{
    for (int i = 0; i < num_bikes; i++)
    {
        if (!bikes[i].dead)
        {
            TRON_RenderBikeTrail(renderer, &bikes[i]);
        }
    }
    for (int i = 0; i < num_bikes; i++)
    {
        if (!bikes[i].dead)
        {
            TRON_RenderBike(renderer, &bikes[i]);
        }
    }
}

void TRON_GameplayKeyDown(TRON_AppState* app, SDL_Event* event)
{
    for (int i = 0; i < app->num_bikes; i++)
    {
        for (int j = 0; j < 4; j++)
        {
            if (event->key.scancode == app->keys[i][j])
            {
                if (SDL_GetTicks() - app->bikes[i].last_turn > TRON_TURN_COOLDOWN)
                {
                    if (TRON_TurnBike(&app->bikes[i], j))
                    {
                        app->bikes[i].last_turn = SDL_GetTicks();
                    }
                }
            }
        }
    }
}

void TRON_StartCallback(void* userdata)
{
    TRON_AppState* app = userdata;
    app->game_started = true;
    app->hide_menu = false;
}

bool TRON_MenuKeyDown(TRON_AppState* app, SDL_Event* event)
{
    if (event->key.scancode == SDL_SCANCODE_DOWN)
    {
        app->player_choice++;
        app->player_choice = SDL_clamp(app->player_choice, 0, TRON_MAX_BIKES - 1);
    }
    else if (event->key.scancode == SDL_SCANCODE_UP)
    {
        app->player_choice--;
        app->player_choice = SDL_clamp(app->player_choice, 0, TRON_MAX_BIKES - 1);
    }
    else if (event->key.scancode == SDL_SCANCODE_RETURN)
    {
        if (app->player_choice == 3) { return false; }
        app->num_bikes = app->player_choice + 2;
        app->hide_menu = true;
        ANI_PlayAnimationWithCallback(TRON_start_animation, TRON_start_text, TRON_GetLogicalCenter(), SDL_GetTicks(), TRON_StartCallback, app);
    }

    return true;
}

bool TRON_RectIntersectsTrail(SDL_FRect* rect, SDL_FPoint p1, SDL_FPoint p2)
{
    SDL_FRect seg_rect;
    if (p1.x == p2.x)
    {
        seg_rect.x = p1.x - TRON_TRAIL_SIZE / 2;
        seg_rect.y = SDL_min(p1.y, p2.y);
        seg_rect.w = TRON_TRAIL_SIZE;
        seg_rect.h = SDL_fabsf(p2.y - p1.y);
    }
    else
    {
        seg_rect.x = SDL_min(p1.x, p2.x);
        seg_rect.y = p1.y - TRON_TRAIL_SIZE / 2;
        seg_rect.w = SDL_fabsf(p2.x - p1.x);
        seg_rect.h = TRON_TRAIL_SIZE;
    }
    return SDL_HasRectIntersectionFloat(rect, &seg_rect);
}

bool TRON_CheckBikeCollision(TRON_Bike* b1, TRON_Bike* b2)
{
    SDL_FRect r1 = TRON_GetBikeRect(b1);
    SDL_FRect r2 = TRON_GetBikeRect(b2);

    if ((r1.x < 0) || (r1.y < 0) || (r1.x + r1.w > TRON_LOGICAL_WIDTH) || (r1.y + r1.h > TRON_LOGICAL_HEIGHT))
    {
        return true;
    }
    if ((b1->direction != b2->direction) && (b1->direction % 2 == b2->direction % 2) && (SDL_HasRectIntersectionFloat(&r1, &r2)))
    {
        return true;
    }
    for (int i = 1; i < b2->num_trail_points; i++)
    {
        if (TRON_RectIntersectsTrail(&r1, b2->trail_points[i-1], b2->trail_points[i]))
        {
            return true;
        }
    }

    return false;
}

void TRON_CheckBikesCollisions(TRON_Bike* bikes, int num_bikes)
{
    bool dead[TRON_MAX_BIKES] = {0};
    for (int i = 0; i < num_bikes; i++)
    {
        for (int j = 0; j < num_bikes; j++)
        {
            if ((!bikes[i].dead) && (!bikes[j].dead))
            {
                if (i == j)
                {
                    bikes[i].num_trail_points -= 2;
                }
                if (TRON_CheckBikeCollision(&bikes[i], &bikes[j]))
                {
                    dead[i] = true;
                    ANI_ClearAnimations();
                    ANI_PlayAnimation(TRON_death_text_animation, TRON_death_texts[i], TRON_GetLogicalCenter(), SDL_GetTicks());
                }
                if (i == j)
                {
                    bikes[i].num_trail_points += 2;
                }
            }
        }
    }
    for (int i = 0; i < num_bikes; i++)
    {
        if (dead[i])
        {
            bikes[i].dead = dead[i];
        }
    }
}

void TRON_CreateTexts(SDL_Renderer* renderer)
{
    TRON_player_texts[0] = TRON_CreateText(renderer, "2 player");
    TRON_player_texts[1] = TRON_CreateText(renderer, "3 player");
    TRON_player_texts[2] = TRON_CreateText(renderer, "4 player");
    TRON_player_texts[3] = TRON_CreateText(renderer, "Quit");

    TRON_death_texts[0] = TRON_CreateText(renderer, "Player 1 Died !");
    TRON_death_texts[1] = TRON_CreateText(renderer, "Player 2 Died !");
    TRON_death_texts[2] = TRON_CreateText(renderer, "Player 3 Died !");
    TRON_death_texts[3] = TRON_CreateText(renderer, "Player 4 Died !");

    TRON_win_texts[0] = TRON_CreateText(renderer, "Player 1 Wins !");
    TRON_win_texts[1] = TRON_CreateText(renderer, "Player 2 Wins !");
    TRON_win_texts[2] = TRON_CreateText(renderer, "Player 3 Wins !");
    TRON_win_texts[3] = TRON_CreateText(renderer, "Player 4 Wins !");

    TRON_title_text = TRON_CreateText(renderer, "Light Bike");
    TRON_draw_text = TRON_CreateText(renderer, "Draw !");
    TRON_start_text = TRON_CreateText(renderer, "Starting Game !");
}

void TRON_DestroyTexts()
{
    for (int i = 0; i < TRON_MAX_BIKES; i++) { SDL_DestroyTexture(TRON_player_texts[i]); }
    for (int i = 0; i < TRON_MAX_BIKES; i++) { SDL_DestroyTexture(TRON_death_texts[i]); }
    for (int i = 0; i < TRON_MAX_BIKES; i++) { SDL_DestroyTexture(TRON_win_texts[i]); }
    SDL_DestroyTexture(TRON_title_text);
    SDL_DestroyTexture(TRON_draw_text);
    SDL_DestroyTexture(TRON_start_text);
}

int TRON_CountAliveBikes(TRON_Bike* bikes, int num_bikes)
{
    int count = 0;
    for (int i = 0; i < num_bikes; i++)
    {
        if (!bikes[i].dead) { count++; }
    }
    return count;
}

void TRON_RenderTitle(SDL_Renderer* renderer)
{
    SDL_FPoint center = TRON_GetLogicalCenter();
    SDL_FRect rect = {center.x, center.y, TRON_title_text->w * TRON_TITLE_SCALE, TRON_title_text->h * TRON_TITLE_SCALE};
    TRON_CenterRect(&rect);
    rect.y -= rect.h * 2;
    Uint8 r = (Uint8)((SDL_sinf(SDL_GetTicks() * 0.001f) * 0.5f + 0.5f) * 255);
    Uint8 g = (Uint8)((SDL_sinf(SDL_GetTicks() * 0.001f + 2.0f) * 0.5f + 0.5f) * 255);
    Uint8 b = (Uint8)((SDL_sinf(SDL_GetTicks() * 0.001f + 4.0f) * 0.5f + 0.5f) * 255);
    SDL_SetTextureColorMod(TRON_title_text, r, g, b);
    SDL_RenderTexture(renderer, TRON_title_text, NULL, &rect);
}

void TRON_RenderPlayerChoice(TRON_AppState* app)
{
    SDL_FPoint center = TRON_GetLogicalCenter();
    for (int i = 0; i < TRON_MAX_BIKES; i++)
    {
        SDL_FRect rect = {center.x, center.y, TRON_player_texts[i]->w * TRON_PLAYER_CHOICE_SCALE, TRON_player_texts[i]->h * TRON_PLAYER_CHOICE_SCALE};
        TRON_CenterRect(&rect);
        rect.y += rect.h * 2 * (i + 1);

        if (app->player_choice == i)
        {
            SDL_SetTextureColorMod(TRON_player_texts[i], 255, 50, 50);
        }
        else
        {
            SDL_SetTextureColorMod(TRON_player_texts[i], 255, 255, 255);
        }

        SDL_RenderTexture(app->renderer, TRON_player_texts[i], NULL, &rect);
    }
}

void TRON_RenderMenu(TRON_AppState* app)
{
    if (!app->hide_menu)
    {
        TRON_RenderTitle(app->renderer);
        TRON_RenderPlayerChoice(app);
    }
}

void TRON_RenderGame(TRON_AppState* app)
{
    TRON_MoveBikes(app->bikes, app->num_bikes);
    TRON_CheckBikesCollisions(app->bikes, app->num_bikes);

    TRON_RenderBikes(app->renderer, app->bikes, app->num_bikes);
}

void TRON_ResetGame(TRON_AppState* app)
{
    TRON_DestroyBikes(app->bikes);
    app->num_bikes = TRON_MAX_BIKES;
    app->bikes = TRON_CreateBikes(app->renderer);
    app->player_choice = 0;
    app->game_started = false;
    app->game_ended = false;
}

void TRON_DeathCallback(void* userdata)
{
    TRON_ResetGame(userdata);
}

void TRON_RenderDeathScreen(TRON_AppState* app)
{
    if (!app->game_ended)
    {
        for (int i = 0; i < app->num_bikes; i++)
        {
            if (!app->bikes[i].dead)
            {
                ANI_ClearAnimations();
                ANI_PlayAnimationWithCallback(TRON_death_text_animation, TRON_win_texts[i], TRON_GetLogicalCenter(), SDL_GetTicks(), TRON_DeathCallback, app);
                app->game_ended = true;
            }
        }
        if (!app->game_ended)
        {
            ANI_ClearAnimations();
            ANI_PlayAnimationWithCallback(TRON_death_text_animation, TRON_draw_text, TRON_GetLogicalCenter(), SDL_GetTicks(), TRON_DeathCallback, app);
            app->game_ended = true;
        }
    }
}

SDL_AppResult SDL_AppInit(void** userdata, int argc, char* argv[])
{
    TRON_AppState* app = SDL_calloc(1, sizeof(TRON_AppState));
    SDL_Init(SDL_INIT_VIDEO);
    SDL_CreateWindowAndRenderer("light bike", 960, 540, SDL_WINDOW_RESIZABLE, &app->window, &app->renderer);
    SDL_SetRenderLogicalPresentation(app->renderer, TRON_LOGICAL_WIDTH, TRON_LOGICAL_HEIGHT, SDL_LOGICAL_PRESENTATION_LETTERBOX);
    SDL_SetHint(SDL_HINT_MAIN_CALLBACK_RATE, "60");

    SDL_memcpy(app->keys, TRON_DEFAULT_KEYS, sizeof(TRON_DEFAULT_KEYS));
    app->num_bikes = TRON_MAX_BIKES;
    app->bikes = TRON_CreateBikes(app->renderer);
    app->game_started = false;
    app->game_ended = false;
    app->player_choice = 0;

    TRON_CreateTexts(app->renderer);
    TRON_death_text_animation = ANI_LoadAnimationFromConstMem(TRON_DEATH_TEXT_XML, SDL_strlen(TRON_DEATH_TEXT_XML));
    TRON_start_animation = ANI_LoadAnimationFromConstMem(TRON_START_XML, SDL_strlen(TRON_START_XML));

    *userdata = app;
    return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppEvent(void* userdata, SDL_Event* event)
{
    TRON_AppState* app = userdata;

    if (event->type == SDL_EVENT_QUIT) { return SDL_APP_SUCCESS; }
    
    if ((event->type == SDL_EVENT_KEY_DOWN) && (!event->key.repeat))
    {
        if (event->key.scancode == SDL_SCANCODE_F11)
        {
            SDL_SetWindowFullscreen(app->window, !(SDL_GetWindowFlags(app->window) & SDL_WINDOW_FULLSCREEN));
        }
        if (!app->game_started)
        {
            if (!TRON_MenuKeyDown(app, event)) { return SDL_APP_SUCCESS; }
        }
        else if (TRON_CountAliveBikes(app->bikes, app->num_bikes) == 1)
        {
        }
        else
        {
            TRON_GameplayKeyDown(app, event);
        }
    }

    return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppIterate(void* userdata)
{
    TRON_AppState* app = userdata;

    SDL_SetRenderDrawColor(app->renderer, 0, 0, 0, 255);
    SDL_RenderClear(app->renderer);
    SDL_SetRenderDrawColor(app->renderer, 100, 100, 100, 255);
    SDL_RenderFillRect(app->renderer, NULL);
    ANI_RenderAnimations(app->renderer, SDL_GetTicks());

    if (!app->game_started)
    {
        TRON_RenderMenu(app);
    }
    else if (TRON_CountAliveBikes(app->bikes, app->num_bikes) <= 1)
    {
        TRON_RenderDeathScreen(app);
    }
    else
    {
        TRON_RenderGame(app);
    }
    SDL_RenderPresent(app->renderer);

    return SDL_APP_CONTINUE;
}

void SDL_AppQuit(void* userdata, SDL_AppResult result)
{
    TRON_AppState* app = userdata;

    TRON_DestroyTexts();
    ANI_ClearAnimations();
    ANI_DestroyAnimation(TRON_death_text_animation);
    ANI_DestroyAnimation(TRON_start_animation);
    TRON_DestroyBikes(app->bikes);
    SDL_free(app);
}
