#include "za_android_input.h"
#include <stddef.h>
#include <string.h>
enum { ZONE_NONE=0, ZONE_MOVE, ZONE_CAMERA, ZONE_ACTION_PRIMARY, ZONE_ACTION_SECONDARY };
static void emit(ZAAndroidInput *in, ZAInputType type, ZAInputPhase phase, SDL_FingerID id, float x, float y) {
 if (!in->callback) return; const ZAInputEvent e={type,phase,id,x,y}; in->callback(&e,in->userdata);
}
static ZAInputFinger *find(ZAAndroidInput *in, SDL_FingerID id) {
 for(size_t i=0;i<ZA_INPUT_MAX_FINGERS;i++) if(in->fingers[i].active && in->fingers[i].id==id) return &in->fingers[i]; return NULL;
}
static ZAInputFinger *alloc_finger(ZAAndroidInput *in) {
 for(size_t i=0;i<ZA_INPUT_MAX_FINGERS;i++) if(!in->fingers[i].active) return &in->fingers[i]; return NULL;
}
void za_android_input_init(ZAAndroidInput *in,const ZAInputConfig *cfg,ZAInputCallback cb,void *ud) {
 if(!in) return; memset(in,0,sizeof(*in));
 in->config=(ZAInputConfig){.move_zone_max_x=.42f,.camera_zone_min_x=.55f,.action_zone_min_y=.78f,.move_radius=.16f,.camera_sensitivity=2.0f};
 if(cfg) in->config=*cfg; in->callback=cb; in->userdata=ud;
}
static int zone_for(const ZAAndroidInput *in,float x,float y) {
 if(x<in->config.move_zone_max_x) return ZONE_MOVE;
 if(x>=in->config.camera_zone_min_x && y<in->config.action_zone_min_y) return ZONE_CAMERA;
 if(x>=.78f && y>=in->config.action_zone_min_y && x<.90f) return ZONE_ACTION_PRIMARY;
 if(x>=.90f && y>=in->config.action_zone_min_y) return ZONE_ACTION_SECONDARY;
 return ZONE_NONE;
}
static void down(ZAAndroidInput *in,const SDL_TouchFingerEvent *t) {
 ZAInputFinger *f=alloc_finger(in); if(!f) return;
 *f=(ZAInputFinger){.id=t->fingerID,.start_x=t->x,.start_y=t->y,.last_x=t->x,.last_y=t->y,.zone=zone_for(in,t->x,t->y),.active=true};
 if(f->zone==ZONE_ACTION_PRIMARY) emit(in,ZA_INPUT_ACTION_PRIMARY,ZA_INPUT_PHASE_PRESSED,f->id,1,0);
 else if(f->zone==ZONE_ACTION_SECONDARY) emit(in,ZA_INPUT_ACTION_SECONDARY,ZA_INPUT_PHASE_PRESSED,f->id,1,0);
 else if(f->zone==ZONE_MOVE) emit(in,ZA_INPUT_MOVE,ZA_INPUT_PHASE_UPDATE,f->id,0,0);
 else if(f->zone==ZONE_CAMERA) emit(in,ZA_INPUT_CAMERA,ZA_INPUT_PHASE_UPDATE,f->id,0,0);
}
static float clamp1(float v) { return v < -1 ? -1 : (v > 1 ? 1 : v); }
static void motion(ZAAndroidInput *in,const SDL_TouchFingerEvent *t) {
 ZAInputFinger *f=find(in,t->fingerID); if(!f) return;
 if(f->zone==ZONE_MOVE) { float r=in->config.move_radius>.001f?in->config.move_radius:.16f; emit(in,ZA_INPUT_MOVE,ZA_INPUT_PHASE_UPDATE,f->id,clamp1((t->x-f->start_x)/r),clamp1((t->y-f->start_y)/r)); }
 else if(f->zone==ZONE_CAMERA) emit(in,ZA_INPUT_CAMERA,ZA_INPUT_PHASE_UPDATE,f->id,(t->x-f->last_x)*in->config.camera_sensitivity,(t->y-f->last_y)*in->config.camera_sensitivity);
 f->last_x=t->x; f->last_y=t->y;
}
static void up(ZAAndroidInput *in,const SDL_TouchFingerEvent *t) {
 ZAInputFinger *f=find(in,t->fingerID); if(!f) return;
 if(f->zone==ZONE_MOVE) emit(in,ZA_INPUT_MOVE,ZA_INPUT_PHASE_UPDATE,f->id,0,0);
 else if(f->zone==ZONE_CAMERA) emit(in,ZA_INPUT_CAMERA,ZA_INPUT_PHASE_UPDATE,f->id,0,0);
 else if(f->zone==ZONE_ACTION_PRIMARY) emit(in,ZA_INPUT_ACTION_PRIMARY,ZA_INPUT_PHASE_RELEASED,f->id,0,0);
 else if(f->zone==ZONE_ACTION_SECONDARY) emit(in,ZA_INPUT_ACTION_SECONDARY,ZA_INPUT_PHASE_RELEASED,f->id,0,0);
 f->active=false;
}
bool za_android_input_handle_event(ZAAndroidInput *in,const SDL_Event *e) {
 if(!in||!e) return false;
 switch(e->type) { case SDL_EVENT_FINGER_DOWN: down(in,&e->tfinger); return true; case SDL_EVENT_FINGER_MOTION: motion(in,&e->tfinger); return true; case SDL_EVENT_FINGER_UP: up(in,&e->tfinger); return true; default: return false; }
}
void za_android_input_reset(ZAAndroidInput *in) {
 if(!in) return;
 for(size_t i=0;i<ZA_INPUT_MAX_FINGERS;i++) if(in->fingers[i].active) { SDL_TouchFingerEvent t={.fingerID=in->fingers[i].id}; up(in,&t); }
}
