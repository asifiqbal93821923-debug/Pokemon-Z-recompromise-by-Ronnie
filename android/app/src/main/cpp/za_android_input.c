#include "za_android_input.h"
#include <stddef.h>
#include <string.h>

enum {
    ZONE_NONE = 0,
    ZONE_MOVE,
    ZONE_CAMERA,
    ZONE_ACTION_PRIMARY,
    ZONE_ACTION_SECONDARY
};

static void emit(ZAAndroidInput *input, ZAInputType type, ZAInputPhase phase,
                 SDL_FingerID id, float x, float y)
{
    if (input->callback == NULL) {
        return;
    }
    const ZAInputEvent event = { type, phase, id, x, y };
    input->callback(&event, input->userdata);
}

static ZAInputFinger *find_finger(ZAAndroidInput *input, SDL_FingerID id)
{
    for (size_t i = 0; i < ZA_INPUT_MAX_FINGERS; ++i) {
        if (input->fingers[i].active && input->fingers[i].id == id) {
            return &input->fingers[i];
        }
    }
    return NULL;
}

static ZAInputFinger *allocate_finger(ZAAndroidInput *input)
{
    for (size_t i = 0; i < ZA_INPUT_MAX_FINGERS; ++i) {
        if (!input->fingers[i].active) {
            return &input->fingers[i];
        }
    }
    return NULL;
}

void za_android_input_init(ZAAndroidInput *input, const ZAInputConfig *config,
                           ZAInputCallback callback, void *userdata)
{
    if (input == NULL) {
        return;
    }
    memset(input, 0, sizeof(*input));
    input->config = (ZAInputConfig) {
        .move_zone_max_x = 0.42f,
        .camera_zone_min_x = 0.55f,
        .action_zone_min_y = 0.78f,
        .move_radius = 0.16f,
        .camera_sensitivity = 2.0f
    };
    if (config != NULL) {
        input->config = *config;
    }
    input->callback = callback;
    input->userdata = userdata;
}

static int classify_zone(const ZAAndroidInput *input, float x, float y)
{
    if (x < input->config.move_zone_max_x) {
        return ZONE_MOVE;
    }
    if (x >= input->config.camera_zone_min_x && y < input->config.action_zone_min_y) {
        return ZONE_CAMERA;
    }
    if (x >= 0.78f && y >= input->config.action_zone_min_y && x < 0.90f) {
        return ZONE_ACTION_PRIMARY;
    }
    if (x >= 0.90f && y >= input->config.action_zone_min_y) {
        return ZONE_ACTION_SECONDARY;
    }
    return ZONE_NONE;
}

static void handle_down(ZAAndroidInput *input, const SDL_TouchFingerEvent *touch)
{
    ZAInputFinger *finger = allocate_finger(input);
    if (finger == NULL) {
        return;
    }

    *finger = (ZAInputFinger) {
        .id = touch->fingerID,
        .start_x = touch->x,
        .start_y = touch->y,
        .last_x = touch->x,
        .last_y = touch->y,
        .zone = classify_zone(input, touch->x, touch->y),
        .active = true
    };

    if (finger->zone == ZONE_ACTION_PRIMARY) {
        emit(input, ZA_INPUT_ACTION_PRIMARY, ZA_INPUT_PHASE_PRESSED, finger->id, 1.0f, 0.0f);
    } else if (finger->zone == ZONE_ACTION_SECONDARY) {
        emit(input, ZA_INPUT_ACTION_SECONDARY, ZA_INPUT_PHASE_PRESSED, finger->id, 1.0f, 0.0f);
    } else if (finger->zone == ZONE_MOVE) {
        emit(input, ZA_INPUT_MOVE, ZA_INPUT_PHASE_UPDATE, finger->id, 0.0f, 0.0f);
    } else if (finger->zone == ZONE_CAMERA) {
        emit(input, ZA_INPUT_CAMERA, ZA_INPUT_PHASE_UPDATE, finger->id, 0.0f, 0.0f);
    }
}

static float clamp_unit(float value)
{
    if (value < -1.0f) {
        return -1.0f;
    }
    if (value > 1.0f) {
        return 1.0f;
    }
    return value;
}

static void handle_motion(ZAAndroidInput *input, const SDL_TouchFingerEvent *touch)
{
    ZAInputFinger *finger = find_finger(input, touch->fingerID);
    if (finger == NULL) {
        return;
    }

    if (finger->zone == ZONE_MOVE) {
        const float radius = input->config.move_radius > 0.001f
                                 ? input->config.move_radius : 0.16f;
        const float dx = (touch->x - finger->start_x) / radius;
        const float dy = (touch->y - finger->start_y) / radius;
        emit(input, ZA_INPUT_MOVE, ZA_INPUT_PHASE_UPDATE, finger->id,
             clamp_unit(dx), clamp_unit(dy));
    } else if (finger->zone == ZONE_CAMERA) {
        emit(input, ZA_INPUT_CAMERA, ZA_INPUT_PHASE_UPDATE, finger->id,
             (touch->x - finger->last_x) * input->config.camera_sensitivity,
             (touch->y - finger->last_y) * input->config.camera_sensitivity);
    }

    finger->last_x = touch->x;
    finger->last_y = touch->y;
}

static void handle_up(ZAAndroidInput *input, const SDL_TouchFingerEvent *touch)
{
    ZAInputFinger *finger = find_finger(input, touch->fingerID);
    if (finger == NULL) {
        return;
    }

    if (finger->zone == ZONE_MOVE) {
        emit(input, ZA_INPUT_MOVE, ZA_INPUT_PHASE_UPDATE, finger->id, 0.0f, 0.0f);
    } else if (finger->zone == ZONE_CAMERA) {
        emit(input, ZA_INPUT_CAMERA, ZA_INPUT_PHASE_UPDATE, finger->id, 0.0f, 0.0f);
    } else if (finger->zone == ZONE_ACTION_PRIMARY) {
        emit(input, ZA_INPUT_ACTION_PRIMARY, ZA_INPUT_PHASE_RELEASED, finger->id, 0.0f, 0.0f);
    } else if (finger->zone == ZONE_ACTION_SECONDARY) {
        emit(input, ZA_INPUT_ACTION_SECONDARY, ZA_INPUT_PHASE_RELEASED, finger->id, 0.0f, 0.0f);
    }

    finger->active = false;
}

bool za_android_input_handle_event(ZAAndroidInput *input, const SDL_Event *event)
{
    if (input == NULL || event == NULL) {
        return false;
    }
    switch (event->type) {
    case SDL_EVENT_FINGER_DOWN:
        handle_down(input, &event->tfinger);
        return true;
    case SDL_EVENT_FINGER_MOTION:
        handle_motion(input, &event->tfinger);
        return true;
    case SDL_EVENT_FINGER_UP:
        handle_up(input, &event->tfinger);
        return true;
    default:
        return false;
    }
}

void za_android_input_reset(ZAAndroidInput *input)
{
    if (input == NULL) {
        return;
    }
    for (size_t i = 0; i < ZA_INPUT_MAX_FINGERS; ++i) {
        if (input->fingers[i].active) {
            SDL_TouchFingerEvent touch = { .fingerID = input->fingers[i].id };
            handle_up(input, &touch);
        }
    }
}
