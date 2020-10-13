#ifndef RTDIAL_H_
#define RTDIAL_H_


#ifdef __cplusplus
extern "C" {
#endif

bool rtdial_init(GMainContext *context);
void rtdial_term();
typedef void (*rtdial_activation_cb)(bool);
void rtdail_register_activation_cb(rtdial_activation_cb cb);

#ifdef __cplusplus
}
#endif

#endif
