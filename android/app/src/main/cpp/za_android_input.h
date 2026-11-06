#ifndef ZA_ANDROID_INPUT_H
#define ZA_ANDROID_INPUT_H
#include <stdbool.h>
#include <SDL3/SDL.h>
#ifdef __cplusplus
extern "C" {
#endif
#define ZA_INPUT_MAX_FINGERS 16
typedef enum { ZA_INPUT_MOVE=1, ZA_INPUT_CAMERA, ZA_INPUT_ACTION_PRIMARY, ZA_INPUT_ACTION_SECONDARY } ZAInputType;
typedef enum { ZA_INPUT_PHASE_UPDATE=0, ZA_INPUT_PHASE_PRESSED, ZA_INPUT_PHASE_RELEASED } ZAInputPhase;
typedef struct { ZAInputType type; ZAInputPhase phase; SDL_FingerID finger_id; float x; float y; } ZAInputEvent;
typedef void (*ZAInputCallback)(const ZAInputEvent *event, void *userdata);
typedef struct { float move_zone_max_x; float camera_zone_min_x; float action_zone_min_y; float move_radius; float camera_sensitivity; } ZAInputConfig;
typedef struct { SDL_FingerID id; float start_x; float start_y; float last_x; float last_y; int zone; bool active; } ZAInputFinger;
typedef struct { ZAInputConfig config; ZAInputFinger fingers[ZA_INPUT_MAX_FINGERS]; ZAInputCallback callback; void *userdata; } ZAAndroidInput;
void za_android_input_init(ZAAndroidInput *, const ZAInputConfig *, ZAInputCallback, void *);
bool za_android_input_handle_event(ZAAndroidInput *, const SDL_Event *);
void za_android_input_reset(ZAAndroidInput *);
#ifdef __cplusplus
}
#endif
#endif
